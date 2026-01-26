#include <stdio.h>
#include "ap_hooks.h"

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