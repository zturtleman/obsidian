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
    o_unlag : Backwards Reconciliation
*/

#include "doomdef.h"
#include "doomstat.h"
#include "r_defs.h"
#include "r_main.h"
#include "p_local.h"

#include "o_common.h"
#include "o_unlag.h"

int numsectors;
sector_t *sectors;

void SV_ULRecordPos (void)
{
	int i;
	player_t *pl;

	// Record player positions.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		pl = &players[i];
		if (playeringame[i] && pl->mo)
		{
			ul_playerpos[i][gametic % UL_MAXTICS].x = pl->mo->x;
			ul_playerpos[i][gametic % UL_MAXTICS].y = pl->mo->y;
			ul_playerpos[i][gametic % UL_MAXTICS].z = pl->mo->z;
			ul_playerpos[i][gametic % UL_MAXTICS].floorz = pl->mo->floorz;
			ul_playerpos[i][gametic % UL_MAXTICS].ceilingz = pl->mo->ceilingz;
		}
	}

	for (i = 0; i < numsectors; i++)
	{
		sectors[i].ul_floorheight[gametic % UL_MAXTICS] = sectors[i].floorheight;
		sectors[i].ul_ceilingheight[gametic % UL_MAXTICS] = sectors[i].ceilingheight;
	}

	return;
}

// Reconciles everything back to a specified gametic, as long as that gametic is within UL_MAXTICS of the current one
void SV_ULReconcile (int tic, player_t *exclude)
{
	int i;
	player_t *pl;
	subsector_t *dest;

	if (tic < gametic - UL_MAXTICS) // Out of range
		return;

	// Record positions for this tic, since it hasn't been done yet.
	SV_ULRecordPos ();

	for (i = 0; i < numsectors; i++)
	{
		sectors[i].floorheight = sectors[i].ul_floorheight[tic % UL_MAXTICS];
		sectors[i].ceilingheight = sectors[i].ul_ceilingheight[tic % UL_MAXTICS];
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		pl = &players[i];
		if (playeringame[i] && pl->mo && pl != exclude)
		{
			P_UnsetThingPosition (pl->mo);
			pl->mo->x = ul_playerpos[i][tic % UL_MAXTICS].x;
			pl->mo->y = ul_playerpos[i][tic % UL_MAXTICS].y;
			pl->mo->z = ul_playerpos[i][tic % UL_MAXTICS].z;
			
			dest = R_PointInSubsector (pl->mo->x, pl->mo->y);
			pl->mo->subsector = dest;
			pl->mo->floorz = ul_playerpos[i][tic % UL_MAXTICS].floorz;
			pl->mo->ceilingz = ul_playerpos[i][tic % UL_MAXTICS].ceilingz;
			P_SetThingPosition (pl->mo);
		}
	}

	return;
}	
