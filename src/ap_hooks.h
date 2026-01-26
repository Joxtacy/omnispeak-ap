//ap_hooks.h
#ifndef AP_HOOKS_H
#define AP_HOOKS_H

extern int ap_current_level;
extern int ap_current_points;
extern bool ap_has_pogo;
extern bool ap_has_stunner;

void ap_on_level_complete(int episode);
void ap_on_keygem_get(int item);
void ap_on_score_increase(int points);

#endif