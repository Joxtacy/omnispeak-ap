#include <stdio.h>
#include "ap_hooks.h"
#include "id_sd.h"
#include "id_ca.h"
#include "id_us.h"
#include "id_vh.h"
#include "id_in.h"
#include "ck_def.h"

int ap_current_level = -1;
int ap_current_points = 0;
bool ap_has_pogo = 0;
bool ap_has_stunner = 0;

void ap_on_level_complete(int episode)
{
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Level Complete: Episode %d, Level %d, %d points\n", episode, ap_current_level, ap_current_points);

	fclose(f);
}

void ap_on_keygem_get(int item)
{
	FILE *f = fopen("ap_log.txt", "a");
	if(!f) return;

	fprintf(f, "[AP] Keygem Received: %d in Level %d\n", item, ap_current_level);

	fclose(f);
}

void ap_on_score_increase(int points)
{
	ap_current_points += points;
}

bool ap_has_level(int level)
{
	switch (level)
	{
		case 1:
		case 2:
		case 18:
			return true;
		default:
			return false;
	}
}

void ap_show_message(const char* msg)
{
	SD_WaitSoundDone();
	CA_UpLevel();
	CA_CacheGrChunk(CK_CHUNKNUM(PIC_KEENTALK1));
	US_CenterWindow(26, 8);
	VHB_DrawBitmap(US_GetWindowX() + US_GetWindowW() - 0x30, US_GetWindowY(), CK_CHUNKNUM(PIC_KEENTALK1));
	US_SetWindowW(US_GetWindowW() - 0x30);
	US_SetPrintY(US_GetPrintY() + 6);
	US_CPrint(msg);
	VH_UpdateScreen();
	// VL_WaitVBL(30);
	IN_ClearKeysDown();
	IN_WaitButton();
	CA_DownLevel();
}