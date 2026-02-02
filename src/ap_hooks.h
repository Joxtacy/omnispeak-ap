//ap_hooks.h
#ifndef AP_HOOKS_H
#define AP_HOOKS_H

extern int ap_current_level;
extern int ap_current_episode;
extern int ap_starting_points;
extern int ap_points_gained;
extern bool ap_has_pogo;
extern bool ap_has_stunner;
extern bool ap_has_wetsuit;
extern bool ap_force_abort;

void ap_on_level_complete();
void ap_on_keygem_get(int item);
void ap_on_security_card_get();
void ap_on_wetsuit_get();
void ap_on_score_increase(int points);
bool ap_has_level(int level, int episode);
void ap_show_message(const char* msg);

#endif