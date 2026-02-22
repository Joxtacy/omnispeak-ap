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

void ap_on_score_increase(int score)
{
	ap_points_gained = score - ap_starting_points;
}

bool ap_has_level(int level, int ep)
{
	int ap_level_to_item_ck4[]{
		0,
		AP_ITEM_BV,
		AP_ITEM_SV,
		AP_ITEM_PP,
		AP_ITEM_COTD,
		AP_ITEM_COC,
		AP_ITEM_CRYS,
		AP_ITEM_HI,
		AP_ITEM_SY,
		AP_ITEM_MIR,
		AP_ITEM_LO,
		AP_ITEM_POTM,
		AP_ITEM_POS,
		AP_ITEM_POTGA,
		AP_ITEM_POTF,
		AP_ITEM_IOT,
		AP_ITEM_IOF,
		AP_ITEM_WOW,
		AP_ITEM_BWBMR
	};

	int ap_level_to_item_ck5[]{
		0,
		AP_ITEM_IVS,
		AP_ITEM_SC,
		AP_ITEM_DTV,
		AP_ITEM_EFS,
		AP_ITEM_DTB,
		AP_ITEM_RCC,
		AP_ITEM_DTS,
		AP_ITEM_NBI,
		AP_ITEM_DTT,
		AP_ITEM_BMI,
		AP_ITEM_GDH,
		AP_ITEM_QED
	};

	if (ep == 1)
		return ap_has_item(ap_level_to_item_ck4[level]);

	if (ep == 2)
		return ap_has_item(ap_level_to_item_ck5[level]);

	return false;

}

void ap_show_message(const char* msg)
{
	US_CenterWindow(20, 3);
	US_PrintCentered(msg);
	VH_UpdateScreen();
	IN_WaitButton();
}