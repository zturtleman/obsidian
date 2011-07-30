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
    o_master : Master server broadcasting
*/

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "master/master.h"
#include "o_master.h"

struct sockaddr_in thisServer;
fd_set fdset;

// Sockets suck.
// This file is here to hide them from the rest of the code.

int MA_Init (void)
{
	int status;
	struct addrinfo hints, *addrList;
	struct timeval timeout = { 10, 0 };

	memset (&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo("localhost", "11500", &hints, &addrList)) != 0)
	{
		printf ("MA_Init: Error in getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}

	if ((master.sock = socket (addrList->ai_family, addrList->ai_socktype, addrList->ai_protocol)) < 0)
	{
		printf ("MA_Init: Could not create socket.\n");
		return 2;
	}

	fcntl (master.sock, F_SETFL, O_NONBLOCK);
	FD_ZERO (&fdset);
	FD_SET (master.sock, &fdset);

	master.server = *(struct sockaddr_in *) addrList->ai_addr;
	master.sendbuf.head = master.sendbuf.tail = (uint8_t*) malloc (MAXMASTERBUFFER);
	master.recvbuf.head = master.recvbuf.tail = (uint8_t*) malloc (MAXMASTERBUFFER);

	freeaddrinfo (addrList);

	connect (master.sock, &master.server, sizeof (master.server));

	if (select (master.sock + 1, NULL, &fdset, NULL, &timeout) == 1)
	{
		int serror;
		socklen_t slen = sizeof (serror);
		getsockopt (master.sock, SOL_SOCKET, SO_ERROR, &serror, &slen);

		if (!serror) // Success!
		{
			printf ("MA_Init: Successfully connected to master.\n");
			return 0;
		}
		else
		{
			printf ("MA_Init: Could not connect to master.\n");
			return 4;
		}
	}
}

// Sends the current master.buffer to the master server
// Returns 0 on success.

int MA_Send (void)
{
	int totalToSend = master.sendbuf.head - master.sendbuf.tail;
	int totalSent, tempSent;

	printf ("MA_Send: Attempting to send %i bytes.\n", totalToSend);

	totalSent = tempSent = 0;
	while (totalSent < totalToSend)
	{
		tempSent = send (master.sock, (master.sendbuf.tail + totalSent), (totalToSend - totalSent), 0);
		if (tempSent < 0)
		{
			printf ("MA_Send: Error %i!\n");
			return 1;
		}
		totalSent += tempSent;
	}
	return 0;
}
