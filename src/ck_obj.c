/*
Omnispeak: A Commander Keen Reimplementation
Copyright (C) 2012 David Gow <david@ingeniumdigital.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <stdlib.h>

#include "id_heads.h"
#include "id_ca.h"
#include "id_rf.h"
#include "id_ti.h"
#include "id_us.h"
#include "ck_act.h"
#include "ck_def.h"
#include "ck_phys.h"
#include "ck_play.h"
#include "ap_hooks.h"

// This file contains some object functions (think, etc) which are common to
// several episodes.
// AFAIK, There never was a "ck_obj.c" in the DOS source

// The maximum height of a gem door. Note that this is the height of the
// _full_ door, including the two ground tiles, not just the main portion
// stored in user1.
#define CK_MAX_DOOR_HEIGHT 50

void CK_DoorOpen(CK_object *obj)
{
	uint16_t tilesToReplace[CK_MAX_DOOR_HEIGHT];

	if (obj->user1 + 2 > CK_MAX_DOOR_HEIGHT)
	{
		Quit("Door too tall!");
	}

	for (int i = 0; i < obj->user1 + 2; ++i)
	{
		tilesToReplace[i] = CA_TileAtPos(obj->posX, obj->posY + i, 1) + 1;
	}

	RF_ReplaceTiles(tilesToReplace, 1, obj->posX, obj->posY, 1, obj->user1 + 2);
}

#ifdef WITH_KEEN5
void CK_SecurityDoorOpen(CK_object *obj)
{
	uint16_t tilesToReplace[0x30];
	for (int y = 0; y < 4; ++y)
	{
		for (int x = 0; x < 4; ++x)
		{
			tilesToReplace[y * 4 + x] = CA_TileAtPos(obj->posX + x, obj->posY + y, 1) - 4;
		}
	}

	RF_ReplaceTiles(tilesToReplace, 1, obj->posX, obj->posY, 4, 4);
	obj->user1++;
	if (obj->user1 == 3)
	{
		obj->currentAction = 0;
	}
}
#endif

chunk_id_t CK_ItemSpriteChunks[] = {
	CK_CHUNKID(SPR_GEM_A1),
	CK_CHUNKID(SPR_GEM_B1),
	CK_CHUNKID(SPR_GEM_C1),
	CK_CHUNKID(SPR_GEM_D1),
	CK_CHUNKID(SPR_100_PTS1),
	CK_CHUNKID(SPR_200_PTS1),
	CK_CHUNKID(SPR_500_PTS1),
	CK_CHUNKID(SPR_1000_PTS1),
	CK_CHUNKID(SPR_2000_PTS1),
	CK_CHUNKID(SPR_5000_PTS1),
	CK_CHUNKID(SPR_1UP1),
	CK_CHUNKID(SPR_STUNNER1),
#ifdef WITH_KEEN5
	CK_CHUNKID(SPR_SECURITYCARD_1)
#endif
};

// Extra-life pickups (item index 10 from info layer, OR foreground tile
// misc=27). Visually a Vitalin Keg in CK5, a Lifewater Flask in CK4. The
// per-level index spans both spawn paths: both info-layer items (handled
// in CK_SpawnItem) and tile-layer items (handled in ap_scan_tile_extralives)
// record their index in the shared tile table, keyed by (tileX, tileY).
// obj->user4 must not be used — it is part of the engine's object dump
// and any write to it would break the demo-regression tests.
#define AP_TILE_EXTRALIFE_MAX 64

static int ap_extralife_count_in_level = 0;

typedef struct
{
	int tileX;
	int tileY;
	int index;
} ap_tile_extralife_t;

static ap_tile_extralife_t ap_tile_extralives[AP_TILE_EXTRALIFE_MAX];
static int ap_tile_extralife_count = 0;

// Pointsanity scaffolding. Mirrors the extralife setup but keyed by point
// class (0..5 for 100/200/500/1000/2000/5000 pt items). Info-layer items
// (item index 4..9) get obj->user4 = ap_pointitem_count_in_level[class]++
// inside CK_SpawnItem; tile-layer items (foreground tile misc 21..26) get
// a continuation index assigned by ap_scan_tile_pointitems() and looked up
// by (x, y) in CK_KeenGetTileItem.
#define AP_TILE_POINTITEM_MAX 256

static int ap_pointitem_count_in_level[6] = {0, 0, 0, 0, 0, 0};

typedef struct
{
	int tileX;
	int tileY;
	int cls;
	int index;
} ap_tile_pointitem_t;

static ap_tile_pointitem_t ap_tile_pointitems[AP_TILE_POINTITEM_MAX];
static int ap_tile_pointitem_count = 0;

void ap_reset_extralife_counter(void)
{
	ap_extralife_count_in_level = 0;
	ap_tile_extralife_count = 0;

	// Mark each level entry in the dump so even zero-extralife levels
	// appear, confirming the dumper is active.
	if (getenv("OMNISPEAK_DUMP_SCORE_ITEMS"))
	{
		FILE *f = fopen("score_item_dump.txt", "a");
		if (f)
		{
			fprintf(f, "# level-enter ep=%d lvl=%d\n",
			        ap_current_episode, ap_current_level);
			fclose(f);
		}
	}
}

void ap_reset_pointitem_counters(void)
{
	for (int i = 0; i < 6; i++)
		ap_pointitem_count_in_level[i] = 0;
	ap_tile_pointitem_count = 0;
}

void ap_scan_tile_extralives(void)
{
	// Do NOT reset ap_tile_extralife_count here — info-layer items were
	// already added by CK_SpawnItem during scanInfoLayer and live at the
	// start of the table. This scan appends tile-layer items after them.
	// Both lifecycle resets happen up front in ap_reset_extralife_counter.

	FILE *f = getenv("OMNISPEAK_DUMP_SCORE_ITEMS")
	    ? fopen("score_item_dump.txt", "a")
	    : NULL;

	int w = CA_GetMapWidth();
	int h = CA_GetMapHeight();
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			int misc = TI_ForeMisc(CA_TileAtPos(x, y, 1)) & 0x7F;
			if (misc != 27)
				continue;

			int idx = ap_extralife_count_in_level++;
			if (ap_tile_extralife_count < AP_TILE_EXTRALIFE_MAX)
			{
				ap_tile_extralives[ap_tile_extralife_count].tileX = x;
				ap_tile_extralives[ap_tile_extralife_count].tileY = y;
				ap_tile_extralives[ap_tile_extralife_count].index = idx;
				ap_tile_extralife_count++;
			}

			if (f)
			{
				fprintf(f, "ep=%d lvl=%d item=extralife idx=%d tile=(%d,%d) source=tile\n",
				        ap_current_episode, ap_current_level, idx, x, y);
			}
		}
	}

	if (f)
		fclose(f);
}

int ap_lookup_tile_extralife(int tileX, int tileY)
{
	for (int i = 0; i < ap_tile_extralife_count; i++)
	{
		if (ap_tile_extralives[i].tileX == tileX
		    && ap_tile_extralives[i].tileY == tileY)
			return ap_tile_extralives[i].index;
	}
	return -1;
}

void ap_scan_tile_pointitems(void)
{
	// Do NOT reset ap_tile_pointitem_count here — info-layer items were
	// already added by CK_SpawnItem during scanInfoLayer and live at the
	// start of the table. This scan appends tile-layer items after them.
	// Both lifecycle resets happen up front in ap_reset_pointitem_counters.

	FILE *f = getenv("OMNISPEAK_DUMP_SCORE_ITEMS")
	    ? fopen("score_item_dump.txt", "a")
	    : NULL;

	int w = CA_GetMapWidth();
	int h = CA_GetMapHeight();
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			int misc = TI_ForeMisc(CA_TileAtPos(x, y, 1)) & 0x7F;
			// MF_Points100..MF_Points5000 are misc 21..26, mapping to
			// classes 0..5 respectively.
			if (misc < 21 || misc > 26)
				continue;

			int cls = misc - 21;
			int idx = ap_pointitem_count_in_level[cls]++;
			if (ap_tile_pointitem_count < AP_TILE_POINTITEM_MAX)
			{
				ap_tile_pointitems[ap_tile_pointitem_count].tileX = x;
				ap_tile_pointitems[ap_tile_pointitem_count].tileY = y;
				ap_tile_pointitems[ap_tile_pointitem_count].cls = cls;
				ap_tile_pointitems[ap_tile_pointitem_count].index = idx;
				ap_tile_pointitem_count++;
			}

			if (f)
			{
				fprintf(f, "ep=%d lvl=%d item=points cls=%d idx=%d tile=(%d,%d) source=tile\n",
				        ap_current_episode, ap_current_level, cls, idx, x, y);
			}
		}
	}

	if (f)
		fclose(f);
}

bool ap_lookup_tile_pointitem(int tileX, int tileY, int *out_class, int *out_index)
{
	for (int i = 0; i < ap_tile_pointitem_count; i++)
	{
		if (ap_tile_pointitems[i].tileX == tileX
		    && ap_tile_pointitems[i].tileY == tileY)
		{
			if (out_class) *out_class = ap_tile_pointitems[i].cls;
			if (out_index) *out_index = ap_tile_pointitems[i].index;
			return true;
		}
	}
	return false;
}

// Object and Centilife functions "should" be in ckx_obj1.c
// but they are similar enough between episodes to put them all here
void CK_SpawnItem(int tileX, int tileY, int itemNumber)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->clipped = CLIP_not;
	//obj->active = OBJ_ACTIVE;
	obj->zLayer = 2;
	obj->type = CT_CLASS(Item);
	obj->posX = RF_TileToUnit(tileX);
	obj->posY = RF_TileToUnit(tileY);
	obj->yDirection = -1;
	obj->user1 = itemNumber;
	obj->gfxChunk = CK_LookupChunk(CK_ItemSpriteChunks[itemNumber]);
	obj->user2 = obj->gfxChunk;
	obj->user3 = obj->gfxChunk + 2;
	obj->user4 = 0;

	// Info-layer extra lives (Vitalin Keg in CK5, Lifewater Flask in CK4)
	// get a deterministic per-level index recorded in the shared tile
	// table — keyed by (tileX, tileY), not by obj->user4. obj->user4 is
	// part of the engine's object dump and any write to it would break
	// the demo-regression tests. The tile-layer scan that follows
	// continues from the same counter via ap_scan_tile_extralives().
	if (itemNumber == 10)
	{
		int idx = ap_extralife_count_in_level++;
		if (ap_tile_extralife_count < AP_TILE_EXTRALIFE_MAX)
		{
			ap_tile_extralives[ap_tile_extralife_count].tileX = tileX;
			ap_tile_extralives[ap_tile_extralife_count].tileY = tileY;
			ap_tile_extralives[ap_tile_extralife_count].index = idx;
			ap_tile_extralife_count++;
		}
		if (getenv("OMNISPEAK_DUMP_SCORE_ITEMS"))
		{
			FILE *f = fopen("score_item_dump.txt", "a");
			if (f)
			{
				fprintf(f, "ep=%d lvl=%d item=extralife idx=%d tile=(%d,%d) source=info\n",
				        ap_current_episode, ap_current_level,
				        idx, tileX, tileY);
				fclose(f);
			}
		}
	}
	// Info-layer point items (100..5000 pt) get a per-class, per-level
	// index recorded in the shared tile table — keyed by (tileX, tileY),
	// not by obj->user4. obj->user4 is part of the engine's object dump
	// and any write to it would break the demo-regression tests. The
	// tile-layer scan that follows continues from the same per-class
	// counter via ap_scan_tile_pointitems().
	else if (itemNumber >= 4 && itemNumber <= 9)
	{
		int cls = itemNumber - 4;
		int idx = ap_pointitem_count_in_level[cls]++;
		if (ap_tile_pointitem_count < AP_TILE_POINTITEM_MAX)
		{
			ap_tile_pointitems[ap_tile_pointitem_count].tileX = tileX;
			ap_tile_pointitems[ap_tile_pointitem_count].tileY = tileY;
			ap_tile_pointitems[ap_tile_pointitem_count].cls = cls;
			ap_tile_pointitems[ap_tile_pointitem_count].index = idx;
			ap_tile_pointitem_count++;
		}
		if (getenv("OMNISPEAK_DUMP_SCORE_ITEMS"))
		{
			FILE *f = fopen("score_item_dump.txt", "a");
			if (f)
			{
				fprintf(f, "ep=%d lvl=%d item=points cls=%d idx=%d tile=(%d,%d) source=info\n",
				        ap_current_episode, ap_current_level,
				        cls, idx, tileX, tileY);
				fclose(f);
			}
		}
	}

	CK_SetAction(obj, CK_ACTION(CK_ACT_item));
}

void CK_SpawnCentilifeNotify(int tileX, int tileY)
{
	CK_object *notify = CK_GetNewObj(true);
	notify->type = 1;
	notify->clipped = CLIP_not;
	notify->zLayer = 3;
	notify->posX = RF_TileToUnit(tileX);
	notify->posY = RF_TileToUnit(tileY);

	CK_SetAction(notify, CK_ACTION(CK_ACT_CentilifeNotify1));
}

void CK_PointItem(CK_object *obj)
{
	//	obj->timeUntillThink = 20;
	obj->visible = true;
	if (++obj->gfxChunk == obj->user3)
		obj->gfxChunk = obj->user2;
}

// Keen 6 specific (but also appears to be present in the Keen 5 EXE)
void CK_FallingItem(CK_object *obj)
{
	if (obj->topTI)
		CK_SetAction(obj, CK_ACTION(CK_ACT_item));
	if (++obj->gfxChunk == obj->user3)
		obj->gfxChunk = obj->user2;
	CK_PhysGravityHigh(obj);
}

// Platforms

// CK4: ck4_obj2.c
// CK5: ck5_obj?.c - This is the RED axis platform
// CK6  ck_obj1.c

// Note that Keen 5 was the only keen to have the purple parameter, but
// it is added here for the other Keens in order to reuse code
void CK_SpawnAxisPlatform(int tileX, int tileY, int direction, bool purple)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->type = CT_CLASS(Platform);
	obj->active = OBJ_ALWAYS_ACTIVE;
	obj->zLayer = 0;
	obj->posX = RF_TileToUnit(tileX);
	obj->posY = RF_TileToUnit(tileY);

	switch (direction)
	{
	case 0:
		obj->xDirection = 0;
		obj->yDirection = -1;
		break;
	case 1:
		obj->xDirection = 1;
		obj->yDirection = 0;
		break;
	case 2:
		obj->xDirection = 0;
		obj->yDirection = 1;
		break;
	case 3:
		obj->xDirection = -1;
		obj->yDirection = 0;
		break;
	}

#ifdef WITH_KEEN5
	if (purple)
	{
		obj->posX += 0x40;
		obj->posY += 0x40;
		CK_SetAction(obj, CK_ACTION(CK5_ACT_purpleAxisPlatform));
	}
	else
#endif
	{

		CK_SetAction(obj, CK_ACTION(CK_ACT_AxisPlatform));
	}
	// TODO: These should *not* be done here.
	obj->gfxChunk = obj->currentAction->chunkLeft;
	CA_CacheGrChunk(obj->gfxChunk);
	// CK_ResetClipRects(obj);
}

void CK_AxisPlatform(CK_object *obj)
{
	uint16_t nextPosUnit, nextPosTile;

	if (ck_nextX || ck_nextY)
	{
		return;
	}
	//TODO: Implement properly.
	ck_nextX = obj->xDirection * 12 * SD_GetSpriteSync();
	ck_nextY = obj->yDirection * 12 * SD_GetSpriteSync();

	if (obj->xDirection == 1)
	{
		nextPosUnit = obj->clipRects.unitX2 + ck_nextX;
		nextPosTile = RF_UnitToTile(nextPosUnit);
		if (obj->clipRects.tileX2 != nextPosTile && CA_TileAtPos(nextPosTile, obj->clipRects.tileY1, 2) == 0x1F)
		{
			obj->xDirection = -1;
			//TODO: Change DeltaVelocity
			ck_nextX -= (nextPosUnit & 255);
		}
	}
	else if (obj->xDirection == -1)
	{
		nextPosUnit = obj->clipRects.unitX1 + ck_nextX;
		nextPosTile = RF_UnitToTile(nextPosUnit);
		if (obj->clipRects.tileX1 != nextPosTile && CA_TileAtPos(nextPosTile, obj->clipRects.tileY1, 2) == 0x1F)
		{
			obj->xDirection = 1;
			//TODO: Change DeltaVelocity
			//CK_PhysUpdateX(obj, 256 - nextPosUnit&255);
			ck_nextX += (256 - nextPosUnit) & 255;
		}
	}
	else if (obj->yDirection == 1)
	{
		nextPosUnit = obj->clipRects.unitY2 + ck_nextY;
		nextPosTile = RF_UnitToTile(nextPosUnit);
		if (obj->clipRects.tileY2 != nextPosTile && CA_TileAtPos(obj->clipRects.tileX1, nextPosTile, 2) == 0x1F)
		{
			if (CA_TileAtPos(obj->clipRects.tileX1, nextPosTile - 2, 2) == 0x1F)
			{
				//Stop the platform.
				obj->visible = true;
				ck_nextY = 0;
			}
			else
			{
				obj->yDirection = -1;
				//TODO: Change DeltaVelocity
				ck_nextY -= (nextPosUnit & 255);
			}
		}
	}
	else if (obj->yDirection == -1)
	{
		nextPosUnit = obj->clipRects.unitY1 + ck_nextY;
		nextPosTile = RF_UnitToTile(nextPosUnit);
		if (obj->clipRects.tileY1 != nextPosTile && CA_TileAtPos(obj->clipRects.tileX1, nextPosTile, 2) == 0x1F)
		{
			if (CA_TileAtPos(obj->clipRects.tileX1, nextPosTile + 2, 2) == 0x1F)
			{
				// Stop the platform.
				obj->visible = true;
				ck_nextY = 0;
			}
			else
			{
				obj->yDirection = 1;
				//TODO: Change DeltaVelocity
				ck_nextY += 256 - (nextPosUnit & 255);
			}
		}
	}
}

void CK_SpawnFallPlat(int tileX, int tileY)
{
	CK_object *new_object = CK_GetNewObj(false);
	new_object->type = CT_CLASS(Platform);
	new_object->active = OBJ_ALWAYS_ACTIVE;
	new_object->zLayer = 0;
	new_object->posX = RF_TileToUnit(tileX);
	new_object->user1 = new_object->posY = RF_TileToUnit(tileY);
	new_object->xDirection = IN_motion_None;
	new_object->yDirection = IN_motion_Down;
	new_object->clipped = CLIP_not;
	CK_SetAction(new_object, CK_ACTION(CK_ACT_FallPlat0));
}

void CK_FallPlatSit(CK_object *obj)
{

	if (obj == ck_keenState.platform)
	{
		ck_nextY = SD_GetSpriteSync() * 16;
		obj->velY = 0;
		if ((unsigned)(obj->posY + ck_nextY - obj->user1) >= 0x80)
			obj->currentAction = CK_ACTION(CK_ACT_FallPlat1);
	}
}

void CK_FallPlatFall(CK_object *obj)
{
	uint16_t newY, newYT;

	CK_PhysGravityHigh(obj);
	newY = obj->clipRects.unitY2 + ck_nextY;
	newYT = RF_UnitToTile(newY);

	// Stop falling if platform hits a block
	if ((obj->clipRects.tileY2 != newYT) && (CA_TileAtPos(obj->clipRects.tileX1, newYT, 2) == 0x1F))
	{
		ck_nextY = 0xFF - (obj->clipRects.unitY2 & 0xFF);
		if (ck_keenState.platform != obj)
			obj->currentAction = CK_ACTION(CK_ACT_FallPlat2);
	}
}

void CK_FallPlatRise(CK_object *obj)
{
	if (ck_keenState.platform == obj)
	{
		obj->velY = 0;
		obj->currentAction = CK_ACTION(CK_ACT_FallPlat1);
	}
	else if ((unsigned)obj->posY <= (unsigned)obj->user1)
	{
		ck_nextY = obj->user1 - obj->posY;
		obj->currentAction = CK_ACTION(CK_ACT_FallPlat0);
	}
}

#if defined(WITH_KEEN5) || defined(WITH_KEEN6)

void CK_SpawnStandPlatform(int tileX, int tileY)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->type = CT_CLASS(Platform);
	obj->active = OBJ_ACTIVE;
	obj->zLayer = 0;
	obj->posX = RF_TileToUnit(tileX);
	obj->posY = obj->user1 = RF_TileToUnit(tileY);
	obj->xDirection = 0;
	obj->yDirection = 1;
	obj->clipped = CLIP_not;
	CK_SetAction(obj, CK_ACTION(CK_ACT_StandPlatform));
	obj->gfxChunk = obj->currentAction->chunkLeft;
	CA_CacheGrChunk(obj->gfxChunk);
}

/*
 * Spawn a GoPlat
 */
void CK_SpawnGoPlat(int tileX, int tileY, int direction, bool purple)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->type = CT_CLASS(Platform);
	obj->active = OBJ_ALWAYS_ACTIVE;
	obj->zLayer = 0;
	obj->posX = RF_TileToUnit(tileX);
	obj->posY = RF_TileToUnit(tileY);
	obj->xDirection = IN_motion_None;
	obj->yDirection = IN_motion_Down;
	obj->clipped = CLIP_not;

#ifdef WITH_KEEN5
	if (purple)
	{
		obj->posX += 0x40;
		obj->posY += 0x40;
		CK_SetAction(obj, CK_ACTION(CK5_ACT_purpleGoPlat));
	}
	else
#endif
	{
		CK_SetAction(obj, CK_ACTION(CK_ACT_GoPlat0));
	}

	CA_SetTileAtPos(tileX, tileY, 2, direction + 0x5B);

	obj->user1 = direction;
	obj->user2 = 256;
}


int ck_infoplaneArrowsX[8] = {0, 1, 0, -1, 1, 1, -1, -1};
int ck_infoplaneArrowsY[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

void CK_GoPlatThink(CK_object *obj)
{

	if (ck_nextX || ck_nextY)
		return;

	int16_t delta = SD_GetSpriteSync() * 12;

	// Will we reach a new tile?
	if (obj->user2 > delta)
	{
		// No... keep moving in the same direction.
		obj->user2 -= delta;

		int dirX = ck_infoplaneArrowsX[obj->user1];
		if (dirX == 1)
		{
			// Moving right.
			ck_nextX += delta;
		}
		else if (dirX == -1)
		{
			// Moving left
			ck_nextX -= delta;
		}

		int dirY = ck_infoplaneArrowsY[obj->user1];
		if (dirY == 1)
		{
			// Moving down
			ck_nextY += delta;
		}
		else if (dirY == -1)
		{
			// Moving up
			ck_nextY -= delta;
		}
	}
	else
	{
		// Move to next tile.
		int dirX = ck_infoplaneArrowsX[obj->user1];
		if (dirX == 1)
		{
			// Moving right.
			ck_nextX += obj->user2;
		}
		else if (dirX == -1)
		{
			// Moving left
			ck_nextX -= obj->user2;
		}

		int dirY = ck_infoplaneArrowsY[obj->user1];
		if (dirY == 1)
		{
			// Moving down
			ck_nextY += obj->user2;
		}
		else if (dirY == -1)
		{
			// Moving up
			ck_nextY -= obj->user2;
		}

		int tileX = RF_UnitToTile((uint16_t)(obj->posX + ck_nextX));
		int tileY = RF_UnitToTile((uint16_t)(obj->posY + ck_nextY));

		obj->user1 = CA_TileAtPos(tileX, tileY, 2) - 0x5B;

		if ((obj->user1 < 0) || (obj->user1 > 8))
		{
			Quit("Goplat moved to a bad spot.");
		}

		delta -= obj->user2;
		obj->user2 = 256 - delta;

		// Move in the new direction.
		dirX = ck_infoplaneArrowsX[obj->user1];
		if (dirX == 1)
		{
			// Moving right.
			ck_nextX += delta;
		}
		else if (dirX == -1)
		{
			// Moving left
			ck_nextX -= delta;
		}

		dirY = ck_infoplaneArrowsY[obj->user1];
		if (dirY == 1)
		{
			// Moving down
			ck_nextY += delta;
		}
		else if (dirY == -1)
		{
			// Moving up
			ck_nextY -= delta;
		}
	}
}
/*
 * Spawn a "Sneak Plat".
 */
void CK_SneakPlatSpawn(int tileX, int tileY)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->type = CT_CLASS(Platform);
	obj->active = OBJ_ALWAYS_ACTIVE;
	obj->zLayer = 0;
	obj->user1 = obj->posX = RF_TileToUnit(tileX);
	obj->posY = RF_TileToUnit(tileY);
	obj->xDirection = 0;
	obj->yDirection = 1;
	obj->clipped = CLIP_not;

	CK_SetAction(obj, CK_ACTION(CK_ACT_sneakPlatWait));
	CA_CacheGrChunk(obj->gfxChunk);
}

void CK_SneakPlatThink(CK_object *obj)
{
	if (ck_keenObj->currentAction == CK_ACTION(CK_ACT_keenJump1))
	{
		if (ck_keenObj->xDirection == 1)
		{
			int dist = obj->clipRects.unitX1 - ck_keenObj->clipRects.unitX2;
			if (dist > 0x400 || dist < 0)
				return;
		}
		else
		{
			int dist = ck_keenObj->clipRects.unitX1 - obj->clipRects.unitX2;
			if (dist > 0x400 || dist < 0)
				return;
		}

		int vertDist = ck_keenObj->posY - obj->posY;
		if (vertDist < -0x600 || vertDist > 0x600)
			return;

		obj->xDirection = ck_keenObj->xDirection;
		obj->currentAction = CK_ACTION(CK_ACT_sneakPlatSneak);
	}
}

// TURRETS

void CK_TurretSpawn(int tileX, int tileY, int direction)
{
	CK_object *obj = CK_GetNewObj(false);

	obj->type = CT_CLASS(Turret);
	obj->active = OBJ_ACTIVE;
	obj->clipRects.tileX1 = obj->clipRects.tileX2 = tileX;
	obj->clipRects.tileY1 = obj->clipRects.tileY2 = tileY;

	obj->posX = RF_TileToUnit(tileX);
	obj->posY = RF_TileToUnit(tileY);
	obj->clipRects.unitX1 = obj->clipRects.unitX2 = RF_TileToUnit(tileX);
	obj->clipRects.unitY1 = obj->clipRects.unitY2 = RF_TileToUnit(tileY);

	obj->user1 = direction;

	CK_SetAction(obj, CK_ACTION(CK_ACT_turretWait));
}

void CK_TurretShoot(CK_object *obj)
{
	CK_object *shot = CK_GetNewObj(true);

	shot->type = CT5_EnemyShot; //TurretShot
	if (CFG_GetConfigBool("ck_turretShotsPersist", CK_INT(ck_turretShotsPersist, ck_currentEpisode->ep == EP_CK6)))
		shot->active = OBJ_ACTIVE;
	else
		shot->active = OBJ_EXISTS_ONLY_ONSCREEN;
	shot->clipped = CLIP_normal;
	shot->posX = obj->posX;
	shot->posY = obj->posY;

	switch (obj->user1)
	{
	case 0:
		shot->velY = -64;
		break;
	case 1:
		shot->velX = 64;
		break;
	case 2:
		shot->velY = 64;
		break;
	case 3:
		shot->velX = -64;
		break;
	}

	CK_SetAction(shot, CK_ACTION(CK_ACT_turretShot1));
	SD_PlaySound(CK_SOUNDNUM(SOUND_ENEMYSHOOT));
}

void CK_TurretShotCol(CK_object *me, CK_object *other)
{
	if (other->type == CT_Player)
	{
		CK_KillKeen();
		CK_SetAction2(me, CK_ACTION(CK_ACT_turretShotHit1));
	}
}

void CK_TurretShotDraw(CK_object *obj)
{
	if (obj->topTI || obj->bottomTI || obj->leftTI || obj->rightTI)
	{
		SD_PlaySound(CK_SOUNDNUM(SOUND_ENEMYSHOTHIT));
		//obj->clipped=false;
		CK_SetAction2(obj, CK_ACTION(CK_ACT_turretShotHit1));
	}

	RF_AddSpriteDraw(&(obj->sde), obj->posX, obj->posY, obj->currentAction->chunkLeft, false, obj->zLayer);
}
#endif
void CK_OBJ_SetupFunctions()
{
	CK_ACT_AddFunction("CK_DoorOpen", &CK_DoorOpen);
#ifdef WITH_KEEN5
	CK_ACT_AddFunction("CK_SecurityDoorOpen", &CK_SecurityDoorOpen);
#endif
	CK_ACT_AddFunction("CK_PointItem", &CK_PointItem);
	CK_ACT_AddFunction("CK_FallingItem", &CK_FallingItem);

	CK_ACT_AddFunction("CK_AxisPlatform", &CK_AxisPlatform);

	CK_ACT_AddFunction("CK_FallPlatSit", &CK_FallPlatSit);
	CK_ACT_AddFunction("CK_FallPlatFall", &CK_FallPlatFall);
	CK_ACT_AddFunction("CK_FallPlatRise", &CK_FallPlatRise);
#if defined(WITH_KEEN5) || defined(WITH_KEEN6)
	CK_ACT_AddFunction("CK_GoPlatThink", &CK_GoPlatThink);
	CK_ACT_AddFunction("CK_SneakPlatThink", &CK_SneakPlatThink);

	CK_ACT_AddFunction("CK_TurretShoot", &CK_TurretShoot);
	CK_ACT_AddColFunction("CK_TurretShotCol", &CK_TurretShotCol);
	CK_ACT_AddFunction("CK_TurretShotDraw", &CK_TurretShotDraw);
#endif
}
