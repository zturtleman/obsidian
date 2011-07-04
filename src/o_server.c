/*  Obsidian
    Copyright (C) 2011 by tm512
    
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:
    
    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.
    
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
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

#include "o_server.h"
#include "o_common.h"

boolean server;
boolean client;

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
			damages[i] = 0;
		}
	}

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
				clients[c].firstspawn = 0;
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

	for(i = 0; i < MAXPLAYERS; i++)
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
	int secbuflen = ReadInt32((int32_t**)&secbuf);
	ENetPacket *pk = enet_packet_create(NULL, 32 + MAX_MOBJ_BUFFER + secbuflen, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;
	void *mobjbuf = P_MakeMobjBuffer();
	uint8_t inGame = 0;
	uint8_t i;

	playeringame[cl->id] = true;
	cl->player->playerstate = PST_REBORN;
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

	memcpy(p, mobjbuf, MAX_MOBJ_BUFFER); // Bit masks for which items have been removed
	p += MAX_MOBJ_BUFFER;
	free(mobjbuf);

	memcpy(p, secbuf, secbuflen); // Sector state information
	p += secbuflen;
	free(secbuf - 4);

	enet_packet_resize(pk, (uint8_t*)p - (uint8_t*)pk->data);
	enet_peer_send(cl->peer, 0, pk);
	cl->type = CT_ACTIVE;

	return;
}

void SV_DropClient(int cn, const char *reason) // Reset one of the client_t inside clients[]
{
	mobj_t *mo;

	if(!clients[cn].type) // Client is already empty
		return;

	playeringame[cn] = false;
	clients[cn].type = CT_EMPTY;
	mo = clients[cn].player->mo;
	P_RemoveMobj(mo);
	mo->player = NULL;
	clients[cn].player->mo = NULL;
	clients[cn].player = NULL;
	printf("disconnected client %i (%s)\n", cn, reason);

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

		case MSG_STATE:
		if(clients[from].player->mo && clients[from].player->mo && clients[from].player->mo->health > 0)
		{
			P_SetMobjState(clients[from].player->mo, (statenum_t)ReadUInt16((uint16_t**)&pkp));
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
		if(clients[from].player && clients[from].firstspawn)
		{
			int dmstart;

			if(clients[from].player->playerstate != PST_REBORN)
				clients[from].player->playerstate = PST_REBORN;

			if(deathmatch)
			{
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
		else
			clients[from].firstspawn = 1; // Hack! Ew! Gross!
		break;

		case MSG_CHAT:
			printf("%i > %s\n", from, pkp);
			resend = 1;
			break;

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
		if(damages[i] && clients[i].player)
		{
			ENetPacket *dmg = enet_packet_create(NULL, 6, ENET_PACKET_FLAG_RELIABLE);
			void *p = dmg->data;

			WriteUInt8((uint8_t**)&p, MSG_DAMAGE);
			WriteUInt8((uint8_t**)&p, i);
			WriteInt32((int32_t**)&p, damages[i]);
			SV_BroadcastPacket(dmg, -1);

			damages[i] = 0;
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

void SV_DamageMobj(mobj_t *target, int damage)
{
	int i;

	// Loop through all players and see if our target and source have players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!clients[i].player || !clients[i].player->mo)
			continue;

		if(clients[i].player->mo == target)
			damages[i] += damage;
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
		if(damages[t]) // Send unsent damage
			SV_SendDamage();

		WriteUInt8((uint8_t**)&p, MSG_KILL);
		WriteUInt8((uint8_t**)&p, t);
		WriteUInt8((uint8_t**)&p, s);
		SV_BroadcastPacket(pk, -1);
	}
}

int SV_ClientNumForPeer(ENetPeer *p)
{
	int i;
	if(!p) return -1;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if(clients[i].peer == p)
			return i;
	}
	return -1;
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

	// allocate j, plus one for a 8-bit signed integer containing -1, which specifies the end of the buffer.
	// Also allocate 4 bytes for putting j into the buffer, so we know how big the buffer is.
	secbuf = malloc(j + 5);
	p = secbuf; // Do not modify secbuf.
	printf ("SV_MakeSectorBuffer: Allocated %i bytes\n", j+1);
	WriteInt32((int32_t**)&p, j+1);

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

			WriteInt32((int32_t**)&p, sectors[i].floorheight);
			WriteInt32((int32_t**)&p, sectors[i].ceilingheight);
		}

	WriteInt8((int8_t**)&p, -1); // End of buffer

	return secbuf;
}
