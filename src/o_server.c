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

#include <stdio.h>
#include "enet/enet.h"

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"

#include "o_server.h"
#include "o_common.h"

boolean server;
boolean client;

int O_SV_Main (void) 
{
	if (enet_initialize() != 0) 
		return 1; // Initialize enet, if it fails, return 1

	atexit(enet_deinitialize);

	addr.host = ENET_HOST_ANY;
	addr.port = 11666;

	srv = enet_host_create(&addr, MAXPLAYERS, 4, 0, 0);
	if(srv == NULL)
		return 1;
	else
	{
		printf("Obsidian Dedicated Server started on port %i\n", addr.port);
		autostart = 1;
		server = 1;
		client = 0;
		return 0;
	}

	// Not liking how this is, but since it seems I can't do anything about setting client's "type" in the header...
	int i;
	for(i = 0; i < MAXPLAYERS; i++)
		clients[i].type = CT_EMPTY;
}

void O_SV_Loop (void)
{
	ENetEvent event;
	while (enet_host_service(srv, &event, 5) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
			{
				int c = O_SV_FindEmptyClientNum(); // Find an empty client for this guy, or kick him out
				if (c < 0) 
				{ enet_peer_reset(event.peer); break; }
				clients[c].type = CT_CONNECT;
				clients[c].id = c;
				clients[c].peer = event.peer;
				clients[c].player = &players[c];
				if(!clients[c].player) 
				{
					enet_peer_reset(clients[c].peer);
					break;
				}
				char hn[512];
				printf("connected: %s\n", (enet_address_get_host(&clients[c].peer->address, hn, sizeof(hn))==0) ? hn : "localhost");
				O_SV_ClientWelcome(&clients[c]);
				break;
			}
		}
	}
	return;
}

int O_SV_FindEmptyClientNum(void)
{
	int i;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		printf("DBG: New Client connected, finding a client_t for him...\n");
		if(clients[i].type == CT_EMPTY)
		{
			printf("client_t found! Returning %i\n", i);
			return i;
		}
	}
	return -1;
}
		
player_t* O_SV_FindEmptyPlayer(void) // Should be unnecessary
{
	int i;
	for(i = 0; i < MAXPLAYERS; i++)
	{
		printf("DBG: Searching for a player for new client: %i\n", i);
		if(!playeringame[i]) // Found an empty player slot, let's return it.
		{
			printf("DBG: Giving new client players[%i]\n", i); 
			return &players[i];
		}
	}
	return NULL; // No player found
}

void O_SV_ClientWelcome (client_t* cl)
{
	playeringame[cl->id + 1] = true; // + 1 since server is in game at this stage in dev
	players[cl->id + 1].playerstate = PST_REBORN;
	return;
}
