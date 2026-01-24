//ap_hooks.h
#ifndef AP_HOOKS_H
#define AP_HOOKS_H

extern int ap_current_level;

void ap_on_level_complete(int episode);
void ap_on_keygem_get(int item);

#endif