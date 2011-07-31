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

typedef struct
{
	// Properties:
	char hostname[128];
	char website[64];
	char email[64];
	uint8_t episode;
	uint8_t map;
	uint8_t skill;
	uint8_t gametype;
	uint8_t clients;
	uint8_t maxclients;
	char playernames[MAXPLAYERS][MAXPLAYERNAME];
} server_t;

typedef struct sock_s
{
	int s;
	struct sockaddr *addr;
	enum
	{ 
		uninit,
		launcher, 
		server 
	} type;
	server_t *server; // NULL for launchers, 

	sock_s *prev;
	sock_s *next;
} sock_t;

#endif
