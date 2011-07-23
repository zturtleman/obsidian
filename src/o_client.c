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
    o_client : Obsidian Client
*/

// GhostlyDeath <March 27, 2011> -- Lean and mean for Windows (since ENet does not use it)
#define WIN32_LEAN_AND_MEAN

#include "enet/enet.h"

#include "doomstat.h"
#include "d_event.h"
#include "p_mobj.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h"
#include "p_local.h"
#include "p_spec.h"
#include "deh_main.h"
#include "p_inter.h"
#include "z_zone.h"

#include "o_client.h"
#include "o_common.h"

boolean server;
boolean client;

ENetPeer *srvpeer;

// Chat stuff, basically working around Doom's crappy message system. We need a better one.
uint8_t chatindex = 0;
char chatmsg[128];

int startmap, startepisode;
skill_t startskill;

extern uint8_t *readmobjbuf;	
extern char *readsecbuf;
uint8_t *readmobjbuf;
char *readsecbuf;
int prndindex;
char *player_name;
boolean netstat = 0;

void CL_Connect (char *srv_hn)
{
	ENetAddress addr = { ENET_HOST_ANY, 11666 };
	ENetEvent event;
	uint8_t* pkd, msgid;
	char *host, *port;

	if (enet_initialize() != 0)
		return; // Initialize enet, if it fails, return

	host = strtok(srv_hn, ":");
	port = strtok(NULL, ":");
	netstat = M_CheckParm ("-netstat");
	if (netstat)
		printf ("CL_Connect: Enabling network debug information.\n");

	if (port)
		addr.port = atoi(port);

	if (enet_address_set_host (&addr, host) < 0)
	{
		I_Error("Could not resolve address!\n");
		return;
	}

	printf("Attempting to connect to %s:%i\n", host, addr.port);

	localclient = enet_host_create (NULL, 1, MAXPLAYERS * 2, 0, 0);
	srvpeer = enet_host_connect (localclient, &addr, MAXPLAYERS * 2, 0);

	if(enet_host_service (localclient, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		enet_host_flush(localclient);
	else
	{
		I_Error ("Connection to %s:%i failed!\n", host, addr.port);
		return;
	}
	
	if (enet_host_service(localclient, &event, 5000) > 0)
		// Make sure we are recieving a packet
		if (event.type == ENET_EVENT_TYPE_RECEIVE)
		{
			pkd = event.packet->data;

			// Read message type
			msgid = ReadUInt8((uint8_t**)&pkd);

			uint8_t sv_proto;

			// Make sure it's a welcome message
			if (msgid == MSG_WELCOME)
			{
				if((sv_proto = ReadUInt8((uint8_t**)&pkd)) != OBS_PROTO) // Incompatible!
					I_Error("Server protocol %i does not match client protocol %i\n", sv_proto, OBS_PROTO);

				prndindex = ReadInt32((int32_t**)&pkd);
				startepisode = ReadUInt8((uint8_t**)&pkd);
				startmap = ReadUInt8((uint8_t**)&pkd);
				startskill = (skill_t) ReadUInt8((uint8_t**)&pkd);
				deathmatch = ReadUInt8((uint8_t**)&pkd);
				localid = ReadUInt8((uint8_t**)&pkd);
				inGameMask = ReadUInt8((uint8_t**)&pkd);
				readmobjbuf = malloc(MAX_MOBJ_BUFFER);
				memcpy(readmobjbuf, pkd, MAX_MOBJ_BUFFER);
				pkd += MAX_MOBJ_BUFFER;
				readsecbuf = malloc(((int*)pkd)[0]);
				memcpy(readsecbuf, pkd, ((int*)pkd)[0]);
				pkd += ((int*)pkd)[0]; // Read the sector data, then increment.

				CL_SendString(MSG_NICK, player_name);

				return;
			}
		}
	
	// Oops
	I_Error("Failed to recieve greeting message. Connection failed!\n");
	return;
}

void CL_Disconnect (void)
{
	// Called when quitting the game.
	ENetEvent event;

	enet_peer_disconnect(srvpeer, 1);

	while(enet_host_service(localclient, &event, 1000) > 0) // Wait up to a second for a reply from the server...
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_DISCONNECT:
				return; // Until the the server sends us back acknowledgement.
			default:
				enet_packet_destroy(event.packet);
		}
	}

	// Kill the connection at this point.
	enet_peer_reset(srvpeer);
	return;
}

void CL_Loop(void)
{
	ENetEvent event;

	while (enet_host_service(localclient, &event, 5) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_RECEIVE:
				CL_ParsePacket(event.packet);
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				I_Error("Connection to remote host was lost\n");
				break;
		}
	}

	// Check for chat:
	char chat_tmp;

	while (chat_tmp = HU_dequeueChatChar())
	{
		if (chat_tmp == '\b') // Remove backspaces, they suck.
		{
			if (chatindex > 0)
				chatindex --;
			chatmsg[chatindex] = 0;
		}
		else
		{
			chatmsg[chatindex] = chat_tmp;
			chatindex = (chatindex + 1) % 128;
		}
	}

	if (chatmsg[chatindex - 1] == '\r') // Okay to send it out now:
	{
		if(strlen(chatmsg) > 2)
		{
			printf(">>> %s\n", chatmsg);
			CL_SendString(MSG_CHAT, chatmsg);
		}
		memset(chatmsg, 0, sizeof(chatmsg));
		chatindex = 0;
	}

	// Print network bandwidth info to stdout
	if (netstat && !(gametic % 35))
	{
		ENetHost *lc = localclient;
		printf ("avg in: %i avg out: %i ping: %i\n", 
		        (lc->totalReceivedData / lc->totalReceivedPackets), (lc->totalSentData / lc->totalSentPackets),
		        srvpeer->lastRoundTripTime);
		if (!(gametic % 3500)) // Reset every 10 seconds:
			lc->totalReceivedData = lc->totalReceivedPackets =
			lc->totalSentData = lc->totalSentPackets = 0;
	}

	return;
}

// Pretty much the same here as serverside, except we're using different variables and reading some extra data,
// we could be super concise and move this into an o_common.c, but that can wait for later maybe.
void P_FireWeapon (player_t* player);
gameaction_t gameaction;
mobj_t *P_FindMobjById (int id);

void CL_ParsePacket(ENetPacket *pk)
{
	uint8_t from, msg;
	void *p = pk->data;
	mobj_t *mo;
	fixed_t oldx, oldy, oldz;
	
	from = ((uint8_t *)(pk->data))[pk->dataLength - 1];
	msg = ReadUInt8((uint8_t**)&p);

	switch(msg)
	{
		case MSG_POS:
		if(playeringame[from] && players[from].mo && players[from].mo->health)
		{
			mo = players[from].mo;
			oldx = mo->x;
			oldy = mo->y;
			oldz = mo->z;

			P_UnsetThingPosition(mo);

			mo->x = ReadInt32((int32_t**)&p);
			mo->y = ReadInt32((int32_t**)&p);
			mo->z = ReadInt32((int32_t**)&p);
			mo->angle = ReadInt32((int32_t**)&p);
			mo->momx = ReadInt32((int32_t**)&p);
			mo->momy = ReadInt32((int32_t**)&p);
			mo->momz = ReadInt32((int32_t**)&p);
			mo->subsector = R_PointInSubsector(mo->x, mo->y);
			mo->floorz = mo->subsector->sector->floorheight;
			mo->ceilingz = mo->subsector->sector->ceilingheight;

			if(!P_CheckPosition(mo, mo->x, mo->y)) // We need to make sure they aren't colliding with anything...
			{
				mo->x = oldx;
				mo->y = oldy;
				mo->z = oldz;
				// And if they do, move them back to their last "good" position.
			}

			P_SetThingPosition(mo);
		}
		break;

		case MSG_USE:
		if(players[from].mo && players[from].mo->health)
			P_UseLines(&players[from]);
		break;

		case MSG_STATE:
		if(players[from].mo && players[from].mo->health)
			P_SetMobjState(players[from].mo, (statenum_t)ReadUInt16((uint16_t**)&p));
		break;

		case MSG_FIRE:
		if(playeringame[from] && players[from].mo->health)
		{
			weapontype_t toFire = (weapontype_t)ReadInt8((uint8_t**)&p);
			if(toFire != players[from].readyweapon)
				players[from].readyweapon = toFire;
			players[from].refire = ReadInt16((int16_t**)&p);
			P_FireWeapon(&players[from]);
		}
		break;

		case MSG_FIRE2:
		{
			player_t *pl = &players[consoleplayer];
			weapontype_t weapon = (weapontype_t)ReadUInt8((uint8_t**)&p);
			fixed_t storeangle = pl->mo->angle;

			pl->refire = ReadInt16((int16_t**)&p);
			pl->mo->angle = (fixed_t)ReadInt32((int32_t**)&p);
			prndindex = ReadInt16((int16_t**)&p);

			CL_FireWeapon (pl, weapon);
			pl->mo->angle = storeangle;
			break;
		}

		case MSG_DAMAGE:
		{
			int damaged;
			damaged = ReadUInt8((uint8_t**)&p);
			if(players[damaged].mo)
				P_DamageMobj(players[damaged].mo, NULL, NULL, ReadInt32((int32_t**)&p), true);
		}
		break;

		case MSG_KILL:
		{
			int killed, killer;
			killed = ReadUInt8((uint8_t**)&p);
			killer = ReadUInt8((uint8_t**)&p);

			if(players[killed].mo && killer > MAXPLAYERS)
				P_KillMobj(NULL, players[killed].mo);
			else if(players[killed].mo && players[killer].mo)
				P_KillMobj(players[killer].mo, players[killed].mo);
		}
		break;

		case MSG_RESPAWN:
		{
			int dmstart;

			if(players[from].playerstate == PST_LIVE) // Don't respawn a living player. (Causes ghosts if you do)
				break;

			if(!playeringame[from])
			{
				playeringame[from] = true;
				if(players[from].mo)
				{
					players[from].mo->player = NULL;
					players[from].mo = NULL;
				}
			}

			players[from].playerstate = PST_REBORN;

			if(deathmatch) // Spawn in the recieved deathmatch spawn
			{
				// First dissociate the corpse...
				if(players[from].mo)
					players[from].mo->player = NULL;

				dmstart = ReadInt8((int8_t**)&p);
				deathmatchstarts[dmstart].type = from + 1;
				G_CheckSpot(from, &deathmatchstarts[dmstart]);
				P_SpawnPlayer(&deathmatchstarts[dmstart]);
			}
		}
		break;

		case MSG_PICKUP:
		{
			int picked;

			picked = ReadUInt8((uint8_t**)&p);
			if(playeringame[picked] && players[picked].mo)
				P_TouchSpecialThing(P_FindMobjById(ReadInt32((int**)&p)), players[picked].mo, true);

			break;
		}

		case MSG_WPICKUP:
		{
			P_GiveWeapon(&players[consoleplayer], (weapontype_t)ReadUInt8((uint8_t**)&p), false);
			break;
		}

		case MSG_CHAT:
		{
			char *c = malloc(128);

			printf("%s\n", p);
			sprintf(c, "%s", p);
			players[consoleplayer].message = c;
			break;
		}

		case MSG_DISC:
		{
			int d = ReadUInt8((uint8_t**)&p);
			mobj_t *mo;

			if(!playeringame[d] || !players[d].mo)
				return;

			playeringame[d] = false;
			mo = players[d].mo;
			P_RemoveMobj(mo);
			mo->player = NULL;
			players[d].mo = NULL;
			memset(&players[d], 0, sizeof(player_t));
			players[d].playerstate = PST_DEAD;
			break;
		}

		case MSG_EXITLEV:
			gameaction = ga_completed;
			break;

		default:
			break;		
	}
	if(pk->referenceCount == 0)
		enet_packet_destroy(pk);

	return;
}

void CL_SendPosUpdate(fixed_t x, fixed_t y, fixed_t z, fixed_t ang, fixed_t momx, fixed_t momy, fixed_t momz)
{
	ENetPacket *pk = enet_packet_create(NULL, 29, 0);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_POS);
	WriteInt32((int32_t**)&p, x);
	WriteInt32((int32_t**)&p, y);
	WriteInt32((int32_t**)&p, z);
	WriteInt32((int32_t**)&p, ang);
	WriteInt32((int32_t**)&p, momx);
	WriteInt32((int32_t**)&p, momy);
	WriteInt32((int32_t**)&p, momz);
	enet_peer_send(srvpeer, 0, pk);
	return;
}

void CL_SendUseCmd(void)
{
	ENetPacket *pk = enet_packet_create(NULL, 1, 0);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_USE);
	enet_peer_send(srvpeer, 0, pk);
	return;
}

void CL_SendStateUpdate(uint16_t state)
{
	ENetPacket *pk = enet_packet_create(NULL, 3, 0);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_STATE);
	WriteUInt16((uint16_t**)&p, state);
	enet_peer_send(srvpeer, 0, pk);
	return;
}

void CL_SendFireCmd(weapontype_t w, int refire)
{
	ENetPacket *pk = enet_packet_create(NULL, 4, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_FIRE);
	WriteInt8((int8_t**)&p, (int8_t) w);
	WriteInt16((int16_t**)&p, refire);
	enet_peer_send(srvpeer, 1, pk);
	return;
}

void CL_SendRespawn(int startnum)
{
	ENetPacket *pk = enet_packet_create(NULL, startnum < 0 ? 1 : 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_RESPAWN);
	if(startnum >= 0)
		WriteInt8((int8_t**)&p, (int8_t)startnum);
	enet_peer_send(srvpeer, 1, pk);
	return;
}

void CL_SendString(messagetype_e type, char *sending)
{
	ENetPacket *pk = enet_packet_create(NULL, strlen(sending) + 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, type);
	memcpy(p, sending, strlen(sending) + 1);
	enet_peer_send(srvpeer, 1, pk);
	return;
}

// This is horrible, and I should feel bad for this:

boolean firefromsrv;

void CL_FireWeapon (player_t *pl, weapontype_t weapon)
{
	firefromsrv = 1;

	switch (weapon)
	{
		case wp_pistol:
			P_BulletSlope (pl->mo);
			A_FirePistol (pl, NULL);
			break;
		case wp_shotgun:
			P_BulletSlope (pl->mo);
			A_FireShotgun (pl, NULL);
			break;
		case wp_supershotgun:
			P_BulletSlope (pl->mo);
			A_FireShotgun2 (pl, NULL);
			break;
		case wp_chaingun:
			P_BulletSlope (pl->mo);
			A_FireCGun (pl, NULL);
			break;
		case wp_missile:
			A_FireMissile (pl, NULL);
			break;
		case wp_plasma:
			A_FirePlasma (pl, NULL);
			break;
		case wp_bfg:
			A_FireBFG (pl, NULL);
			break;
		default: break;
	}

	firefromsrv = 0;
	return;
}

void CL_ReadSectorBuffer (char *secbuf)
{
	int secnum;
	specialtype_e spec;

	while ((secnum = ReadInt32((int32_t**)&secbuf)) >= 0) // Read until we reach the end, -1
	{
		if(spec = (specialtype_e)ReadUInt8((uint8_t**)&secbuf)) // Sector has a special
		{
			switch(spec)
			{
				case spt_ceiling:
				{
					ceiling_t *ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVSPEC, 0);
					P_AddThinker (&ceiling->thinker);
					sectors[secnum].specialdata = ceiling;
					
					ceiling->thinker.function.acp1 = (actionf_p1)T_MoveCeiling;
					ceiling->sector = &sectors[secnum];
					ceiling->type = (ceiling_e)ReadUInt8((uint8_t**)&secbuf);
					ceiling->bottomheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
					ceiling->topheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
					ceiling->speed = (fixed_t)ReadInt32((int32_t**)&secbuf);
					ceiling->crush = (boolean)ReadUInt8((uint8_t**)&secbuf);
					ceiling->direction = ReadInt8((int8_t**)&secbuf);
					ceiling->tag = ReadInt32((int32_t**)&secbuf);
					ceiling->olddirection = ReadInt8((int8_t**)&secbuf);
					break;
				}

				case spt_door:
				{
					vldoor_t *door = Z_Malloc (sizeof(*door), PU_LEVSPEC, 0);
					P_AddThinker (&door->thinker);
					sectors[secnum].specialdata = door;

					door->thinker.function.acp1 = (actionf_p1)T_VerticalDoor;
					door->sector = &sectors[secnum];
					door->type = (vldoor_e)ReadUInt8((uint8_t**)&secbuf);
					door->topheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
					door->speed = (fixed_t)ReadInt32((int32_t**)&secbuf);
					door->direction = ReadInt8((int8_t**)&secbuf);
					door->topwait = ReadInt32((int32_t**)&secbuf);
					door->topcountdown = ReadInt32((int32_t**)&secbuf);
					break;
				}

				case spt_floor:
				{
					floormove_t *floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
					P_AddThinker (&floor->thinker);
					sectors[secnum].specialdata = floor;

					floor->thinker.function.acp1 = (actionf_p1)T_MoveFloor;
					floor->sector = &sectors[secnum];
					floor->type = (floor_e)ReadUInt8((uint8_t**)&secbuf);
					floor->crush = (boolean)ReadUInt8((uint8_t**)&secbuf);
					floor->direction = ReadInt8((int8_t**)&secbuf);
					floor->newspecial = ReadInt32((int32_t**)&secbuf);
					floor->texture = (short)ReadInt16((int16_t**)&secbuf);
					floor->floordestheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
					floor->speed = (fixed_t)ReadInt32((int32_t**)&secbuf);
					break;
				}

				case spt_plat:
				{
					plat_t *plat = Z_Malloc (sizeof(*plat), PU_LEVSPEC, 0);
					P_AddThinker (&plat->thinker);
					sectors[secnum].specialdata = plat;

					plat->thinker.function.acp1 = (actionf_p1)T_PlatRaise;
					plat->sector = &sectors[secnum];
					plat->type = (plattype_e)ReadUInt8((uint8_t**)&secbuf);
					plat->speed = (fixed_t)ReadInt32((int32_t**)&secbuf);
					plat->high = (fixed_t)ReadInt32((int32_t**)&secbuf);
					plat->low = (fixed_t)ReadInt32((int32_t**)&secbuf);
					plat->wait = ReadInt32((int32_t**)&secbuf);
					plat->count = ReadInt32((int32_t**)&secbuf);
					plat->status = (plat_e)ReadUInt8((uint8_t**)&secbuf);
					plat->oldstatus = (plat_e)ReadUInt8((uint8_t**)&secbuf);
					plat->crush = (boolean)ReadUInt8((uint8_t**)&secbuf);
					plat->tag = ReadInt32((int32_t**)&secbuf);
					P_AddActivePlat(plat);
					break;
				}
					
				default:
				I_Error ("Read unknown sector type %i. Malformed packet?\n", spec);
				break;
			}
		}
		sectors[secnum].floorheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
		sectors[secnum].ceilingheight = (fixed_t)ReadInt32((int32_t**)&secbuf);
	}

	return;
}
