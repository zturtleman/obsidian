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

#ifndef O_SERVER_H
#define O_SERVER_H

// GhostlyDeath <March 27, 2011> -- Lean and mean for Windows (since ENet does not use it)
#define WIN32_LEAN_AND_MEAN

#include "enet/enet.h"
#include "o_common.h"
#include "d_player.h"

// Functions
void SV_Main(void);
void SV_Loop(void);
void SV_DropClient(int cn, const char *reason);
int SV_FindEmptyClientNum(void);
player_t* SV_FindEmptyPlayer(void);
void SV_ParsePacket (ENetPacket *pk, ENetPeer *p);
void SV_ResizeBroadcastPacket(ENetPacket *pk, int from, uint8_t msg);
void SV_BroadcastPacket(ENetPacket *pk, int exclude);
void SV_SendDamage(void);
void SV_SendPickup(player_t *player, int netid);
void SV_SendWeapon(player_t *player, weapontype_t weapon);
void SV_DamageMobj(mobj_t *target, int damage);
void SV_KillMobj(mobj_t *source, mobj_t *target);
int SV_ClientNumForPeer(ENetPeer *p);

// Variables

enum {
CT_EMPTY,
CT_CONNECT,
CT_ACTIVE
};

ENetAddress addr;
ENetHost *srv;

// Have a damages integer for each client. Every tic, this will be sent out to the client and reset if they were hurt that tic.
int damages[MAXPLAYERS];

typedef struct
{
	int type;
	int id;
	ENetPeer *peer;
	player_t *player;
	char name[MAXPLAYERNAME + 1];
} client_t;

client_t clients[MAXPLAYERS];
void SV_ClientWelcome (client_t *cl);

#endif
