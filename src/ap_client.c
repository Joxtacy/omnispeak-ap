#include <stdio.h>
#include <string.h>

#include "ap_client.h"
#include "ap_defs.h"
#include "ap_hooks.h"

#include "ck_def.h"


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

void ap_apply_level_items(int level, int ep)
{
	if (ep == 1) //keen 4
	{
		switch(level)
		{
			case AP_LEVEL_THE_PERILOUS_PIT:
				if (ap_client_has_item(AP_ITEM_PP_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_PP_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_PP_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_CAVE_OF_THE_DESCENDENTS:
				if (ap_client_has_item(AP_ITEM_COTD_RED_GEM) || ap_client_has_item(AP_ITEM_COTD_GEMSET))
					ck_gameState.keyGems[0] = 1;
				break;
			case AP_LEVEL_CRYSTALUS:
				if (ap_client_has_item(AP_ITEM_CRYS_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_CRYS_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_CRYS_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_CRYS_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_CRYS_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_SAND_YEGO:
				if (ap_client_has_item(AP_ITEM_SY_GREEN_GEM) || ap_client_has_item(AP_ITEM_SY_GEMSET))
					ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_LIFEWATER_OASIS:
				if (ap_client_has_item(AP_ITEM_LO_GREEN_GEM) || ap_client_has_item(AP_ITEM_LO_GEMSET))
					ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_MOONS:
				if (ap_client_has_item(AP_ITEM_POTM_YELLOW_GEM) || ap_client_has_item(AP_ITEM_POTM_GEMSET))
					ck_gameState.keyGems[1] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_SHADOWS:
				if (ap_client_has_item(AP_ITEM_POS_BLUE_GEM) || ap_client_has_item(AP_ITEM_POS_GEMSET))
					ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_GNOSTICENE_ANCEINTS:
				if (ap_client_has_item(AP_ITEM_POTGA_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_POTGA_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_POTGA_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_FORBIDDEN:
				if (ap_client_has_item(AP_ITEM_POTF_RED_GEM_1) || ap_client_has_item(AP_ITEM_POTF_RED_GEM_2))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_POTF_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_POTF_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_POTF_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_POTF_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_ISLE_OF_TAR:
				if (ap_client_has_item(AP_ITEM_IOT_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_IOT_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_IOT_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_IOT_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = ck_gameState.keyGems[1] = 1;
				break;
			case AP_LEVEL_ISLE_OF_FIRE:
				if (ap_client_has_item(AP_ITEM_IOF_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_IOF_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_IOF_GEMSET))
					ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
		}
	}
	else if (ep == 2) //keen 5
	{
		switch (level)
		{
			case AP_LEVEL_SECURITY_CENTER:
				if (ap_client_has_item(AP_ITEM_SC_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_SC_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_SC_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_SC_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_VLOOK:
				if (ap_client_has_item(AP_ITEM_DTV_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_DTV_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_DTV_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_DTV_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_ENERGY_FLOW_SYSTEMS:
				if (ap_client_has_item(AP_ITEM_EFS_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_EFS_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_EFS_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_EFS_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_EFS_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_BURRH:
				if (ap_client_has_item(AP_ITEM_DTB_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_DTB_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_DTB_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_DTB_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_DTB_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_DTB_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_REGULATION_CONTROL_CENTER:
				if (ap_client_has_item(AP_ITEM_RCC_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_RCC_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_RCC_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_RCC_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_SORRA:
				if (ap_client_has_item(AP_ITEM_DTS_YELLOW_GEM) || ap_client_has_item(AP_ITEM_DTS_GEMSET))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_DTS_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_NEUTRINO_BURST_INJECTOR:
				if (ap_client_has_item(AP_ITEM_NBI_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_NBI_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_NBI_GEMSET))
					ck_gameState.keyGems[2] = ck_gameState.keyGems[0] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_TELN:
				if (ap_client_has_item(AP_ITEM_DTT_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_DTT_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_DTT_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_DTT_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_DTT_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_DTT_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_BROWNIAN_MOTION_INDUCER:
				if (ap_client_has_item(AP_ITEM_BMI_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_BMI_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_BMI_GEMSET))
					ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_GRAVITATIONAL_DAMPING_HUB:
				if (ap_client_has_item(AP_ITEM_GDH_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_GDH_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_GDH_GEMSET))
					ck_gameState.keyGems[3] = ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_GDH_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_QUANTUM_EXPLOSION_DYNAMO:
				if (ap_client_has_item(AP_ITEM_QED_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_client_has_item(AP_ITEM_QED_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_client_has_item(AP_ITEM_QED_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_client_has_item(AP_ITEM_QED_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_client_has_item(AP_ITEM_QED_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
		}
	}
}