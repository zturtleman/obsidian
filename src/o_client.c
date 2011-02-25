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
#include "o_client.h"

ENetHost *localclient;

void O_CL_Connect (char *srv_hn)
{
        if (enet_initialize() != 0)
                return 1; // Initialize enet, if it fails, return 1

	printf("Connecting obsidian client to %s\n", srv_hn);
	ENetAddress addr = { ENET_HOST_ANY, 11666 };
	ENetPeer *peer;
	ENetEvent event;

	if (enet_address_set_host (&addr, srv_hn) < 0)
	{
		printf("Could not resolve address!\n");
		return;
	}

	printf("%i\n", addr.port);

	localclient = enet_host_create (NULL, 1, 2, 0, 0);
	peer = enet_host_connect (localclient, &addr, 2, 0);
	if(enet_host_service (localclient, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
	{
		printf("connection succeeded\n");
		enet_host_flush(localclient);
	}
	else
		printf("connection failed\n");
}
