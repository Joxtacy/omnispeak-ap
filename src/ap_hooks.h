//ap_hooks.h
#ifndef AP_HOOKS_H
#define AP_HOOKS_H

#include <stdbool.h>

extern int ap_current_level;
extern int ap_current_episode;
extern int ap_starting_points;
extern int ap_points_gained;
extern bool ap_has_pogo;
extern bool ap_has_stunner;
extern bool ap_force_abort;

extern bool ap_death_link_enabled;
extern bool ap_pending_death;
extern bool ap_suppress_death_send;

void ap_on_level_complete(void);
void ap_on_keygem_get(int item);
void ap_on_security_card_get(void);
void ap_on_wetsuit_get(void);
void ap_on_score_increase(int points);
// Extra-life pickups (item index 10 / foreground tile misc=27) give the
// player +1 life on contact. The sprite is a Vitalin Keg in CK5 and a
// Lifewater Flask in CK4 — apworld names locations accordingly, but they
// all dispatch through this hook. Indices span both spawn paths
// (info-layer CK_SpawnItem + tile-layer CK_KeenGetTileItem) so the
// per-level scan order is one contiguous range.
void ap_on_extralife_get(int extralife_idx);
void ap_reset_extralife_counter(void);
void ap_scan_tile_extralives(void);
int ap_lookup_tile_extralife(int tileX, int tileY);

// Pointsanity: point-pickup items (100/200/500/1000/2000/5000 pts) as
// location checks. point_class is engine item index minus 4 (0..5, where
// 5 = 5000 pt). instance_index is the per-level, per-class scan index,
// continuous across the info-layer and tile-layer scan passes. Hook is
// safe to call for disabled classes — it gates internally.
void ap_on_pointitem_get(int point_class, int instance_index);
void ap_reset_pointitem_counters(void);
void ap_scan_tile_pointitems(void);
bool ap_lookup_tile_pointitem(int tileX, int tileY, int *out_class, int *out_index);
void ap_on_death(const char* cause);
void ap_apply_pending_death(void);

// Death message categories for ap_random_death_message().
#define AP_DEATH_GENERIC 0
#define AP_DEATH_EATEN   1
#define AP_DEATH_FELL    2
const char* ap_random_death_message(int kind);
bool ap_has_level(int level, int episode);
void ap_open_blocks(void);
void ap_show_message(const char* msg);

// Non-blocking in-game toast notifications.
// ap_toast_push enqueues a message (newest renders at the baseline, older
// toasts stack upward). The category is consulted against the per-category
// enables below before queuing — passing AP_TOAST_RECEIVED for an
// item-received notification lets the user silence those independently.
// ap_toast_tick decrements TTLs once per gameplay frame.
// ap_toast_draw renders active toasts directly into the back buffer; called
// inline from RF_Refresh because registering it as rf_drawFunc triggers a
// black-screen regression on this build.
typedef enum
{
	AP_TOAST_RECEIVED  = 0,
	AP_TOAST_SENT      = 1,
	AP_TOAST_DEATHLINK = 2,
} ap_toast_category_t;

extern bool ap_toasts_enabled;
extern bool ap_toasts_received_enabled;
extern bool ap_toasts_sent_enabled;
extern bool ap_toasts_deathlink_enabled;
extern int  ap_toast_duration_secs;

void ap_toast_push(ap_toast_category_t category, const char* msg);
void ap_toast_tick(void);
void ap_toast_draw(void);

#endif