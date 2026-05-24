#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ap_hooks.h"
#include "ap_defs.h"
#include "ap_client.h"
#include "id_sd.h"
#include "id_ca.h"
#include "id_us.h"
#include "id_vh.h"
#include "id_in.h"
#include "ck_def.h"
#include "ck_play.h"

int ap_current_level = -1;
int ap_current_episode = -1;
int ap_starting_points = 0;
int ap_points_gained = 0;
bool ap_has_pogo = 0;
bool ap_has_stunner = 0;
bool ap_force_abort = 0;

bool ap_death_link_enabled = false;
bool ap_pending_death = false;
bool ap_suppress_death_send = false;

void ap_on_level_complete(void)
{
	int location_id = LOC_LEVEL_COMPLETE(ap_current_episode, ap_current_level);
	ap_client_location_check(location_id);

	// Goal completion is tracked locally — the server's "checked locations"
	// set can be polluted by !collect / auto-collect when other players
	// goal, which would otherwise trigger a false victory.
	if (ap_current_episode == AP_EPISODE_CK4 && ap_current_level == AP_LEVEL_BEAN_WITH_BACON_MEGAROCKET)
		ap_mark_boss_complete(AP_EPISODE_CK4);
	else if (ap_current_episode == AP_EPISODE_CK5 && ap_current_level == AP_LEVEL_QUANTUM_EXPLOSION_DYNAMO)
		ap_mark_boss_complete(AP_EPISODE_CK5);

	ap_announce_victory();
}

void ap_on_keygem_get(int item)
{
	int location_id = LOC_KEYGEM(ap_current_episode, ap_current_level, item);
	ap_client_location_check(location_id);
}

void ap_on_security_card_get(void)
{
	int location_id = LOC_SECURITY_KEYCARD(ap_current_episode, ap_current_level);
	ap_client_location_check(location_id);

}

void ap_on_extralife_get(int extralife_idx)
{
	// CK4 extra lives (Lifewater Flasks) use the FLASK base, CK5 extra
	// lives (Vitalin Kegs) use the KEG base. The base picks itself from
	// ap_current_episode.
	int location_id;
	if (ap_current_episode == AP_EPISODE_CK4)
		location_id = LOC_FLASK(AP_EPISODE_CK4, ap_current_level, extralife_idx);
	else if (ap_current_episode == AP_EPISODE_CK5)
		location_id = LOC_KEG(AP_EPISODE_CK5, ap_current_level, extralife_idx);
	else
		return;
	ap_client_location_check(location_id);
}

// Per-class enable for pointsanity. Flip an entry to true to start emitting
// location checks for that point class. Coordinate flips with an apworld
// release that declares the matching LOC_POINTSANITY locations.
static const bool ap_pointsanity_class_enabled[6] = {
	false,  // class 0: 100 pt
	false,  // class 1: 200 pt
	false,  // class 2: 500 pt
	false,  // class 3: 1000 pt
	false,  // class 4: 2000 pt
	true,   // class 5: 5000 pt
};

void ap_on_pointitem_get(int point_class, int instance_index)
{
	if (point_class < 0 || point_class > 5)
		return;
	if (!ap_pointsanity_class_enabled[point_class])
		return;
	// Upper bound matches AP_POINTSANITY_LEVEL_STRIDE; exceeding it would
	// collide with the next level's instance 0 in the apworld's decode.
	// Real maps cap out around 124, well under the 1000-slot stride.
	if (instance_index < 0 || instance_index >= AP_POINTSANITY_LEVEL_STRIDE)
		return;
	// Match the extralife hook: only emit for episodes the apworld supports.
	if (ap_current_episode != AP_EPISODE_CK4 && ap_current_episode != AP_EPISODE_CK5)
		return;

	int location_id = LOC_POINTSANITY(
		point_class, ap_current_episode, ap_current_level,
		instance_index);
	ap_client_location_check(location_id);
	// No in-game toast here — matches ap_on_extralife_get. The standard
	// "Item received" toast that fires when the server sends an item back
	// is enough feedback; an unconditional "Found N pt pickup" toast would
	// also fire on slots that don't have pointsanity enabled (engine has
	// no slot_data awareness for the per-class enable), confusing players.
}

void ap_on_score_increase(int score)
{
	ap_points_gained = score - ap_starting_points;
}

static const char *ap_death_msgs_generic[] = {
	"Commander Keen was hit",
	"Commander Keen forgot to dodge",
	"Commander Keen got Slugged",
	"Commander Keen's helmet wasn't on tight enough",
	"Commander Keen took a Wormouth to the face",
	"Commander Keen ran out of lifewater",
	"Commander Keen's mom is going to be mad",
	"Billy Blaze got grounded for a week",
	"Commander Keen learned not to touch the spiky things",
	"Commander Keen's eight-year-old genius failed him",
	"Commander Keen got a little too curious",
	"Commander Keen needed more pickles",
};

static const char *ap_death_msgs_eaten[] = {
	"Commander Keen got Dopefish'd",
	"Commander Keen was today's special",
	"Commander Keen learned why Dopefish is so happy",
	"Commander Keen swam too close to the buffet",
	"Commander Keen forgot fish gotta eat too",
};

static const char *ap_death_msgs_fell[] = {
	"Commander Keen took the express elevator down",
	"Commander Keen discovered gravity, the hard way",
	"Commander Keen failed his orbital re-entry",
	"Commander Keen jumped one time too many",
	"Commander Keen forgot to pack a parachute",
};

const char* ap_random_death_message(int kind)
{
	const char **arr;
	int n;
	switch (kind)
	{
		case AP_DEATH_EATEN:
			arr = ap_death_msgs_eaten;
			n = (int)(sizeof(ap_death_msgs_eaten) / sizeof(ap_death_msgs_eaten[0]));
			break;
		case AP_DEATH_FELL:
			arr = ap_death_msgs_fell;
			n = (int)(sizeof(ap_death_msgs_fell) / sizeof(ap_death_msgs_fell[0]));
			break;
		default:
			arr = ap_death_msgs_generic;
			n = (int)(sizeof(ap_death_msgs_generic) / sizeof(ap_death_msgs_generic[0]));
			break;
	}
	return arr[US_RndT() % n];
}

void ap_on_death(const char* cause)
{
	if (!ap_death_link_enabled)
		return;
	if (ap_suppress_death_send)
	{
		// This kill was triggered by an incoming DeathLink — don't echo it back.
		ap_suppress_death_send = false;
		return;
	}
	ap_send_death(cause ? cause : "Commander Keen died");
}

void ap_apply_pending_death(void)
{
	if (!ap_pending_death)
		return;
	// Drop pending if we can't apply it right now. Holding it across the
	// death animation would cause a second death immediately after respawn.
	if (ck_gameState.levelState != LS_Playing
	    || ck_invincibilityTimer != 0
	    || ck_godMode)
	{
		ap_pending_death = false;
		return;
	}

	ap_pending_death = false;
	ap_suppress_death_send = true;
	CK_KillKeen();
}

bool ap_has_level(int level, int ep)
{
	// Score-item dumper bypasses level locks so every level is reachable
	// without owning the AP unlock items.
	if (getenv("OMNISPEAK_DUMP_SCORE_ITEMS"))
		return true;

	int ap_level_to_item_ck4[] = {
		0,
		AP_ITEM_BV,
		AP_ITEM_SV,
		AP_ITEM_PP,
		AP_ITEM_COTD,
		AP_ITEM_COC,
		AP_ITEM_CRYS,
		AP_ITEM_HI,
		AP_ITEM_SY,
		AP_ITEM_MIR,
		AP_ITEM_LO,
		AP_ITEM_POTM,
		AP_ITEM_POS,
		AP_ITEM_POTGA,
		AP_ITEM_POTF,
		AP_ITEM_IOT,
		AP_ITEM_IOF,
		AP_ITEM_WOW,
		AP_ITEM_BWBMR
	};

	int ap_level_to_item_ck5[] = {
		0,
		AP_ITEM_IVS,
		AP_ITEM_SC,
		AP_ITEM_DTV,
		AP_ITEM_EFS,
		AP_ITEM_DTB,
		AP_ITEM_RCC,
		AP_ITEM_DTS,
		AP_ITEM_NBI,
		AP_ITEM_DTT,
		AP_ITEM_BMI,
		AP_ITEM_GDH,
		AP_ITEM_QED
	};

	if (ep == 1)
		if (level <= 17)
		{
			return ap_has_item(ap_level_to_item_ck4[level]);
		}
		else
		{
			//bwbm should require all other levels at least reachable
			for (int i = 1; i <= 18; i++)
			{
				if (i == 14) continue; //skip pyramid of the forbidden
				if (!ap_has_item(ap_level_to_item_ck4[i]))
					return false;
			}
			return true;
		}

	if (ep == 2)
		return ap_has_item(ap_level_to_item_ck5[level]);

	return false;

}

void ap_open_blocks(void)
{
    int x, y;
    uint16_t *pw;
    int flags;

    pw = CA_TilePtrAtPos(0, 0, 2); // info layer

    for (y = 0; y < CA_GetMapHeight(); y++)
    {
        for (x = 0; x < CA_GetMapWidth(); x++, pw++)
        {
            flags = (*pw) >> 8;

            if (flags == 0xD0)
            {
                // remove blocking foreground tile
                CA_SetTileAtPos(x, y, 1, 0);
            }
        }
    }
}

void ap_show_message(const char* msg)
{
	US_CenterWindow(20, 3);
	US_PrintCentered(msg);
	VH_UpdateScreen();
	IN_WaitButton();
}

// ---------------------------------------------------------------------------
// In-game toast notifications
//
// Newest toast lives at index 0 and is drawn at the bottom. Older toasts
// stack upward and fade off as their TTL expires. Sprite sync ticks at
// AP_TOAST_TICKS_PER_SEC per second in gameplay, so the configured duration
// (seconds) becomes that many ticks of TTL.

#define AP_TOAST_MAX 4
#define AP_TOAST_TICKS_PER_SEC 70
#define AP_TOAST_MSG_LEN 80
#define AP_TOAST_FONT 1
#define AP_TOAST_TEXT_COLOUR 14
#define AP_TOAST_BG_COLOUR 0
// Baseline for the most recent toast. The bottom strip keeps the toast clear
// of the scoreBox HUD that sits in the top-left corner. Newer toasts anchor
// here and older toasts stack upward.
#define AP_TOAST_Y_BASE 184
#define AP_TOAST_X 4
#define AP_TOAST_MAX_W 312
#define AP_TOAST_PAD_X 4
#define AP_TOAST_PAD_Y 2
#define AP_TOAST_GAP 2

// User-tunable enables and duration. Defaults preserve the original behavior
// (every toast type on, 4-second TTL). Loaded from connection.txt at startup
// in ap_load_connection_info().
bool ap_toasts_enabled = true;
bool ap_toasts_received_enabled = true;
bool ap_toasts_sent_enabled = true;
bool ap_toasts_deathlink_enabled = true;
int  ap_toast_duration_secs = 4;

typedef struct
{
	char msg[AP_TOAST_MSG_LEN];
	int ttl;
} ap_toast_t;

static ap_toast_t ap_toasts[AP_TOAST_MAX];

void ap_toast_push(ap_toast_category_t category, const char* msg)
{
	if (!msg)
		return;
	if (!ap_toasts_enabled)
		return;

	switch (category)
	{
		case AP_TOAST_RECEIVED:
			if (!ap_toasts_received_enabled) return;
			break;
		case AP_TOAST_SENT:
			if (!ap_toasts_sent_enabled) return;
			break;
		case AP_TOAST_DEATHLINK:
			if (!ap_toasts_deathlink_enabled) return;
			break;
	}

	// Shift older toasts up one slot, dropping the oldest.
	for (int i = AP_TOAST_MAX - 1; i > 0; i--)
		ap_toasts[i] = ap_toasts[i - 1];

	strncpy(ap_toasts[0].msg, msg, AP_TOAST_MSG_LEN - 1);
	ap_toasts[0].msg[AP_TOAST_MSG_LEN - 1] = '\0';

	int duration = ap_toast_duration_secs;
	if (duration < 1) duration = 1;
	ap_toasts[0].ttl = duration * AP_TOAST_TICKS_PER_SEC;
}

void ap_toast_tick(void)
{
	int ticks = SD_GetSpriteSync();
	if (ticks <= 0)
		ticks = 1;

	for (int i = 0; i < AP_TOAST_MAX; i++)
	{
		if (ap_toasts[i].ttl > 0)
		{
			ap_toasts[i].ttl -= ticks;
			if (ap_toasts[i].ttl < 0)
				ap_toasts[i].ttl = 0;
		}
	}
}

void ap_toast_draw(void)
{
	// Invoked inline from RF_Refresh after the engine's tile/sprite work
	// and dirty-flag cleanup, in the same slot rf_drawFunc would have
	// occupied. VHB_* adds the engine's scroll offset and marks tiles
	// dirty on all pages, so each subsequent frame's RFL_UpdateTiles
	// repaints the underlying tiles fresh (clears expired-toast residue)
	// before this function repaints the current toast on top.
	//
	// Newest toast is at index 0 and renders at AP_TOAST_Y_BASE; older
	// toasts stack upward. The bar is padded generously around the glyph
	// extents because the prop font's XOR text rendering produces visible
	// flicker if any glyph pixel falls outside the bar onto a tile that
	// changes from frame to frame.

	// VHB_* internally adds `VL_GetScrollX() & 8` to X (the engine only
	// tracks the bit-3 portion of horizontal scroll because the buffer is
	// just 16px wider than the screen). For a HUD overlay we want fixed
	// screen X, which requires tracking the FULL scrollX mod 16. We add
	// the missing low bits (`& 7`) here so VHB_'s subsequent `& 8` add
	// completes the full mod-16 compensation. Y already gets the full
	// scrollY from VHB_, so no compensation needed there.
	int scrX_fix = VL_GetScrollX() & 7;

	int y = AP_TOAST_Y_BASE;

	for (int i = 0; i < AP_TOAST_MAX; i++)
	{
		if (ap_toasts[i].ttl <= 0)
			continue;

		uint16_t w = 0, h = 0;
		VH_MeasurePropString(ap_toasts[i].msg, &w, &h, AP_TOAST_FONT);
		if (h == 0)
			h = 8;
		if (w > AP_TOAST_MAX_W)
			w = AP_TOAST_MAX_W;

		int bar_x = AP_TOAST_X - AP_TOAST_PAD_X + scrX_fix;
		int bar_y = y - AP_TOAST_PAD_Y;
		int bar_w = (int)w + 2 * AP_TOAST_PAD_X;
		int bar_h = (int)h + 2 * AP_TOAST_PAD_Y;

		if (bar_y < 0)
			break;

		VHB_Bar(bar_x, bar_y, bar_w, bar_h, AP_TOAST_BG_COLOUR);
		VHB_DrawPropString(ap_toasts[i].msg, AP_TOAST_X + scrX_fix, y, AP_TOAST_FONT, AP_TOAST_TEXT_COLOUR);

		y -= bar_h + AP_TOAST_GAP;
	}
}