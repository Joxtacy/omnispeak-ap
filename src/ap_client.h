//ap_client.h
#ifndef AP_CLIENT_H
#define AP_CLIENT_H

#include <stdbool.h>

void ap_toggle_stunner(void);
void ap_toggle_pogo(void);
void ap_toggle_wetsuit(void);

void client_init(void);
void client_poll(void);
void client_location_check(int location_id);
bool client_has_item(int item_id);
void ap_client_give_item(int item_id);

#endif