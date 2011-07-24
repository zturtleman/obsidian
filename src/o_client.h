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
    o_client : Obsidian Client
*/

#ifndef CLIENT_H
#define CLIENT_H

// GhostlyDeath <March 27, 2011> -- Lean and mean for Windows (since ENet does not use it)
#define WIN32_LEAN_AND_MEAN

#include "enet/enet.h"
#include "o_common.h"

ENetHost *localclient;
int localid;
uint16_t inGameMask;
extern boolean firefromsrv;

void CL_Connect (char *srv_hn);
void CL_Loop(void);
void CL_ParsePacket(ENetPacket *pk);

void CL_SendPosUpdate(fixed_t x, fixed_t y, fixed_t z, fixed_t ang, fixed_t momx, fixed_t momy, fixed_t momz);
void CL_SendUseCmd(void);
void CL_SendStateUpdate(uint16_t state);
void CL_SendFireCmd(weapontype_t w, int refire);
void CL_SendRespawn(int startnum);
void CL_SendString(messagetype_e type, char *sending);
void CL_FireWeapon (player_t *pl, weapontype_t weapon);
void CL_ReadSectorBuffer (char *secbuf);

#endif
