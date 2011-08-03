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

sock_t *serverlist[MAXSERVERS]; // Server list
sock_t *slhead = NULL, *sltail = NULL; // Socket linked list magic
int numservers = 0;

int mastersock;
struct addrinfo masteraddr;

fd_set fdread, fdwrite, fdexce;
int maxfd = 0;

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

	maxfd = mastersock;

	// Okay, so if we've gotten here, the server's socket is bound and listening
	// for new connections. However, since the socket is still blocking, it will
	// NOT perform well for our purposes. Set the socket to non-blocking:

	fcntl (mastersock, F_SETFL, O_NONBLOCK);

	// Zero the fd_sets...
	FD_ZERO (&fdread);
	FD_ZERO (&fdwrite);
	FD_ZERO (&fdexce);

	// At this point, the server is ready to wait for new connections and handle them! :D
	return 0;
}

// Checks for new connections via accept() and loads them into sock_t linked list.
// Returns 0 if a new socket was added.
int getnewconn (void)
{
	static socklen_t socklen = sizeof (struct sockaddr_storage);
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

	// Loop through and get new waiting connections, but don't do more than 10 each time around.
	if ((tempSock = accept (mastersock, (struct sockaddr *)&tempAddr, &socklen)) >= 0)
	{
		// YES A NEW CLIENT WOO YEAH
		printf (".");
		// Fill it out, then put it in the linked list.
		if (!sltail && !slhead) sltail = slhead = slnext; // :|
		slnext->s = tempSock;
		slnext->addr = *((struct sockaddr *) &tempAddr);
		slhead->next = slnext;
		slnext->prev = slhead;
		slnext->next = NULL;
		slhead = slnext;
		slnext = NULL; // Let's get another next time.

		// Now, add the socket to our fd_sets, for select():
		FD_SET (tempSock, &fdread);
		FD_SET (tempSock, &fdwrite);
		FD_SET (tempSock, &fdexce);

		// If this new file descriptor is higher, use that for select.
		maxfd = tempSock > maxfd ? tempSock : maxfd;
	}

	return (tempSock < 0);
}

void checksockets (void)
{
	struct timeval timeout = { 0, 0 };

	if (select (maxfd + 1, &fdread, &fdwrite, &fdexce, &timeout) < 0) // Error!
	{
		fprintf (stderr, "ERROR: checksockets select failed: %i\n", errno);
		return;
	}

	// Iterate through the linked list
	sock_t *crawler = sltail;
	for (; crawler != NULL; crawler = crawler->next)
	{
		// If the socket is not initialized, don't check for being able to write
		// to them, just see if they're open for reading.
		if ((crawler->type == uninit || crawler->type == clean) && FD_ISSET (crawler->s, &fdread))
			uninit_handler (crawler);

		// Don't ever read from launchers, just give them their stuff and close()
		if (crawler->type == launcher && FD_ISSET (crawler->s, &fdwrite))
			launcher_handler (crawler);

		// Servers can be read and written to, 
		if (crawler->type == server && (FD_ISSET (crawler->s, &fdread) || FD_ISSET (crawler->s, &fdwrite)))
			server_handler (crawler);
	}

	return;
}

void masterloop (void)
{
	int i;

	// Get up to 10 new connections per cycle.
	// We can change this to alter performance.
	for (i = 0; i < 10; i++)
		if (getnewconn())
			break;

	// Now check all of our file descriptors to see which ones are ready
	// to do something. Then, we can try reading or writing to them from
	// there.
	checksockets();

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

	for (;;) masterloop();

	return 0;
}
