//
// Obsidian Master Server
// [c] 2011 tm512, All Rights Reserved
//
// This file is licensed under the WTFPL,
// The Do What the Fuck You Want To Public License,
// under the condition that you leave me alone about it.
//

#ifndef __MASTER_H__
#define __MASTER_H__

#define SERVER_CHALLENGE 0x45
#define LAUNCHER_CHALLENGE 0x46

#define SERVER_INIT 0x55
#define SERVER_UPD 0x65

#define MAXPLAYERS 4
#define MAXPLAYERNAME 10

typedef struct sock_s
{
	int s;
	struct sockaddr addr;
	enum
	{ 
		uninit,
		launcher, 
		server
	} type;
	struct timeval ctime;
	int heartbeats; // (servers only)
	uint16_t port; // (servers only, nbo)

	struct sock_s *prev;
	struct sock_s *next;
} sock_t;

#endif
