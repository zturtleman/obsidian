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
int SV_Main(void);
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
void SV_SendFire (player_t *player, weapontype_t weapon);
void SV_DamageMobj(mobj_t *target, mobj_t *source, int damage);
void SV_KillMobj(mobj_t *source, mobj_t *target);
void SV_CheckFrags (mobj_t *mo);
int SV_ClientNumForPeer(ENetPeer *p);
void SV_SendString (messagetype_e type, const char *sending, int exclude, boolean sound);
void SV_SendExit (boolean secret);
void *SV_MakeSectorBuffer(void);
void SV_SpawnMobj (mobj_t *mo);

// Variables

enum {
CT_EMPTY,
CT_CONNECT,
CT_ACTIVE
};

ENetAddress addr;
ENetHost *srv;

typedef struct
{
	int type;
	int id;
	ENetPeer *peer;
	player_t *player;
	char nick[MAXPLAYERNAME];
	int damage[MAXPLAYERS + 1];
} client_t;

client_t clients[MAXPLAYERS];
void SV_ClientWelcome (client_t *cl);

#endif
