#include <stdio.h>
#include "ap_hooks.h"

void ap_on_level_complete(int episode, int level)
{
	printf("[AP] Level Complete: Episode %d, Level %d\n", episode, level);
}