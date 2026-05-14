#ifndef AP_PICKER_H
#define AP_PICKER_H

#include "ck_ep.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Show a native modal dialog asking the user which Keen episode to
 * play. `episodes` is the null-terminated `ck_episodes` array; only
 * entries whose isPresent() returns true become buttons.
 *
 * Returns the chosen CK_EpisodeDef *, or NULL if:
 *   - the user closed/cancelled the dialog,
 *   - fewer than 2 episodes are present (nothing to pick between), or
 *   - the build has no SDL support (picker is a no-op).
 *
 * The caller distinguishes "user cancelled" from "picker unavailable"
 * by counting present episodes before calling.
 */
CK_EpisodeDef *AP_Picker_PickEpisode(CK_EpisodeDef **episodes);

#ifdef __cplusplus
}
#endif

#endif
