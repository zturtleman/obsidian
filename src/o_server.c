/*  Obsidian
    Copyright (C) 2011 by tm512, All Rights Reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Redistributions in any form must be accompanied by information on
       how to obtain complete source code for the software and any
       accompanying software that uses the software.  The source code
       must either be included in the distribution or be available for no
       more than the cost of distribution plus a nominal fee, and must be
       freely redistributable under reasonable conditions.  For an
       executable file, complete source code means the source code for all
       modules it contains.  It does not include source code for modules or
       files that typically accompany the major components of the operating
       system on which the executable file runs.

    THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
    NON-INFRINGEMENT, ARE DISCLAIMED.  IN NO EVENT SHALL
    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
    THE POSSIBILITY OF SUCH DAMAGE. 
    ---
    o_server : Obsidian Server
*/

// GhostlyDeath <March 27, 2011> -- Lean and mean for Windows (since ENet does not use it)
#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include "enet/enet.h"

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "d_event.h"
#include "i_system.h"
#include "r_defs.h"
#include "r_main.h"
#include "p_local.h"
#include "p_pspr.h"
#include "p_spec.h"
#include "m_argv.h"
#include "m_config.h"

#include "o_server.h"
#include "o_common.h"

boolean server;
boolean client;

char *sv_hostname;
int sv_fraglimit;
int sv_timelimit;
int sv_skill;
int sv_maxplayers;

int SV_Main (void) 
{
	int i, j, attempts;

	if (enet_initialize() != 0) 
		return 1; // Initialize enet, if it fails, return 1

	atexit(enet_deinitialize);

	j = M_CheckParmWithArgs("-port", 1);

	addr.host = ENET_HOST_ANY;
	if(j > 0)
		addr.port = atoi(myargv[j+1]);
	else
		addr.port = 11666;

	// Attempt to bind the server to this address and port, if it fails, increment the port and try again.
	srv = enet_host_create(&addr, MAXPLAYERS, MAXPLAYERS * 2, 0, 0);
	attempts = 0;

	while(srv == NULL && attempts < 100)
	{
		addr.port ++;
		attempts ++;

		srv = enet_host_create(&addr, MAXPLAYERS, MAXPLAYERS * 2, 0, 0);
	}

	// If it still isn't working, error out.
	if(srv == NULL)
		I_Error("Failed to initialize server.\nIf for some reason there aren't any open ports in this range, use -port to try another.");
	else
	{
		printf("Obsidian Dedicated Server started on%s port %i\n", addr.port == 11666 ? "" : " alternate", addr.port);
		autostart = 1;
		server = 1;
		client = 0;

		for(i = 0; i < MAXPLAYERS; i++)
		{
			clients[i].type = CT_EMPTY;
			clients[i].damage = 0;
		}
	}

	M_LoadServerDefaults();
	startskill = sv_skill;
	timelimit = sv_timelimit;

	return 0;
}

void SV_Loop (void)
{
	ENetEvent event;
	int from, c;
	char hn[512];

	while (enet_host_service(srv, &event, 5) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
			{
				c = SV_FindEmptyClientNum(); // Find an empty client for this guy, or kick him out
				if (c < 0) 
				{ 
					enet_peer_reset(event.peer); 
					break;
				}
				clients[c].type = CT_CONNECT;
				clients[c].id = c;
				clients[c].peer = event.peer;
				clients[c].player = &players[c];
				if(!clients[c].player) 
				{
					enet_peer_reset(clients[c].peer);
					break;
				}
				
				printf("connecting: %s\n", (enet_address_get_host(&clients[c].peer->address, hn, sizeof(hn))==0) ? hn : "localhost");
				SV_ClientWelcome(&clients[c]);
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:
			{
				SV_ParsePacket(event.packet, event.peer);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				from = SV_ClientNumForPeer(event.peer);
				switch(event.data)
				{
					case 0:
						SV_DropClient(from, "timed out");
						break;
					case 1:
						SV_DropClient(from, "client exited");
						break;
				}
			}
		}
	}

	SV_SendDamage();

	return;
}

int SV_FindEmptyClientNum(void)
{
	int i;

	for(i = 0; i < sv_maxplayers; i++)
		if(clients[i].type == CT_EMPTY)
			return i;

	return -1;
}

int d_map, d_episode;
skill_t d_skill;
int prndindex;
uint8_t *P_MakeMobjBuffer (void);

void SV_ClientWelcome (client_t* cl)
{
	void *secbuf = SV_MakeSectorBuffer();
	int secbuflen = ((int*)secbuf)[0];
	ENetPacket *pk = enet_packet_create(NULL,
		32 + (MAXPLAYERS * MAXPLAYERS) + MAX_MOBJ_BUFFER + secbuflen, 
		ENET_PACKET_FLAG_RELIABLE);

	void *p = pk->data;
	void *mobjbuf = P_MakeMobjBuffer();
	uint8_t inGame = 0;
	uint8_t i, j;

	cl->player->playerstate = PST_DEAD;
	for (i = 0; i < MAXPLAYERS; i++)
		if(playeringame[i])
			inGame |= 1 << i;

	WriteUInt8((uint8_t**)&p, MSG_WELCOME); // put a greeting marker on it
	WriteUInt8((uint8_t**)&p, (uint8_t)OBS_PROTO); // Protocol version
	WriteInt32((int32_t**)&p, prndindex); // Random Index
	WriteUInt8((uint8_t**)&p, (uint8_t)gameepisode); // Game episode... for doom(1).wad
	WriteUInt8((uint8_t**)&p, (uint8_t)gamemap); // Game map
	WriteUInt8((uint8_t**)&p, (uint8_t)gameskill); // Game skill
	WriteUInt8((uint8_t**)&p, (uint8_t)deathmatch); // Game mode
	WriteUInt8((uint8_t**)&p, cl->id); // client will set this to consoleplayer
	WriteUInt8((uint8_t**)&p, inGame); // Bit mask for which players are in game

	for (i = 0; i < MAXPLAYERS; i++)
		for (j = 0; j < MAXPLAYERS; j++)
			WriteInt8((int8_t**)&p, players[i].frags[j]);

	memcpy(p, mobjbuf, MAX_MOBJ_BUFFER); // Bit masks for which items have been removed
	p += MAX_MOBJ_BUFFER;
	free(mobjbuf);

	memcpy(p, secbuf, secbuflen); // Sector state information
	p += secbuflen;
	free(secbuf);

	enet_packet_resize(pk, (uint8_t*)p - (uint8_t*)pk->data);
	enet_peer_send(cl->peer, 0, pk);
	cl->type = CT_ACTIVE;

	return;
}

void SV_DropClient(int cn, const char *reason) // Reset one of the client_t inside clients[]
{
	mobj_t *mo;
	char discmsg[128];
	ENetPacket *pk = enet_packet_create(NULL, 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	if(!clients[cn].type) // Client is already empty
		return;

	printf("%s disconnected (%s)\n", clients[cn].nick, reason);
	snprintf(discmsg, 128, "%s disconnected.", clients[cn].nick);

	if(strlen(clients[cn].nick) > 0)
		SV_SendString(MSG_CHAT, discmsg, cn, 0);

	playeringame[cn] = false;
	clients[cn].type = CT_EMPTY;
	mo = clients[cn].player->mo;

	if (!mo)
		return;

	P_RemoveMobj(mo);
	mo->player = NULL;
	clients[cn].player->mo = NULL;
	memset(&players[cn], 0, sizeof(player_t));
	memset(clients[cn].nick, 0, MAXPLAYERNAME);
	clients[cn].player->playerstate = PST_DEAD;
	clients[cn].player = NULL;

	WriteUInt8((uint8_t**)&p, MSG_DISC);
	WriteUInt8((uint8_t**)&p, cn);
	SV_BroadcastPacket(pk, -1);

	return;
}

void P_FireWeapon (player_t* player);

void SV_ParsePacket (ENetPacket *pk, ENetPeer *p)
{
	int from = SV_ClientNumForPeer(p);
	void *pkp = pk->data;
	uint8_t msg;
	mobj_t *mo;
	boolean resend;

	if(from < 0) return; // Not a client
	msg = ReadUInt8((uint8_t**)&pkp);
	resend = 0;

	if (msg != MSG_NICK && !strlen(clients[from].nick))
	{
		SV_DropClient(from, "No nickname");
		return;
	}

	switch(msg)
	{
		case MSG_POS:
		if(clients[from].player && clients[from].player->mo && clients[from].player->mo->health > 0)
		{
			mo = clients[from].player->mo;
			P_UnsetThingPosition(mo);
			mo->x = ReadInt32((int32_t**)&pkp);
			mo->y = ReadInt32((int32_t**)&pkp);
			mo->z = ReadInt32((int32_t**)&pkp);
			mo->angle = ReadInt32((int32_t**)&pkp);
			mo->momx = ReadInt32((int32_t**)&pkp);
			mo->momy = ReadInt32((int32_t**)&pkp);
			mo->momz = ReadInt32((int32_t**)&pkp);
			mo->subsector = R_PointInSubsector(mo->x, mo->y);
			mo->floorz = mo->subsector->sector->floorheight;
			mo->ceilingz = mo->subsector->sector->ceilingheight;
			P_SetThingPosition(mo);
			resend = 1;
		}
		break;

		case MSG_USE:
		if(clients[from].player && clients[from].player->mo && clients[from].player->mo->health > 0)
		{
			P_UseLines(clients[from].player);
			resend = 1;
		}
		break;

		case MSG_FIRE:
		if(clients[from].player && clients[from].player->mo && clients[from].player->mo->health > 0)
		{
			weapontype_t toFire = (weapontype_t)ReadInt8((uint8_t**)&pkp);
			if(toFire != clients[from].player->readyweapon)
				clients[from].player->readyweapon = toFire;
			clients[from].player->refire = ReadInt16((int16_t**)&pkp);
			P_FireWeapon(clients[from].player);
			resend = 1;
		}
		break;

		case MSG_RESPAWN:
		if(clients[from].player)
		{
			int dmstart;

			if(clients[from].player->playerstate == PST_LIVE) // No respawning a live player
				break;

			if(!playeringame[from])
			{
				playeringame[from] = true;
				if(clients[from].player->mo)
				{
					clients[from].player->mo->player = NULL;
					clients[from].player->mo = NULL;
				}
			}

			clients[from].player->playerstate = PST_REBORN;

			if(deathmatch)
			{
				if(clients[from].player->mo)
					clients[from].player->mo->player = NULL;

				dmstart = ReadInt8((int8_t**)&pkp);

				if(dmstart < 0) // Uh oh, client either bugging out or trying to mess us up, kick their ass.
				{
					SV_DropClient(from, "Invalid player start");
					break;
				} 

				deathmatchstarts[dmstart].type = from + 1;
				G_CheckSpot(from, &deathmatchstarts[dmstart]);
				P_SpawnPlayer(&deathmatchstarts[dmstart]);
			}
			resend = 1;
		}
		break;

		case MSG_CHAT:
		{
			char sendchat[130 + MAXPLAYERNAME]; // 128 for original message, 2 for ": ", and MAXPLAYERNAME to hold the player's name.
			printf("<%s> %s\n", clients[from].nick, pkp);
			snprintf(sendchat, 130 + MAXPLAYERNAME, "%s: %s", clients[from].nick, pkp);
			SV_SendString(MSG_CHAT, sendchat, from, 1);
			resend = 0;
			break;
		}

		case MSG_NICK:
		{
			char joinmsg[128];
			strncpy(clients[from].nick, pkp, MAXPLAYERNAME);
			printf("Client %i sets nickname to \"%s\"\n", from, clients[from].nick);
			snprintf(joinmsg, 128, "%s has entered the game!", clients[from].nick);
			SV_SendString(MSG_CHAT, joinmsg, -1, 0);
			resend = 0;
			break;
		}

		default:
			resend = 0;
			break;
	}
	
	if(resend)
		SV_ResizeBroadcastPacket(pk, from, msg);

	return;
}

void SV_ResizeBroadcastPacket(ENetPacket *pk, int from, uint8_t msg)
{
	int len = pk->dataLength;
	void *pkp;

	if(enet_packet_resize(pk, pk->dataLength + 1) < 0) // Failure! :O
		return;

	pkp = pk->data;
	pkp = ((uint8_t*)pkp) + len;

	WriteUInt8((uint8_t**)&pkp, (uint8_t)from);

	SV_BroadcastPacket(pk, from);

	return;
}

void SV_BroadcastPacket(ENetPacket *pk, int exclude)
{
	// Use channels 0 - 3 for unreliable, 4 - 7 for reliable. Each client gets two dedicated channels
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
		if(i != exclude && clients[i].type > 1)
			enet_peer_send(clients[i].peer, i + (((pk->flags & ENET_PACKET_FLAG_RELIABLE) > 0) * MAXPLAYERS), pk); 

	if(pk->referenceCount == 0)
		enet_packet_destroy(pk);

	return;
}

void SV_SendDamage(void)
{
	int i;

	// Send damage if needed.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(clients[i].damage && clients[i].player)
		{
			ENetPacket *dmg = enet_packet_create(NULL, 6, ENET_PACKET_FLAG_RELIABLE);
			void *p = dmg->data;

			WriteUInt8((uint8_t**)&p, MSG_DAMAGE);
			WriteUInt8((uint8_t**)&p, i);
			WriteInt32((int32_t**)&p, clients[i].damage);
			SV_BroadcastPacket(dmg, -1);

			clients[i].damage = 0;
		}
	}
}

void SV_SendPickup (player_t *player, int netid)
{
	uint8_t i, t;
	ENetPacket *pk = enet_packet_create(NULL, 6, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	t = MAXPLAYERS + 1;

	for (i = 0; i < MAXPLAYERS; i++)
		if (&players[i] == player) 
			t = i;

	if (t > MAXPLAYERS)
		return;

	WriteUInt8((uint8_t**)&p, MSG_PICKUP);
	WriteUInt8((uint8_t**)&p, t);
	WriteInt32((int**)&p, netid);

	SV_BroadcastPacket(pk, -1);

	return;
}

void SV_SendWeapon (player_t *player, weapontype_t weapon)
{
	uint8_t i, t;
	ENetPacket *pk = enet_packet_create(NULL, 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	t = MAXPLAYERS + 1;

	for (i = 0; i < MAXPLAYERS; i++)
		if (&players[i] == player)
			t = i;

	if (t > MAXPLAYERS)
		return;

	WriteUInt8((uint8_t**)&p, MSG_WPICKUP);
	WriteUInt8((uint8_t**)&p, (uint8_t)weapon);

	enet_peer_send(clients[t].peer, t + MAXPLAYERS, pk);

	return;
}

void SV_SendFire (player_t *player, weapontype_t weapon)
{
	int i, t;
	ENetPacket *pk = enet_packet_create(NULL, 10, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	t = sv_maxplayers + 1;

	for (i = 0; i < sv_maxplayers; i++)
		if (&players[i] == player)
			t = i;

	if(t > sv_maxplayers)
		return;

	WriteUInt8((uint8_t**)&p, MSG_FIRE2);
	WriteUInt8((uint8_t**)&p, (uint8_t)weapon);
	WriteInt16((int16_t**)&p, players[t].refire);
	WriteInt32((int32_t**)&p, (int32_t)clients[t].player->mo->angle);
	WriteInt16((int16_t**)&p, prndindex);

	enet_peer_send(clients[t].peer, t + MAXPLAYERS, pk);

	return;
}

void SV_DamageMobj(mobj_t *target, int damage)
{
	int i;

	// Loop through all players and see if our target and source have players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!clients[i].player || !clients[i].player->mo)
			continue;

		if(clients[i].player->mo == target)
			clients[i].damage += damage;
	}

	
}

void SV_KillMobj(mobj_t *source, mobj_t *target)
{
	uint8_t i, s, t;
	ENetPacket *pk = enet_packet_create(NULL, 3, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	s = t = MAXPLAYERS + 1;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!clients[i].player || !clients[i].player->mo)
			continue;

		if(clients[i].player->mo == target)
			t = i;

		if(clients[i].player->mo == source)
			s = i;
	}

	if(t <= MAXPLAYERS)
	{
		if(clients[t].damage) // Send unsent damage
			SV_SendDamage();

		WriteUInt8((uint8_t**)&p, MSG_KILL);
		WriteUInt8((uint8_t**)&p, t);
		WriteUInt8((uint8_t**)&p, s);
		SV_BroadcastPacket(pk, -1);
	}
}

// TODO: Make this a bit better.

gameaction_t gameaction;

void SV_CheckFrags (mobj_t *mo)
{
	int fragcount, i;

	if (!mo->player || !sv_fraglimit)
		return;

	fragcount = 0;

	for (i = 0; i < sv_maxplayers; i++)
	{
		if (clients[i].player == mo->player) // Check for suicides
			fragcount -= mo->player->frags[i];
		else
			fragcount += mo->player->frags[i];
	}

	if (fragcount == sv_fraglimit)
	{
		printf("Fraglimit hit!\n");
		gameaction = ga_completed;
	}

	return;
}

int SV_ClientNumForPeer(ENetPeer *p)
{
	int i;
	if(!p) return -1;
	for (i = 0; i < sv_maxplayers; i++)
	{
		if(clients[i].peer == p)
			return i;
	}
	return -1;
}

void SV_SendString (messagetype_e type, const char *sending, int exclude, boolean sound)
{
	ENetPacket *pk = enet_packet_create(NULL, strlen(sending) + 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, type);
	WriteUInt8((uint8_t**)&p, sound);
	strncpy(p, sending, strlen(sending) + 1);
	SV_BroadcastPacket(pk, exclude);
	return;
}

void SV_SendExit (boolean secret)
{
	ENetPacket *pk = enet_packet_create(NULL, 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_EXITLEV);
	WriteUInt8((uint8_t**)&p, secret);

	SV_BroadcastPacket(pk, -1);
	return;
}

// Loop through all sectors, if they have a special, write info about them out to a buffer.
void *SV_MakeSectorBuffer (void)
{
	int i, j; // i is the iterator, j is the memory to allocate to secbuf
	void *secbuf, *p;

	for (i = 0, j = 0; i < numsectors; i++)
		if(sectors[i].specialtype) 
		{
			// For each moving sector, allocate 13 bytes for the sector number,
			// specialtype, floorheight, ceilingheight, and if the sector has it,
			// enough room for to write enough info for specialdata.
			j += 13;

			if (!sectors[i].specialdata)
				continue;

			switch (sectors[i].specialtype)
			{
				case spt_ceiling:
				j += 20;
				break;

				case spt_door:
				j += 18;
				break;

				case spt_floor:
				j += 17;
				break;

				case spt_plat:
				j += 28;
				break;

				default:
				I_Error ("Unknown specialtype %i at sector %i\n", sectors[i].specialdata, i);
				break;
			}
		}

	// allocate j, plus 4 for an integer containing -1, which specifies the end of the buffer.
	// Also allocate 4 bytes for putting j into the buffer, so we know how big the buffer is.
	j += 8;
	secbuf = malloc(j);
	p = secbuf; // Do not modify secbuf.
	WriteInt32((int32_t**)&p, j);

	for (i = 0, j = 0; i < numsectors; i++)
		if(sectors[i].specialtype)
		{
			WriteInt32((int32_t**)&p, i); // Sector number
			WriteUInt8((uint8_t**)&p, (sectors[i].specialdata) ? sectors[i].specialtype : 0); // Sector type, 0 if it's not moving

			if(sectors[i].specialdata)
			{
				switch(sectors[i].specialtype)
				{
					case spt_ceiling:
					{
						ceiling_t* ceil = (ceiling_t*)sectors[i].specialdata;

						WriteUInt8((uint8_t**)&p, (uint8_t)ceil->type);
						WriteInt32((int32_t**)&p, (int32_t)ceil->bottomheight);
						WriteInt32((int32_t**)&p, (int32_t)ceil->topheight);
						WriteInt32((int32_t**)&p, (int32_t)ceil->speed);
						WriteUInt8((uint8_t**)&p, (uint8_t)ceil->crush);
						WriteInt8((int8_t**)&p, (int8_t)ceil->direction);
						WriteInt32((int32_t**)&p, (int32_t)ceil->tag);
						WriteInt8((int8_t**)&p, (int8_t)ceil->olddirection);
						break;
					}

					case spt_door:
					{
						vldoor_t *door = (vldoor_t*)sectors[i].specialdata;

						WriteUInt8((uint8_t**)&p, (uint8_t)door->type);
						WriteInt32((int32_t**)&p, (int32_t)door->topheight);
						WriteInt32((int32_t**)&p, (int32_t)door->speed);
						WriteInt8((int8_t**)&p, (int8_t)door->direction);
						WriteInt32((int32_t**)&p, (int32_t)door->topwait);
						WriteInt32((int32_t**)&p, (int32_t)door->topcountdown);
						break;
					}

					case spt_floor:
					{
						floormove_t *floor = (floormove_t*)sectors[i].specialdata;

						WriteUInt8((uint8_t**)&p, (uint8_t)floor->type);
						WriteUInt8((uint8_t**)&p, (uint8_t)floor->crush);
						WriteInt8((int8_t**)&p, (int8_t)floor->direction);
						WriteInt32((int32_t**)&p, (int32_t)floor->newspecial);
						WriteInt16((int16_t**)&p, (int16_t)floor->texture);
						WriteInt32((int32_t**)&p, (int32_t)floor->floordestheight);
						WriteInt32((int32_t**)&p, (int32_t)floor->speed);
						break;
					}

					case spt_plat:
					{
						plat_t *plat = (plat_t*)sectors[i].specialdata;

						WriteUInt8((uint8_t**)&p, (uint8_t)plat->type);
						WriteInt32((int32_t**)&p, (int32_t)plat->speed);
						WriteInt32((int32_t**)&p, (int32_t)plat->high);
						WriteInt32((int32_t**)&p, (int32_t)plat->low);
						WriteInt32((int32_t**)&p, (int32_t)plat->wait);
						WriteInt32((int32_t**)&p, (int32_t)plat->count);
						WriteUInt8((uint8_t**)&p, (uint8_t)plat->status);
						WriteUInt8((uint8_t**)&p, (uint8_t)plat->oldstatus);
						WriteUInt8((uint8_t**)&p, (uint8_t)plat->crush);
						WriteInt32((int32_t**)&p, (int32_t)plat->tag);
						break;
					}

					default:
					break;
				}
			}

			WriteInt32((int32_t**)&p, (int32_t)sectors[i].floorheight);
			WriteInt32((int32_t**)&p, (int32_t)sectors[i].ceilingheight);
		}

	WriteInt32((int32_t**)&p, -1); // End of buffer

	return secbuf;
}
