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
#include <sys/time.h>

// My includes
#include "config.h"
#include "master.h"

sock_t *serverlist[MAXSERVERS] = { 0 }; // Server list
sock_t *slhead = NULL, *sltail = NULL; // Socket linked list magic
int numservers = 0;
struct timeval timenow;

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

	// Get new waiting connections,
	if ((tempSock = accept (mastersock, (struct sockaddr *)&tempAddr, &socklen)) >= 0)
	{
		// Fill it out, then put it in the linked list.
		slnext->s = tempSock;
		// Non-blocking please:
		fcntl (slnext->s, F_SETFL, O_NONBLOCK);

		slnext->addr = *((struct sockaddr *) &tempAddr);
		gettimeofday (&slnext->ctime, NULL);
		if (!sltail && !slhead)
		{
			sltail = slhead = slnext; // :|
			slnext->next = slnext->prev = NULL;
		}
		else
		{
			slhead->next = slnext;
			slnext->prev = slhead;
			slnext->next = NULL;
			slhead = slnext;
		}

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

// Removes a socket from existence
void purgesock (sock_t *sock)
{
	// Server? Remove it from the list:
	int i;
	if (sock->type == server)
	{
		for (i = 0; i < MAXSERVERS; i++)
			if (serverlist[i] == sock)
			{
				serverlist[i] = NULL;
				numservers --;
				break;
			}
		
		char ipchar[32];
		inet_ntop (AF_INET, &((struct sockaddr_in *) &sock->addr)->sin_addr, ipchar, 32);
		printf ("Removing server [%i] [%s:%i]\n", i, ipchar, ntohs (sock->port));
	}

	// Link prev and next to each other
	if (sock->next)
		sock->next->prev = sock->prev;
	if (sock->prev)
		sock->prev->next = sock->next;

	// Set new sltail and slhead where applicable
	if (sock == slhead)
		slhead = sock->prev;
	if (sock == sltail)
		sltail = sock->next;

	// Clear file descriptor from our fdsets
	FD_CLR (sock->s, &fdread);
	FD_CLR (sock->s, &fdwrite);
	FD_CLR (sock->s, &fdexce);

	// Close the file descriptor and free the allocated memory
	close (sock->s);
	free (sock);
	return;
}

// Returns an empty slot in the serverlist array, or -1 if it's full
int findemptyserver (void)
{
	int i;

	for (i = 0; i < MAXSERVERS; i++)
		if (serverlist[i] == NULL)
			return i;

	return -1;
}

void uninit_handler (sock_t *sock)
{
	// For this, we're only gonna need a single byte
	uint8_t buf;
	int res; // Store the return value of recv/send

	if ((res = recv (sock->s, &buf, 1, 0)) < 1)
		purgesock (sock);

	if (buf == SERVER_CHALLENGE) // Server
	{
		int freeSlot; // Find free slot on server list array
		short port; // Read in the server's port number.

		if (recv (sock->s, &sock->port, 2, 0) < 2)
		{
			purgesock (sock);
			return;
		}

		if ((freeSlot = findemptyserver()) < 0) purgesock (sock);
		else serverlist[freeSlot] = sock;

		sock->type = server;
		sock->heartbeats = 2; // Miss 2 heartbeats, get booted
		numservers ++;

		// Echo this back to them so they know we recieved the message
		// We don't care about return value here
		send (sock->s, &buf, 1, MSG_NOSIGNAL);

		char ipchar[32];
		inet_ntop (AF_INET, &((struct sockaddr_in *) &sock->addr)->sin_addr, ipchar, 32);
		printf ("Added server    [%i] [%s:%i]\n", freeSlot, ipchar, ntohs (sock->port));
	}
	else if (buf == LAUNCHER_CHALLENGE) // Launcher
		sock->type = launcher;
	else purgesock (sock); // Neither server or launcher, goodbye.

	return;
}

void launcher_handler (sock_t *sock)
{
	// Allocate 6 bytes per server, plus 2 bytes for the number of servers.
	// Also make a duplicate pointer so we do not modify the original.
	int toMalloc = (6 * numservers) + 2;
	int i, totalSent, tempSent;
	uint8_t *buf = (uint8_t *) malloc (toMalloc);
	uint8_t *p = buf;

	// Write the number of servers (short), then increment
	*((uint16_t*)p) = htons ((uint16_t) numservers);
	p += 2;

	for (i = 0; i < MAXSERVERS; i++)
	{
		if (serverlist[i])
		{
			// Write IP address
			*((uint32_t*)p) = (uint32_t) ((struct sockaddr_in *) &serverlist[i]->addr)->sin_addr.s_addr;
			p += 4;

			// Write port number
			*((uint16_t*)p) = (uint16_t) serverlist[i]->port;
			p += 2;
		}
	}

	totalSent = tempSent = 0;

	while (totalSent < toMalloc)
	{
		if ((tempSent = send (sock->s, buf + totalSent, toMalloc - totalSent, MSG_NOSIGNAL)) < 0)
			break;

		totalSent += tempSent;
	}

	// Goodbye now!
	purgesock (sock);
 
	return;
}

void server_handler (sock_t *sock)
{
	uint8_t buf;

	// Receive if possible
	if (recv (sock->s, &buf, 1, 0) < 1)
		return;

	if (buf == SERVER_UPD)
		sock->heartbeats = 2;

	return;
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
		if (crawler->type == uninit && FD_ISSET (crawler->s, &fdread))
			uninit_handler (crawler);

		// Don't ever read from launchers, just give them their stuff and close()
		if (crawler->type == launcher && FD_ISSET (crawler->s, &fdwrite))
			launcher_handler (crawler);

		// Servers will send periodic messages to reset their heartbeat counter 
		if (crawler->type == server && FD_ISSET (crawler->s, &fdread))
			server_handler (crawler);

		// If a socket is too old, remove it.
		// Do this ONLY to ->prev so we don't mess up our pointers!
		if (crawler->prev && crawler->prev->ctime.tv_sec + (crawler->prev->type == server ? SERVER_TIMEOUT : 3) < timenow.tv_sec)
		{
			if (crawler->prev->type == server)
				if (--crawler->prev->heartbeats)
					continue;

			purgesock (crawler->prev);
			continue;
		}

		// Unfortunately the above doesn't work if crawler == slhead
		if (crawler == slhead && crawler->ctime.tv_sec + (crawler->type == server ? SERVER_TIMEOUT : 3) < timenow.tv_sec)
		{
			if (crawler->type == server)
				if (--crawler->heartbeats)
					continue;

			purgesock (crawler);
			break;
		}
	}

	return;
}

void masterloop (void)
{
	int i;

	gettimeofday (&timenow, NULL);

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
	printf ("bound to %s:%s\n\n", ipchar, MASTERPORT);

	for (;;) masterloop();

	return 0;
}
