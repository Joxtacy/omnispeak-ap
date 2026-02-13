#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "ap_client.h"
#include "ap_defs.h"
#include "ap_hooks.h"

#include "ck_def.h"

#define AP_MAX_ITEMS		128
#define AP_MAX_LOCATIONS	512

static bool ap_items[AP_MAX_ITEMS];
static bool ap_initialized = false;
static int ap_locations_checked[AP_MAX_LOCATIONS];
static int ap_check_count = 0;
static char ap_server[256] = {0};
static int ap_port = 0;
static SOCKET ap_socket = INVALID_SOCKET;
static int ap_connected = 0;

static int ap_translate_item(int id);
static void ap_load_connection_info(void);
static void ap_connect_to_server(void);

void ap_client_init(void)
{
	memset(ap_items, 0, sizeof(ap_items));
	memset(ap_locations_checked, 0, sizeof(ap_locations_checked));

	ap_check_count = 0;
	ap_initialized = true;

	ap_load_connection_info();
	ap_connect_to_server();
	
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

bool ap_is_checked(int id)
{
	for (int i=0; i < ap_check_count; i++)
		if (ap_locations_checked[i] == id)
			return true;
	return false;
}

bool ap_has_item(int local_id)
{
    if (!ap_initialized)
        return false;

    if (local_id < 0 || local_id >= AP_MAX_ITEMS)
        return false;

    return ap_items[local_id];
}

void ap_client_give_item(int item_id)
{
	int local_id = ap_translate_item(item_id);

	if (!ap_initialized)
		return;

	if (local_id < 0 || local_id >= AP_MAX_ITEMS)
		return;

	if (ap_items[local_id])
		return;

	ap_items[local_id] = true;

	//apply level required items immediately if in the level
	if (ap_current_level > 0)
		ap_apply_level_items(ap_current_level, ap_current_episode);

	//apply abilities immediately when received
	switch (local_id)
	{
		case AP_ITEM_POGO:
			ap_has_pogo = 1;
			break;
		case AP_ITEM_STUNNER:
			ap_has_stunner = 1;
			break;
		case AP_ITEM_WETSUIT:
			ck_gameState.ep.ck4.wetsuit = 1;
			break;
	}

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] Item Received: %d -> local: %d \n", item_id, local_id);
		fclose(f);
	}
}

void ap_apply_level_items(int level, int ep)
{
	if (ep == 1) //keen 4
	{
		switch(level)
		{
			case AP_LEVEL_THE_PERILOUS_PIT:
				if (ap_has_item(AP_ITEM_PP_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_PP_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_PP_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_CAVE_OF_THE_DESCENDENTS:
				if (ap_has_item(AP_ITEM_COTD_RED_GEM) || ap_has_item(AP_ITEM_COTD_GEMSET))
					ck_gameState.keyGems[0] = 1;
				break;
			case AP_LEVEL_CRYSTALUS:
				if (ap_has_item(AP_ITEM_CRYS_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_CRYS_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_CRYS_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_CRYS_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_CRYS_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_SAND_YEGO:
				if (ap_has_item(AP_ITEM_SY_GREEN_GEM) || ap_has_item(AP_ITEM_SY_GEMSET))
					ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_LIFEWATER_OASIS:
				if (ap_has_item(AP_ITEM_LO_GREEN_GEM) || ap_has_item(AP_ITEM_LO_GEMSET))
					ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_MOONS:
				if (ap_has_item(AP_ITEM_POTM_YELLOW_GEM) || ap_has_item(AP_ITEM_POTM_GEMSET))
					ck_gameState.keyGems[1] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_SHADOWS:
				if (ap_has_item(AP_ITEM_POS_BLUE_GEM) || ap_has_item(AP_ITEM_POS_GEMSET))
					ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_GNOSTICENE_ANCEINTS:
				if (ap_has_item(AP_ITEM_POTGA_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_POTGA_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_POTGA_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_FORBIDDEN:
				if (ap_has_item(AP_ITEM_POTF_RED_GEM_1) || ap_has_item(AP_ITEM_POTF_RED_GEM_2))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_POTF_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_POTF_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_POTF_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_POTF_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_ISLE_OF_TAR:
				if (ap_has_item(AP_ITEM_IOT_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_IOT_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_IOT_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_IOT_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = ck_gameState.keyGems[1] = 1;
				break;
			case AP_LEVEL_ISLE_OF_FIRE:
				if (ap_has_item(AP_ITEM_IOF_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_IOF_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_IOF_GEMSET))
					ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
		}
	}
	else if (ep == 2) //keen 5
	{
		switch (level)
		{
			case AP_LEVEL_SECURITY_CENTER:
				if (ap_has_item(AP_ITEM_SC_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_SC_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_SC_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_SC_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_VLOOK:
				if (ap_has_item(AP_ITEM_DTV_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_DTV_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_DTV_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_DTV_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_ENERGY_FLOW_SYSTEMS:
				if (ap_has_item(AP_ITEM_EFS_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_EFS_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_EFS_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_EFS_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_EFS_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_BURRH:
				if (ap_has_item(AP_ITEM_DTB_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_DTB_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_DTB_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_DTB_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_DTB_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_DTB_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_REGULATION_CONTROL_CENTER:
				if (ap_has_item(AP_ITEM_RCC_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_RCC_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_RCC_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_RCC_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_SORRA:
				if (ap_has_item(AP_ITEM_DTS_YELLOW_GEM) || ap_has_item(AP_ITEM_DTS_GEMSET))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_DTS_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_NEUTRINO_BURST_INJECTOR:
				if (ap_has_item(AP_ITEM_NBI_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_NBI_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_NBI_GEMSET))
					ck_gameState.keyGems[2] = ck_gameState.keyGems[0] = 1;
				break;
			case AP_LEVEL_DEFENSE_TUNNEL_TELN:
				if (ap_has_item(AP_ITEM_DTT_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_DTT_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_DTT_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_DTT_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_DTT_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_DTT_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_BROWNIAN_MOTION_INDUCER:
				if (ap_has_item(AP_ITEM_BMI_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_BMI_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_BMI_GEMSET))
					ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = 1;
				break;
			case AP_LEVEL_GRAVITATIONAL_DAMPING_HUB:
				if (ap_has_item(AP_ITEM_GDH_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_GDH_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_GDH_GEMSET))
					ck_gameState.keyGems[3] = ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_GDH_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
				break;
			case AP_LEVEL_QUANTUM_EXPLOSION_DYNAMO:
				if (ap_has_item(AP_ITEM_QED_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_QED_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_QED_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_QED_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_QED_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				break;
		}
	}
}

static int ap_translate_item(int id)
{
	switch(id)
	{
		case 101: return AP_ITEM_POGO;
		case 102: return AP_ITEM_STUNNER;
		case 103: return AP_ITEM_WETSUIT;
		case 1018: return AP_ITEM_BWBMR;
		case 1001: return AP_ITEM_BV;
		case 1002: return AP_ITEM_SV;
		case 1003: return AP_ITEM_PP;
		case 100300: return AP_ITEM_PP_RED_GEM;
		case 100302: return AP_ITEM_PP_BLUE_GEM;
		case 100399: return AP_ITEM_PP_GEMSET;
		case 1004: return AP_ITEM_COTD;
		case 100400: return AP_ITEM_COTD_RED_GEM;
		case 100499: return AP_ITEM_COTD_GEMSET;
		case 1005: return AP_ITEM_COC;
		case 1006: return AP_ITEM_CRYS;
		case 100600: return	AP_ITEM_CRYS_RED_GEM;
		case 100601: return AP_ITEM_CRYS_YELLOW_GEM;
		case 100602: return AP_ITEM_CRYS_BLUE_GEM;
		case 100603: return AP_ITEM_CRYS_GREEN_GEM;
		case 100699: return AP_ITEM_CRYS_GEMSET;
		case 1007: return AP_ITEM_HI;
		case 1008: return AP_ITEM_SY;
		case 100803: return AP_ITEM_SY_GREEN_GEM;
		case 100899: return AP_ITEM_SY_GEMSET;
		case 1009: return AP_ITEM_MIR;
		case 1010: return AP_ITEM_LO;
		case 101003: return AP_ITEM_LO_GREEN_GEM;
		case 101099: return AP_ITEM_LO_GEMSET;
		case 1011: return AP_ITEM_POTM;
		case 101101: return AP_ITEM_POTM_YELLOW_GEM;
		case 101199: return AP_ITEM_POTM_YELLOW_GEM;
		case 1012: return AP_ITEM_POS;
		case 101202: return AP_ITEM_POS_BLUE_GEM;
		case 101299: return AP_ITEM_POS_GEMSET;
		case 1013: return AP_ITEM_POTGA;
		case 101300: return AP_ITEM_POTGA_RED_GEM;
		case 101303: return AP_ITEM_POTGA_GREEN_GEM;
		case 101399: return AP_ITEM_POTGA_GEMSET;
		case 1014: return AP_ITEM_POTF;
		case 101400: return AP_ITEM_POTF_RED_GEM_1;
		case 101410: return AP_ITEM_POTF_RED_GEM_2;
		case 101401: return AP_ITEM_POTF_YELLOW_GEM;
		case 101402: return AP_ITEM_POTF_BLUE_GEM;
		case 101403: return AP_ITEM_POTF_GREEN_GEM;
		case 101499: return AP_ITEM_POTF_GEMSET;
		case 1015: return AP_ITEM_IOT;
		case 101500: return AP_ITEM_IOT_RED_GEM;
		case 101501: return AP_ITEM_IOT_YELLOW_GEM;
		case 101502: return AP_ITEM_IOT_BLUE_GEM;
		case 101599: return AP_ITEM_IOT_GEMSET;
		case 1016: return AP_ITEM_IOF;
		case 101601: return AP_ITEM_IOF_YELLOW_GEM;
		case 101602: return AP_ITEM_IOF_BLUE_GEM;
		case 101699: return AP_ITEM_IOF_GEMSET;
		case 1017: return AP_ITEM_WOW;
		case 2001: return AP_ITEM_IVS;
		case 2002: return AP_ITEM_SC;
		case 200200: return AP_ITEM_SC_RED_GEM;
		case 200202: return AP_ITEM_SC_BLUE_GEM;
		case 200204: return AP_ITEM_SC_KEYCARD;
		case 200299: return AP_ITEM_SC_GEMSET;
		case 2003: return AP_ITEM_DTV;
		case 200300: return AP_ITEM_DTV_RED_GEM;
		case 200301: return AP_ITEM_DTV_YELLOW_GEM;
		case 200304: return AP_ITEM_DTV_KEYCARD;
		case 200399: return AP_ITEM_DTV_GEMSET;
		case 2004: return AP_ITEM_EFS;
		case 200400: return AP_ITEM_EFS_RED_GEM;
		case 200401: return AP_ITEM_EFS_YELLOW_GEM;
		case 200402: return AP_ITEM_EFS_BLUE_GEM;
		case 200403: return AP_ITEM_EFS_GREEN_GEM;
		case 200499: return AP_ITEM_EFS_GEMSET;
		case 2005: return AP_ITEM_DTB;
		case 200500: return AP_ITEM_DTB_RED_GEM;
		case 200501: return AP_ITEM_DTB_YELLOW_GEM;
		case 200502: return AP_ITEM_DTB_BLUE_GEM;
		case 200503: return AP_ITEM_DTB_GREEN_GEM;
		case 200504: return AP_ITEM_DTB_KEYCARD;
		case 200599: return AP_ITEM_DTB_GEMSET;
		case 2006: return AP_ITEM_RCC;
		case 200600: return AP_ITEM_RCC_RED_GEM;
		case 200601: return AP_ITEM_RCC_YELLOW_GEM;
		case 200602: return AP_ITEM_RCC_BLUE_GEM;
		case 200699: return AP_ITEM_RCC_GEMSET;
		case 2007: return AP_ITEM_DTS;
		case 200701: return AP_ITEM_DTS_YELLOW_GEM;
		case 200704: return AP_ITEM_DTS_KEYCARD;
		case 200799: return AP_ITEM_DTS_GEMSET;
		case 2008: return AP_ITEM_NBI;
		case 200800: return AP_ITEM_NBI_RED_GEM;
		case 200802: return AP_ITEM_NBI_BLUE_GEM;
		case 200899: return AP_ITEM_NBI_GEMSET;
		case 2009: return AP_ITEM_DTT;
		case 200900: return AP_ITEM_DTT_RED_GEM;
		case 200901: return AP_ITEM_DTT_YELLOW_GEM;
		case 200902: return AP_ITEM_DTT_BLUE_GEM;
		case 200903: return AP_ITEM_DTT_GREEN_GEM;
		case 200904: return AP_ITEM_DTT_KEYCARD;
		case 200999: return AP_ITEM_DTT_GEMSET;
		case 2010: return AP_ITEM_BMI;
		case 201001: return AP_ITEM_BMI_YELLOW_GEM;
		case 201002: return AP_ITEM_BMI_BLUE_GEM;
		case 201099: return AP_ITEM_BMI_GEMSET;
		case 2011: return AP_ITEM_GDH;
		case 201100: return AP_ITEM_GDH_RED_GEM;
		case 201103: return AP_ITEM_GDH_GREEN_GEM;
		case 201104: return AP_ITEM_GDH_KEYCARD;
		case 201105: return AP_ITEM_GDH_GEMSET;
		case 2012: return AP_ITEM_QED;
		case 201200: return AP_ITEM_QED_RED_GEM;
		case 201201: return AP_ITEM_QED_YELLOW_GEM;
		case 201202: return AP_ITEM_QED_BLUE_GEM;
		case 201203: return AP_ITEM_QED_GREEN_GEM;
		case 201299: return AP_ITEM_QED_GEMSET;
	}
	return -1;
}

static void ap_load_connection_info(void)
{
    FILE *f = fopen("connection.txt", "r");
    if (!f)
    {
        FILE *log = fopen("ap_log.txt", "a");
        if (log)
        {
            fprintf(log, "[AP] connection.txt not found\n");
            fclose(log);
        }
        return;
    }

    char line[512];

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "server:", 7) == 0)
        {
            sscanf(line + 7, "%255s", ap_server);
        }
        else if (strncmp(line, "port:", 5) == 0)
        {
            sscanf(line + 5, "%d", &ap_port);
        }
    }

    fclose(f);

    FILE *log = fopen("ap_log.txt", "a");
    if (log)
    {
        fprintf(log, "[AP] Server parsed: %s\n", ap_server);
        fprintf(log, "[AP] Port parsed: %d\n", ap_port);
        fclose(log);
    }
}

static void ap_connect_to_server(void)
{
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
    {
        FILE *f = fopen("ap_log.txt", "a");
        if (f) {
            fprintf(f, "[AP] WSAStartup failed\n");
            fclose(f);
        }
        return;
    }

    ap_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ap_socket == INVALID_SOCKET)
    {
        FILE *f = fopen("ap_log.txt", "a");
        if (f) {
            fprintf(f, "[AP] Socket creation failed\n");
            fclose(f);
        }
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ap_port);

    // Try numeric IP first
    if (inet_pton(AF_INET, ap_server, &addr.sin_addr) != 1)
    {
        // Not an IP - resolve hostname
        struct hostent *he = gethostbyname(ap_server);
        if (!he)
        {
            FILE *f = fopen("ap_log.txt", "a");
            if (f) {
                fprintf(f, "[AP] Failed to resolve host: %s\n", ap_server);
                fclose(f);
            }
            return;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (connect(ap_socket, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        FILE *f = fopen("ap_log.txt", "a");
        if (f) {
            fprintf(f, "[AP] Connection failed\n");
            fclose(f);
        }
        closesocket(ap_socket);
        ap_socket = INVALID_SOCKET;
        return;
    }

    ap_connected = 1;

    FILE *f = fopen("ap_log.txt", "a");
    if (f) {
        fprintf(f, "[AP] Connected to %s:%d\n", ap_server, ap_port);
        fclose(f);
    }
}