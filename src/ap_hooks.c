#include <stdio.h>
#include "ap_hooks.h"
#include "ap_defs.h"
#include "ap_client.h"
#include "id_sd.h"
#include "id_ca.h"
#include "id_us.h"
#include "id_vh.h"
#include "id_in.h"
#include "ck_def.h"

int ap_current_level = -1;
int ap_current_episode = -1;
int ap_starting_points = 0;
int ap_points_gained = 0;
bool ap_has_pogo = 0;
bool ap_has_stunner = 0;
bool ap_has_wetsuit = 0;
bool ap_force_abort = 0;

void ap_on_level_complete(void)
{
	int location_id = LOC_LEVEL_COMPLETE(ap_current_episode, ap_current_level);
	ap_client_location_check(location_id);
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

void ap_on_wetsuit_get(void)
{
	int location_id = 12345; //whatever the location id ends up being
	ap_client_location_check(location_id);
}

void ap_on_score_increase(int score)
{
	ap_points_gained = score - ap_starting_points;
}

bool ap_has_level(int level, int ep)
{
	if (ep == 1) //CK4 is episode 1
	{
		switch (level)
		{
			case AP_LEVEL_BORDER_VILLAGE:
			case AP_LEVEL_SLUG_VILLAGE:
			case AP_LEVEL_THE_PERILOUS_PIT:
			case AP_LEVEL_MIRAGIA:
				return true;
			default:
				return false;
		}
	}
	else if (ep == 2) //CK5 is episode 2
	{
		switch (level)
		{
			case AP_LEVEL_ION_VENTILATION_SYSTEM:
			case AP_LEVEL_SECURITY_CENTER:
				return true;
			default:
				return false;
		}
	}
	return -1;
}

void ap_show_message(const char* msg)
{
	US_CenterWindow(20, 3);
	US_PrintCentered(msg);
	VH_UpdateScreen();
	IN_WaitButton();
}