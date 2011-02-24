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

int O_SV_Main (void) 
{
	if (enet_initialize() < 0) 
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
		if(!autostart) // Set game to autostart, we don't want the server going to the title screen.
			autostart = 1;
		server = 1;
		client = 0;
		return 0;
	}
}

void O_SV_Loop (void)
{
	ENetEvent event;
	while (enet_host_service(srv, &event, 10) > 0)
	{
		switch(event.type)
		{
			case ENET_EVENT_TYPE_CONNECT:
			{
				client_t c;
				c.peer = event.peer;
				char hn[512];
				printf("Client connected! (%s)\n", (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "localhost");
				break;
			}
		}
	}
}
