#include <stdio.h>
#include "ap_hooks.h"
#include "ap_defs.h"
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
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Level Complete: Episode %d, Level %d, %d points\n", ap_current_episode, ap_current_level, ap_points_gained);

	fclose(f);
}

void ap_on_keygem_get(int item)
{
	/*const char* msg = "Oh sweet, I found\nARCHIPELAGO ITEM\nfor ARCHIPELAGO PLAYER\0";
	ap_show_message(msg);*/
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Keygem Received: %d in Level %d in Episode %d\n", item, ap_current_level, ap_current_episode);

	fclose(f);
}

void ap_on_security_card_get(void)
{
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Security Card GET: Episode 5, Level %d\n", ap_current_level);

	fclose(f);
}

void ap_on_wetsuit_get(void)
{
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Wetsuit GET!!\n");

	fclose(f);
}

void ap_on_score_increase(int score)
{
	ap_points_gained = score - ap_starting_points;
}

bool ap_has_level(int level, int ep)
{
	int unique_check = level * 3 + (ep+3);
	switch (unique_check)
	{
		case AP_LEVEL_SLUG_VILLAGE:
		case AP_LEVEL_BORDER_VILLAGE:
		case AP_LEVEL_THE_PERILOUS_PIT:
		case AP_LEVEL_BEAN_WITH_BACON_MEGAROCKET:
		case AP_LEVEL_MIRAGIA:
		case AP_LEVEL_ION_VENTILATION_SYSTEM:
			return true;
		default:
			return false;
	}
}

void ap_show_message(const char* msg)
{
	US_CenterWindow(20, 3);
	US_PrintCentered(msg);
	VH_UpdateScreen();
	IN_WaitButton();
}