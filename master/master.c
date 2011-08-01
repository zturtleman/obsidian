//
// Obsidian Master Server
// [c] 2011 tm512, All Rights Reserved
//
// This file is licensed under the WTFPL,
// The Do What the Fuck You Want To Public License,
// under the condition that you leave me alone about it.
//

// System includes
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

// My includes
#include "config.h"
#include "master.h"

server_t serverlist[MAXSERVERS]; // Server list
sock_t *slhead = NULL, *sltail = NULL; // Socket linked list magic
int numservers = 0;

int mastersock;
struct addrinfo masteraddr;
struct timespec lasttic = { 0, 0 };
fd_set fdread, fdwrite, fdexce;

int initialize (void)
{
	struct addrinfo hints, *addrList;
	int status;

	memset (&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo (NULL, MASTERPORT, &hints, &addrList)) != 0)
	{
		fprintf (stderr, "ERROR: getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}

	if ((mastersock = socket (addrList->ai_family, addrList->ai_socktype, addrList->ai_protocol)) < 0)
	{
		fprintf (stderr, "ERROR: could not create socket: %i\n", errno);
		return 2;
	}

	masteraddr = *addrList; // Just take the first address, for now.
	freeaddrinfo (addrList);

	if (bind (mastersock, masteraddr.ai_addr, masteraddr.ai_addrlen) < 0)
	{
		fprintf (stderr, "ERROR: could not bind socket: %i\n", errno);
		return 3;
	}

	if (listen (mastersock, 15) < 0)
	{
		fprintf (stderr, "ERROR: could not listen on socket: %i\n", errno);
		return 4;
	}

	// Okay, so if we've gotten here, the server's socket is bound and listening
	// for new connections. However, since the socket is still blocking, it will
	// NOT perform well for our purposes. Set the socket to non-blocking:

	fcntl (mastersock, F_SETFL, O_NONBLOCK);

	// At this point, the server is ready to wait for new connections and handle them! :D
	return 0;
}

// Checks for new connections via accept() and loads them into sock_t linked list.
void getnewconn (void)
{
	struct sockaddr_storage tempAddr;
	int tempSock;
	static sock_t *slnext = NULL;

	// if slnext is NULL, we need to allocate some more memory for a new one.
	// After using up this new one, we set it back to NULL.
	if (!slnext)
	{
		slnext = (sock_t *) malloc (sizeof (sock_t));
		memset (slnext, 0, sizeof (sock_t));
	}

	if ((tempSock = accept (mastersock, (struct sockaddr *)&tempAddr, sizeof(struct sockaddr_storage))) < 0)
		return; // Not this time. D:
	else // YES A NEW CLIENT
	{
		// Fill it out, then put it in the linked list.
		if (!sltail && !slhead) sltail = slhead = slnext; // :|
		slnext->s = tempSock;
		slnext->addr = *((struct sockaddr *) &tempAddr);
		slhead->next = slnext;
		slnext->prev = slhead;
		slnext->next = NULL;
		slhead = slnext;
		slnext = NULL; // Let's get another next time.
	}

	return;
}

void masterloop (void)
{
	getnewconn();

	usleep (50000);
	return;
}

int main (void)
{
	// Hello, world
	printf ("obsidian master server\n");

	if (initialize()) return 1;
	char ipchar[32];
	inet_ntop (AF_INET, &(((struct sockaddr_in *)masteraddr.ai_addr)->sin_addr), ipchar, 32);
	printf ("bound to %s:%s\n", ipchar, MASTERPORT);
	printf ("server_t struct is %i bytes\n", sizeof(server_t));

	for (;;) masterloop();

	return 0;
}
