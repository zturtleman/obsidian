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
#include "r_defs.h"
#include "r_main.h"
#include "p_local.h"
#include "p_pspr.h"

#include "o_server.h"
#include "o_common.h"

boolean server;
boolean client;

// Have a damages integer for each client. Every tic, this will be sent out to the client and reset if they were hurt that tic. 
int damages[MAXPLAYERS];

int SV_Main (void) 
{
	int i;

	if (enet_initialize() != 0) 
		return 1; // Initialize enet, if it fails, return 1

	atexit(enet_deinitialize);

	addr.host = ENET_HOST_ANY;
	addr.port = 11666;

	srv = enet_host_create(&addr, MAXPLAYERS, MAXPLAYERS * 2, 0, 0);
	if(srv == NULL)
		return 1;
	else
	{
		printf("Obsidian Dedicated Server started on port %i\n", addr.port);
		autostart = 1;
		server = 1;
		client = 0;

		for(i = 0; i < MAXPLAYERS; i++)
		{
			clients[i].type = CT_EMPTY;
			damages[i] = 0;
		}
		return 0;
	}

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
					case 1:
						SV_DropClient(from, "client exited");
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

void SV_ClientWelcome (client_t* cl)
{
	ENetPacket *pk = enet_packet_create(NULL, 32, ENET_PACKET_FLAG_RELIABLE);
	ENetPacket *newplayer = enet_packet_create(NULL, 2, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;
	uint8_t inGame = 0;
	uint8_t i;

	playeringame[cl->id] = true;
	cl->player->playerstate = PST_REBORN;
	WriteUInt8((uint8_t**)&p, MSG_WELCOME); // put a greeting marker on it
	WriteUInt8((uint8_t**)&p, cl->id); // client will set this to consoleplayer
	for (i = 0; i < MAXPLAYERS; i++)
		if(playeringame[i])
			inGame |= 1 << i;
	WriteUInt8((uint8_t**)&p, inGame);
	enet_packet_resize(pk, (uint8_t*)p - (uint8_t*)pk->data);
	enet_peer_send(cl->peer, 0, pk);
	cl->type = CT_ACTIVE;

	// Now, we inform everyone else about the new player. 
	p = newplayer->data;
	WriteUInt8((uint8_t**)&p, MSG_JOIN);
	WriteUInt8((uint8_t**)&p, cl->id);
	SV_BroadcastPacket(newplayer, cl->id);
	
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
}

void P_FireWeapon (player_t* player);

void SV_ParsePacket (ENetPacket *pk, ENetPeer *p)
{
	int from = SV_ClientNumForPeer(p);
	void *pkp = pk->data;
	uint8_t msg;
	boolean valid;

	if(from < 0) return; // Not a client
	msg = ReadUInt8((uint8_t**)&pkp);
	valid = 1;

	switch(msg)
	{
		case MSG_POS:
		if(clients[from].player && clients[from].player->mo)
		{
			P_UnsetThingPosition(clients[from].player->mo);
			clients[from].player->mo->x = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->y = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->z = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->angle = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->momx = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->momy = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->momz = ReadInt32((int32_t**)&pkp);
			clients[from].player->mo->subsector = R_PointInSubsector(clients[from].player->mo->x, clients[from].player->mo->y);
			clients[from].player->mo->floorz = clients[from].player->mo->subsector->sector->floorheight;
			clients[from].player->mo->ceilingz = clients[from].player->mo->subsector->sector->ceilingheight;
			P_SetThingPosition(clients[from].player->mo);
		}
		break;

		case MSG_USE:
		if(clients[from].player)
			P_UseLines(clients[from].player);
		break;

		case MSG_STATE:
		if(clients[from].player->mo)
			P_SetMobjState(clients[from].player->mo, (statenum_t)ReadUInt16((uint16_t**)&pkp));
		break;

		case MSG_FIRE:
		if(clients[from].player)
		{
			weapontype_t toFire = (weapontype_t)ReadInt8((uint8_t**)&pkp);
			if(toFire != clients[from].player->readyweapon)
				clients[from].player->readyweapon = toFire;
			clients[from].player->refire = ReadInt32((int32_t**)&pkp);
			P_FireWeapon(clients[from].player);
		}
		break;

		case MSG_RESPAWN:
		if(clients[from].player)
			clients[from].player->playerstate = PST_REBORN;
		break;

		default:
			valid = 0;
			break;
	}
	
	if(valid)
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
			ENetPacket *dmg = enet_packet_create(NULL, 5, ENET_PACKET_FLAG_RELIABLE);
			void *p = dmg->data;

			WriteUInt8((uint8_t**)&p, MSG_DAMAGE);
			WriteUInt8((uint8_t**)&p, i);
			WriteInt32((int32_t**)&p, damages[i]);
			SV_BroadcastPacket(dmg, -1);

			damages[i] = 0;
		}
	}
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
