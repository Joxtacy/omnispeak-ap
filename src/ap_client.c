#include <stdio.h>
#include "ap_client.h"
#include "ap_defs.h"
#include "ap_hooks.h"

#include <string.h>

#define AP_MAX_ITEMS		256
#define AP_MAX_LOCATIONS	1024

static bool ap_items[AP_MAX_ITEMS];
static bool ap_initialized = false;
static int ap_locations_checked[AP_MAX_LOCATIONS];
int ap_check_count = 0;

//----------------------------TEST ONLY----------------------------
void ap_toggle_stunner(void)
{
	ap_has_stunner = !ap_has_stunner;
}

void ap_toggle_pogo(void)
{
	ap_has_pogo = !ap_has_pogo;
}

void ap_toggle_wetsuit(void)
{
	ap_has_wetsuit = !ap_has_wetsuit;
}

//----------------------------END TEST-----------------------------

void ap_client_init(void)
{
	memset(ap_items, 0, sizeof(ap_items));
	memset(ap_locations_checked, 0, sizeof(ap_locations_checked));

	ap_check_count = 0;
	ap_initialized = true;
	
	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Client initialized (offline)\n");
		fclose(f);
	}
}

void ap_client_poll(void)
{
	//nothing for now
}

void ap_client_location_check(int location_id)
{
	if (!ap_initialized)
		return;

	if (ap_is_checked(location_id))
		return;

	if (ap_check_count < AP_MAX_LOCATIONS)
		ap_locations_checked[ap_check_count++] = location_id;

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Location checked: %d\n", location_id);
		fclose(f);
	}
}

bool ap_client_has_item(int item_id)
{
	if (!ap_initialized)
		return false;

	if (item_id < 0 || item_id >= AP_MAX_ITEMS)
		return false;

	return ap_items[item_id];
}

void ap_client_give_item(int item_id)
{
	if (!ap_initialized)
		return;

	if (item_id < 0 || item_id >= AP_MAX_ITEMS)
		return;

	if (ap_items[item_id])
		return;

	ap_items[item_id] = true;

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Item Received: %d\n", item_id);
		fclose(f);
	}
}

bool ap_is_checked(int id)
{
	for (int i=0; i < ap_check_count; i++)
		if (ap_locations_checked[i] == id)
			return true;
	return false;
}