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
    o_client : Obsidian Client
*/

// GhostlyDeath <March 27, 2011> -- Lean and mean for Windows (since ENet does not use it)
#define WIN32_LEAN_AND_MEAN

#include "enet/enet.h"

#include "doomstat.h"
#include "p_mobj.h"
#include "r_data.h"
#include "r_defs.h"
#include "r_main.h"
#include "p_local.h"
#include "deh_main.h"
#include "p_inter.h"

#include "o_client.h"
#include "o_common.h"

boolean server;
boolean client;

ENetPeer *srvpeer;

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

	if (port)
		addr.port = atoi(port);

	if (enet_address_set_host (&addr, host) < 0)
	{
		printf("Could not resolve address!\n");
		return;
	}

	printf("Attempting to connect to %s:%i\n", host, addr.port);

	localclient = enet_host_create (NULL, 1, MAXPLAYERS * 2, 0, 0);
	srvpeer = enet_host_connect (localclient, &addr, MAXPLAYERS * 2, 0);

	if(enet_host_service (localclient, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
	{
		enet_host_flush(localclient);
		autostart = 1;
		client = 1;
		server = 0;
	}
	else
	{
		printf("Connection to %s:%i failed!\n", host, addr.port);
		return;
	}
	
	if (enet_host_service(localclient, &event, 5000) > 0)
		// Make sure we are recieving a packet
		if (event.type == ENET_EVENT_TYPE_RECEIVE)
		{
			pkd = event.packet->data;

			// Read message type
			msgid = ReadUInt8((uint8_t**)&pkd);

			// Make sure it's a welcome message
			if (msgid == MSG_WELCOME)
			{
				localid = ReadUInt8((uint8_t**)&pkd);
				inGameMask = ReadUInt8((uint8_t**)&pkd);
				return;
			}
		}
	
	// Oops
	printf("Failed to recieve greeting message. Connection failed!\n");
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
				DEH_printf("Connection to remote host was lost\n");
				players[consoleplayer].message = "Connection to remote host was lost";
				break;
		}
	}
	return;
}

// Pretty much the same here as serverside, except we're using different variables and reading some extra data,
// we could be super concise and move this into an o_common.c, but that can wait for later maybe.
void P_FireWeapon (player_t* player);
mobj_t *P_FindMobjById (int id);

void CL_ParsePacket(ENetPacket *pk)
{
	uint8_t from, msg;
	void *p = pk->data;
	
	from = ((uint8_t *)(pk->data))[pk->dataLength - 1];
	msg = ReadUInt8((uint8_t**)&p);

	switch(msg)
	{
		case MSG_POS:
		if(playeringame[from] && players[from].mo && players[from].mo->health)
		{
			P_UnsetThingPosition(players[from].mo);
			players[from].mo->x = ReadInt32((int32_t**)&p);
			players[from].mo->y = ReadInt32((int32_t**)&p);
			players[from].mo->z = ReadInt32((int32_t**)&p);
			players[from].mo->angle = ReadInt32((int32_t**)&p);
			players[from].mo->momx = ReadInt32((int32_t**)&p);
			players[from].mo->momy = ReadInt32((int32_t**)&p);
			players[from].mo->momz = ReadInt32((int32_t**)&p);
			players[from].mo->subsector = R_PointInSubsector(players[from].mo->x, players[from].mo->y);
			players[from].mo->floorz = players[from].mo->subsector->sector->floorheight;
			players[from].mo->ceilingz = players[from].mo->subsector->sector->ceilingheight;
			P_SetThingPosition(players[from].mo);
		}
		break;

		case MSG_USE:
		if(players[from].mo && players[from].mo->health)
			P_UseLines(&players[from]);
		break;

		case MSG_JOIN:
		{
			int newpl;
			newpl = ReadUInt8((uint8_t**)&p);
			playeringame[newpl] = true;
			players[newpl].playerstate = PST_REBORN;
		}
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
		if(playeringame[from])
		{
			players[from].playerstate = PST_REBORN;
			dmStart = ReadUInt8((uint8_t**)&p);
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

void CL_SendReborn(void)
{
	ENetPacket *pk = enet_packet_create(NULL, 1, ENET_PACKET_FLAG_RELIABLE);
	void *p = pk->data;

	WriteUInt8((uint8_t**)&p, MSG_RESPAWN);
	enet_peer_send(srvpeer, 1, pk);
	return;
}
