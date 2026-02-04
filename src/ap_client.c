#include <stdio.h>
#include "ap_client.h"
#include "ap_defs.h"

#include <stdio.h>
#include <string.h>

#define AP_MAX_ITEMS	256
#define AP_MAX_LOCATIONS	512

static bool ap_items[AP_MAX_ITEMS];
static bool ap_locations_checked[AP_MAX_LOCATIONS];
static bool ap_initialized = false;

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

void client_init(void)
{
	memset(ap_items, 0, sizeof(ap_items));
	memset(ap_locations_checked, 0, sizeof(ap_locations_checked));
	ap_initialized = true;

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Client initialized (offline)\n");
		fclose(f);
	}
}

void client_poll(void)
{
	//nothing for now
}

void client_location_check(int location_id)
{
	if (!ap_initialized)
		return;

	if (location_id < 0 || location_id >= AP_MAX_LOCATIONS)
		return;

	if (ap_locations_checked[location_id])
		return;

	ap_locations_checked[location_id] = true;

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Location checked: %d\n", location_id);
		fclose(f);
	}
}

bool client_has_item(int item_id)
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