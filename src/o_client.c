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

#include "enet/enet.h"
#include "doomstat.h"
#include "o_client.h"
#include "o_common.h"

boolean server;
boolean client;

ENetPeer *srvpeer;

void CL_Connect (char *srv_hn)
{
        if (enet_initialize() != 0)
                return 1; // Initialize enet, if it fails, return 1

	ENetAddress addr = { ENET_HOST_ANY, 11666 };
	ENetEvent event;

	if (enet_address_set_host (&addr, srv_hn) < 0)
	{
		printf("Could not resolve address!\n");
		return;
	}

	printf("Attempting to connect to %s:%i\n", srv_hn, addr.port);

	localclient = enet_host_create (NULL, 1, 2, 0, 0);
	srvpeer = enet_host_connect (localclient, &addr, 2, 0);

	if(enet_host_service (localclient, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
	{
		enet_host_flush(localclient);
		autostart = 1;
		client = 1;
		server = 0;
	}

	if(enet_host_service (localclient, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_RECEIVE
	   && ReadUInt8((uint8_t**)&event.packet->data) == MSG_WELCOME) // Wait for server's greeting, set localid to the second marker we get.
	{
		localid = ReadUInt8((uint8_t**)&event.packet->data);
		inGameMask = ReadUInt8((uint8_t**)&event.packet->data);
		printf("DBG: Setting client's id to: %i\n", localid);
		return;
	}

	printf("Connection to %s failed!\n", srv_hn);
}

void CL_Loop(void)
{
	ENetEvent event;
	while (enet_host_service(localclient, &event, 5) > 0)
	{
		switch(event.type)
		{}
	}
	return;
}

void CL_SendPosUpdate(fixed_t x, fixed_t y, fixed_t z, fixed_t ang, fixed_t momx, fixed_t momy, fixed_t momz)
{
	ENetPacket *pk = enet_packet_create(NULL, 32, 0);
	void *start = pk->data;
	void *p = start;
	WriteUInt8((uint8_t**)&p, MSG_POS);
	WriteInt32((int32_t**)&p, x);
	WriteInt32((int32_t**)&p, y);
	WriteInt32((int32_t**)&p, z);
	WriteInt32((int32_t**)&p, ang);
	WriteInt32((int32_t**)&p, momx);
	WriteInt32((int32_t**)&p, momy);
	WriteInt32((int32_t**)&p, momz);
	enet_packet_resize(pk, p-start);
	enet_peer_send(srvpeer, 0, pk);
}

void CL_SendUseCmd(void)
{
	ENetPacket *pk = enet_packet_create(NULL, 1, ENET_PACKET_FLAG_RELIABLE);
	void *start = pk->data;
	void *p = start;
	WriteUInt8((uint8_t**)&p, MSG_USE);
	enet_peer_send(srvpeer, 1, pk);
}

void CL_SendStateUpdate(uint16_t state)
{
	ENetPacket *pk = enet_packet_create(NULL, 3, 0);
	void *start = pk->data;
	void *p = start;
	WriteUInt8((uint8_t**)&p, MSG_STATE);
	WriteUInt16((uint16_t**)&p, state);
	enet_peer_send(srvpeer, 0, pk);
}

void CL_SendFireCmd(weapontype_t w, int refire)
{
	ENetPacket *pk = enet_packet_create(NULL, 9, ENET_PACKET_FLAG_RELIABLE);
	void *start = pk->data;
	void *p = start;
	WriteUInt8((uint8_t**)&p, MSG_FIRE);
	WriteInt8((int32_t**)&p, (int8_t) w);
	WriteInt32((int32_t**)&p, refire);
	enet_peer_send(srvpeer, 0, pk);
}
