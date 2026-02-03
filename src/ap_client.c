#include <stdio.h>
#include "ap_hooks.h"

void ap_toggle_stunner()
{
	ap_has_stunner = !ap_has_stunner;
}

void ap_toggle_pogo()
{
	ap_has_pogo = !ap_has_pogo;
}

void ap_toggle_wetsuit()
{
	ap_has_wetsuit = !ap_has_wetsuit;
}