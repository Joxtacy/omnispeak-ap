#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <apclientpp/apclient.hpp>
#include <apclientpp/apuuid.hpp>
#include <nlohmann/json.hpp>
//#include <third_party/valijson/include>

#include "ap_client.h"
#include "ap_defs.h"
#include "ap_hooks.h"

#include "ck_def.h"
#include "id_vl.h"

#define AP_MAX_ITEMS		128
#define AP_MAX_LOCATIONS	512

static bool ap_items[AP_MAX_ITEMS];
static bool ap_initialized = false;

// Highest server item index we've already applied this session. On connect
// (and every idle-drop reconnect) the server replays the entire received-items
// list from index 0; without this high-water mark the stackable filler items
// (extra keens, stunner ammo) would be re-granted on every reconnect. Permanent
// items are idempotent via ap_items[], but filler deliberately bypasses that
// guard, so we gate the whole replay on the monotonic NetworkItem index here.
// Reset per process in ap_client_init (reconnects don't re-run init), so a
// genuine fresh launch still receives the full replay exactly once.
static int64_t ap_last_item_index = -1;
static char ap_server[256] = {0};
static char ap_slotname[64] = {0};
static char ap_password[64] = {0};
static int ap_port = 0;
static int episode = 0;

// Locally tracked boss completions. Set when the player actually finishes
// the boss level in this client; loaded from AP DataStorage on slot connect
// so two-episode goals survive disconnect/reconnect. Do NOT derive these
// from the server's "checked locations" set — AP marks a location as
// checked whenever its item is delivered (e.g. via !collect / auto-collect
// when another player goals), which can flip the boss-level location to
// "checked" without the player ever beating it.
static bool local_keen4_done = false;
static bool local_keen5_done = false;

static APClient* ap = nullptr;

static int ap_translate_item(int id);
static void ap_load_connection_info(void);
bool ap_announce_victory(void);
static std::string ap_boss_done_key(int ep);

// Build "Omnispeak AP — Keen <N>[ — <slot>]" and push to the window title.
// Called once at startup with the slot from connection.txt; never updated
// later, so OBS window-capture sources stay locked to a stable string.
static void ap_update_window_title(const char *slot)
{
    if (!ck_currentEpisode) return;
    const char *epName =
        ck_currentEpisode->ep == EP_CK4 ? "Keen 4" :
        ck_currentEpisode->ep == EP_CK5 ? "Keen 5" :
        ck_currentEpisode->ep == EP_CK6 ? "Keen 6" : "Keen";
    char title[128];
    if (slot && *slot)
        snprintf(title, sizeof(title),
                 "Omnispeak AP \xE2\x80\x94 %s \xE2\x80\x94 %s", epName, slot);
    else
        snprintf(title, sizeof(title),
                 "Omnispeak AP \xE2\x80\x94 %s", epName);
    VL_SetWindowTitle(title);
}

void ap_client_init(void)
{
    memset(ap_items, 0, sizeof(ap_items));
    ap_last_item_index = -1;

	ap_initialized = true;

    ap_load_connection_info();

    // Set the window title once, using the slot from connection.txt. We
    // deliberately don't update it later (e.g. on slot-connected) so that
    // OBS window-capture sources stay locked to a stable string for the
    // whole session, even when streaming multiple AP-Keen instances.
    ap_update_window_title(ap_slotname);

    #include "apuuid.hpp"

	std::string uri;

	std::string server = ap_server;

	// If user already included ws:// or wss://, use it as-is
	if (server.rfind("ws://", 0) == 0 || server.rfind("wss://", 0) == 0)
	{
		uri = server;
	}
	else
	{
		// Default to secure for non-localhost
		if (server == "127.0.0.1" || server == "localhost")
			uri = "ws://" + server + ":" + std::to_string(ap_port);
		else
			uri = "wss://" + server + ":" + std::to_string(ap_port) + "/";
	}

	ap = new APClient(
		ap_get_uuid("ap_uuid.txt", ap_server),
		"Commander Keen",
		uri,
		"cacert.pem"
	);

    ap->set_socket_connected_handler([]() {
		FILE *f = fopen("ap_log.txt", "a");
		if (f) {
			fprintf(f, "[AP] Socket connected\n");
			fclose(f);
		}
	});

	ap->set_socket_disconnected_handler([]() {
		FILE *f = fopen("ap_log.txt", "a");
		if (f) {
			fprintf(f, "[AP] Socket disconnected\n");
			fclose(f);
		}
	});

	ap->set_socket_error_handler([](const std::string& err) {
		FILE *f = fopen("ap_log.txt", "a");
		if (f) {
			fprintf(f, "[AP] Socket error: %s\n", err.c_str());
			fclose(f);
		}
	});

	ap->set_room_info_handler([&]() {
		FILE *f = fopen("ap_log.txt", "a");
		if (f) {
			fprintf(f, "[AP] RoomInfo received, connecting slot\n");
			fclose(f);
		}

		APClient::Version ver;
		ver.ma = 0;
		ver.mi = 6;
		ver.build = 6;

		ap->ConnectSlot(
			ap_slotname,
			ap_password,
			7,
			{"AP"},
			ver
		);
	});

   ap->set_slot_connected_handler([](const nlohmann::json& slot_data) {
    if (slot_data.contains("episode_select"))
    {
        auto& val = slot_data["episode_select"];
        if (val.is_number())
            episode = val.get<int>();
        else if (val.is_string())
            episode = std::stoi(val.get<std::string>());
    }

    if (slot_data.contains("death_link"))
    {
        auto& val = slot_data["death_link"];
        if (val.is_number())
            ap_death_link_enabled = val.get<int>() != 0;
        else if (val.is_boolean())
            ap_death_link_enabled = val.get<bool>();
        else if (val.is_string())
            ap_death_link_enabled = std::stoi(val.get<std::string>()) != 0;
    }

    if (ap_death_link_enabled)
    {
        ap->ConnectUpdate(false, 0, true, {"AP", "DeathLink"});
        FILE *f = fopen("ap_log.txt", "a");
        if (f)
        {
            fprintf(f, "[AP] DeathLink subscribed\n");
            fclose(f);
        }
    }

    // Seed local boss-done flags from DataStorage. The retrieved handler
    // will set them and re-evaluate victory in case the player goaled in
    // a previous session but the GOAL status update never went through.
    ap->Get({ap_boss_done_key(AP_EPISODE_CK4), ap_boss_done_key(AP_EPISODE_CK5)});

	ap_show_message("Slot Connected.");
});

    ap->set_retrieved_handler([](const std::map<std::string, nlohmann::json>& keys) {
        std::string k4 = ap_boss_done_key(AP_EPISODE_CK4);
        std::string k5 = ap_boss_done_key(AP_EPISODE_CK5);
        auto it4 = keys.find(k4);
        if (it4 != keys.end() && it4->second.is_boolean() && it4->second.get<bool>())
            local_keen4_done = true;
        auto it5 = keys.find(k5);
        if (it5 != keys.end() && it5->second.is_boolean() && it5->second.get<bool>())
            local_keen5_done = true;
        ap_announce_victory();
    });

    ap->set_items_received_handler(
		[](const std::list<APClient::NetworkItem>& items) {
		for (auto& item : items)
		{
			// Skip anything already applied this session. The server replays
			// the full list from index 0 on every (re)connect; only items past
			// our high-water mark are genuinely new. This stops idle-drop
			// reconnects from re-granting filler (extra keens / stunner ammo)
			// and from re-toasting every previously received item.
			if (item.index <= ap_last_item_index)
				continue;
			ap_last_item_index = item.index;

			ap_client_give_item(item.item);

			// Toast every received item. The PrintJSON handler covers
			// "items I sent to others"; this handles "items I received".
			std::string my_game = ap->get_game();
			std::string item_name = ap->get_item_name(item.item, my_game);
			char buf[80];
			if (item.player == ap->get_player_number())
			{
				snprintf(buf, sizeof(buf), "Found %s", item_name.c_str());
			}
			else
			{
				std::string sender = ap->get_player_alias(item.player);
				snprintf(buf, sizeof(buf), "Got %s from %s",
				         item_name.c_str(), sender.c_str());
			}
			ap_toast_push(AP_TOAST_RECEIVED, buf);
		}
	});

	// Items WE sent to other slots. items_received only fires for things
	// granted TO us, so to cover outgoing items we listen to PrintJSON
	// ItemSend events and filter to those where our slot is the source
	// (item.player) and the receiver is someone else (*receiving). We
	// drop the "NoText" tag on ConnectSlot/ConnectUpdate above so these
	// PrintJSON messages actually arrive.
	ap->set_print_json_handler(
		[](const APClient::PrintJSONArgs& args) {
			if (args.type != "ItemSend") return;
			if (!args.item || !args.receiving) return;

			int my_slot = ap->get_player_number();
			if (args.item->player != my_slot) return;   // not from us
			if (*args.receiving == my_slot) return;      // self-find, items_received covers it

			std::string recv_game = ap->get_player_game(*args.receiving);
			std::string item_name = ap->get_item_name(args.item->item, recv_game);
			std::string recipient = ap->get_player_alias(*args.receiving);

			char buf[80];
			snprintf(buf, sizeof(buf), "Sent %s to %s",
			         item_name.c_str(), recipient.c_str());
			ap_toast_push(AP_TOAST_SENT, buf);
		});

	ap->set_bounced_handler([](const nlohmann::json& cmd) {
		if (!ap_death_link_enabled)
			return;
		if (!cmd.contains("tags"))
			return;
		bool is_deathlink = false;
		for (auto& tag : cmd["tags"])
		{
			if (tag.is_string() && tag.get<std::string>() == "DeathLink")
			{
				is_deathlink = true;
				break;
			}
		}
		if (!is_deathlink)
			return;

		std::string source, cause;
		if (cmd.contains("data"))
		{
			if (cmd["data"].contains("source") && cmd["data"]["source"].is_string())
				source = cmd["data"]["source"].get<std::string>();
			if (cmd["data"].contains("cause") && cmd["data"]["cause"].is_string())
				cause = cmd["data"]["cause"].get<std::string>();
		}

		if (source == ap_slotname)
			return; // ignore our own bounce

		ap_pending_death = true;

		// Surface the death in-game so the player sees who killed them and
		// (when the sender included one) the flavor cause string.
		char buf[80];
		if (!cause.empty())
			snprintf(buf, sizeof(buf), "%s: %s",
			         source.empty() ? "Someone" : source.c_str(),
			         cause.c_str());
		else
			snprintf(buf, sizeof(buf), "DeathLink from %s",
			         source.empty() ? "Someone" : source.c_str());
		ap_toast_push(AP_TOAST_DEATHLINK, buf);

		FILE *f = fopen("ap_log.txt", "a");
		if (f)
		{
			fprintf(f, "[AP] DeathLink received from %s: %s\n",
			        source.empty() ? "?" : source.c_str(),
			        cause.empty() ? "?" : cause.c_str());
			fclose(f);
		}
	});

}

// Run an AP network operation, swallowing any transport-layer exception so a
// transient bad socket state can't abort the whole game. websocketpp throws
// "invalid state" from send()/poll() when the underlying connection isn't OPEN
// — e.g. an idle drop whose close event hasn't been processed yet because the
// game was paused (poll() doesn't run while paused). Without this guard the
// exception propagates uncaught out of the game loop and terminates the
// process. Swallowing it is safe: the next poll() detects the dead socket and
// reconnects cleanly, and AP re-syncs all state on reconnect.
template <typename F>
static void ap_guard(const char* what, F&& fn)
{
	try {
		fn();
	} catch (const std::exception& ex) {
		FILE *f = fopen("ap_log.txt", "a");
		if (f) {
			fprintf(f, "[AP] %s threw: %s (ignored, will retry)\n",
			        what, ex.what());
			fclose(f);
		}
	}
}

void ap_send_death(const char* cause)
{
	if (!ap || ap->get_state() != APClient::State::SLOT_CONNECTED)
		return;
	if (!ap_death_link_enabled)
		return;

	nlohmann::json data = {
		{"time",   ap->get_server_time()},
		{"source", ap_slotname},
		{"cause",  cause ? cause : "Commander Keen died"},
	};
	ap_guard("Bounce(DeathLink)", [&]{ ap->Bounce(data, {}, {}, {"DeathLink"}); });
	// Flush immediately: most death paths set LS_Died on the same frame they
	// fire ap_on_death, which exits the gameplay loop where ap_client_poll
	// runs. Without this, the Bounce sits queued in the websocket until the
	// player dismisses the death/try-again modal.
	ap_guard("poll(send_death)", []{ ap->poll(); });

	FILE *f = fopen("ap_log.txt", "a");
	if (f)
	{
		fprintf(f, "[AP] DeathLink sent: %s\n", cause ? cause : "Commander Keen died");
		fclose(f);
	}
}

void ap_client_poll(void)
{
    if (ap)
        ap_guard("poll", []{ ap->poll(); });
}

void ap_client_location_check(int location_id)
{
    if (!ap) return;
    if (ap_is_checked(location_id)) return;

    std::list<int64_t> checks;
    checks.push_back(location_id);
    ap_guard("LocationChecks", [&]{ ap->LocationChecks(checks); });
}

bool ap_is_checked(int id)
{
	if (!ap) return false;
	auto checked = ap->get_checked_locations();
	return checked.count(id) > 0;
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

	//do not mark filler as received so we can obtain multiple
	if (local_id != AP_ITEM_EXTRA_KEEN && local_id != AP_ITEM_STUNNER_AMMO)
	{
		if (ap_items[local_id])
			return;

		ap_items[local_id] = true;
	}

	//apply level required items immediately if in the level
	if (ap_current_level > 0)
		ap_apply_level_items(ap_current_level, ap_current_episode);

	//apply abilities + filler immediately when received
	switch (local_id)
	{
		case AP_ITEM_POGO:
			ap_has_pogo = 1;
			break;
		case AP_ITEM_STUNNER:
			ap_has_stunner = 1;
			break;
		case AP_ITEM_WETSUIT:
			if (ap_current_episode == AP_EPISODE_CK4)
				ck_gameState.ep.ck4.wetsuit = 1;
			break;
		case AP_ITEM_STUNNER_AMMO:
			ck_gameState.numShots++;
			break;
		case AP_ITEM_EXTRA_KEEN:
			ck_gameState.numLives++;
			break;
	}
}

static std::string ap_boss_done_key(int ep)
{
	int player = ap ? ap->get_player_number() : -1;
	const char* tag = (ep == AP_EPISODE_CK4) ? "keen4_done_" : "keen5_done_";
	return std::string(tag) + std::to_string(player);
}

void ap_mark_boss_complete(int ep)
{
	if (ep == AP_EPISODE_CK4)
	{
		if (local_keen4_done) return;
		local_keen4_done = true;
	}
	else if (ep == AP_EPISODE_CK5)
	{
		if (local_keen5_done) return;
		local_keen5_done = true;
	}
	else
	{
		return;
	}

	if (ap && ap->get_state() == APClient::State::SLOT_CONNECTED)
	{
		APClient::DataStorageOperation op;
		op.operation = "replace";
		op.value = true;
		ap_guard("Set(boss_done)", [&]{
			ap->Set(ap_boss_done_key(ep), nlohmann::json(false), false, {op});
		});
	}
}

bool ap_announce_victory(void)
{
	if (!ap || ap->get_state() != APClient::State::SLOT_CONNECTED) return false;

	bool victory = false;

	if (episode == 1)
		victory = local_keen4_done;
	else if (episode == 2)
		victory = local_keen5_done;
	else
		victory = local_keen4_done && local_keen5_done;

	if (victory)
	{
		ap_guard("StatusUpdate(GOAL)", []{ ap->StatusUpdate(APClient::ClientStatus::GOAL); });
		return true;
	}

	return false;
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
				if (ap_has_item(AP_ITEM_COTD_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_COTD_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_COTD_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[1] = 1;
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
			case AP_LEVEL_PYRAMID_OF_THE_GNOSTICENE_ANCIENTS:
				if (ap_has_item(AP_ITEM_POTGA_RED_GEM))
					ck_gameState.keyGems[0] = 1;
				if (ap_has_item(AP_ITEM_POTGA_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_POTGA_GEMSET))
					ck_gameState.keyGems[0] = ck_gameState.keyGems[3] = 1;
				break;
			case AP_LEVEL_PYRAMID_OF_THE_FORBIDDEN:
				// POTF is the only level with TWO red gem holders. keyGems is a
				// consumable count (each holder does keyGems[c]--), so opening
				// both red doors needs a red count of 2. Grant one red per red
				// item held (two distinct AP items), not a single boolean.
				ck_gameState.keyGems[0] =
					(ap_has_item(AP_ITEM_POTF_RED_GEM_1) ? 1 : 0) +
					(ap_has_item(AP_ITEM_POTF_RED_GEM_2) ? 1 : 0);
				if (ap_has_item(AP_ITEM_POTF_BLUE_GEM))
					ck_gameState.keyGems[2] = 1;
				if (ap_has_item(AP_ITEM_POTF_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_POTF_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				if (ap_has_item(AP_ITEM_POTF_GEMSET))
				{
					ck_gameState.keyGems[0] = 2; // both red doors
					ck_gameState.keyGems[1] = ck_gameState.keyGems[2] = ck_gameState.keyGems[3] = 1;
				}
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
			case AP_LEVEL_KORATH_III_BASE:
				// Korath has TWO blue gem holders (cf. POTF's two reds), so the
				// blue count must reach 2 to open both doors. Grant one blue per
				// blue item held; the gemset grants both.
				if (ap_has_item(AP_ITEM_KORATH_YELLOW_GEM))
					ck_gameState.keyGems[1] = 1;
				ck_gameState.keyGems[2] =
					(ap_has_item(AP_ITEM_KORATH_BLUE_GEM_1) ? 1 : 0) +
					(ap_has_item(AP_ITEM_KORATH_BLUE_GEM_2) ? 1 : 0);
				if (ap_has_item(AP_ITEM_KORATH_GREEN_GEM))
					ck_gameState.keyGems[3] = 1;
				if (ap_has_item(AP_ITEM_KORATH_GEMSET))
				{
					ck_gameState.keyGems[1] = 1;
					ck_gameState.keyGems[2] = 2; // both blue doors
					ck_gameState.keyGems[3] = 1;
				}
				if (ap_has_item(AP_ITEM_KORATH_KEYCARD))
					ck_gameState.ep.ck5.securityCard = 1;
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
		case 900: return AP_ITEM_EXTRA_KEEN;
		case 901: return AP_ITEM_STUNNER_AMMO;
		case 1018: return AP_ITEM_BWBMR;
		case 1001: return AP_ITEM_BV;
		case 1002: return AP_ITEM_SV;
		case 1003: return AP_ITEM_PP;
		case 100300: return AP_ITEM_PP_RED_GEM;
		case 100302: return AP_ITEM_PP_BLUE_GEM;
		case 100399: return AP_ITEM_PP_GEMSET;
		case 1004: return AP_ITEM_COTD;
		case 100400: return AP_ITEM_COTD_RED_GEM;
		case 100401: return AP_ITEM_COTD_YELLOW_GEM;
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
		case 101199: return AP_ITEM_POTM_GEMSET;
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
		case 201199: return AP_ITEM_GDH_GEMSET;
		case 2012: return AP_ITEM_QED;
		case 201200: return AP_ITEM_QED_RED_GEM;
		case 201201: return AP_ITEM_QED_YELLOW_GEM;
		case 201202: return AP_ITEM_QED_BLUE_GEM;
		case 201203: return AP_ITEM_QED_GREEN_GEM;
		case 201299: return AP_ITEM_QED_GEMSET;
		case 2013: return AP_ITEM_KORATH;
		case 201301: return AP_ITEM_KORATH_YELLOW_GEM;
		case 201302: return AP_ITEM_KORATH_BLUE_GEM_1;
		case 201312: return AP_ITEM_KORATH_BLUE_GEM_2;
		case 201303: return AP_ITEM_KORATH_GREEN_GEM;
		case 201304: return AP_ITEM_KORATH_KEYCARD;
		case 201399: return AP_ITEM_KORATH_GEMSET;
	}
	return -1;
}

void ap_datastorage_set_level(int level, int episode)
{
	if (!ap || ap->get_state() != APClient::State::SLOT_CONNECTED) return;

	int player = ap->get_player_number();
	std::string key = "keen_current_level_" + std::to_string(player);

	nlohmann::json value;
	value["level"] = level;
	value["episode"] = episode;

	APClient::DataStorageOperation op;
	op.operation = "replace";
	op.value = value;

	ap_guard("Set(current_level)", [&]{ ap->Set(key, nlohmann::json(nullptr), false, {op}); });
}

// Parse a key's value as a boolean. Accepts 0/1, true/false, yes/no, on/off
// (case-insensitive, leading whitespace ok). Returns dflt if the value can't
// be recognized.
static bool ap_parse_bool(const char* s, bool dflt)
{
    if (!s) return dflt;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s || *s == '\n' || *s == '\r') return dflt;

    char c = *s;
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    char d = s[1];
    if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');

    if (c == '0' || c == 'f' || c == 'n') return false;
    if (c == '1' || c == 't' || c == 'y') return true;
    if (c == 'o' && d == 'n') return true;
    if (c == 'o' && d == 'f') return false;
    return dflt;
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
            sscanf(line + 7, " %255[^\n]", ap_server);
        }
        else if (strncmp(line, "port:", 5) == 0)
        {
            sscanf(line + 5, "%d", &ap_port);
        }
        else if (strncmp(line, "slotname:", 9) == 0)
        {
            sscanf(line + 9, " %63[^\n]", ap_slotname);
        }
        else if (strncmp(line, "password:", 9) == 0)
        {
            sscanf(line + 9, " %63[^\n]", ap_password);
        }
        else if (strncmp(line, "toasts_received:", 16) == 0)
        {
            ap_toasts_received_enabled = ap_parse_bool(line + 16, ap_toasts_received_enabled);
        }
        else if (strncmp(line, "toasts_sent:", 12) == 0)
        {
            ap_toasts_sent_enabled = ap_parse_bool(line + 12, ap_toasts_sent_enabled);
        }
        else if (strncmp(line, "toasts_deathlink:", 17) == 0)
        {
            ap_toasts_deathlink_enabled = ap_parse_bool(line + 17, ap_toasts_deathlink_enabled);
        }
        else if (strncmp(line, "toasts:", 7) == 0)
        {
            // master switch — checked LAST so the specific keys don't false-match
            ap_toasts_enabled = ap_parse_bool(line + 7, ap_toasts_enabled);
        }
        else if (strncmp(line, "toast_duration:", 15) == 0)
        {
            int secs = ap_toast_duration_secs;
            if (sscanf(line + 15, "%d", &secs) == 1 && secs > 0)
                ap_toast_duration_secs = secs;
        }
    }

    fclose(f);

    FILE *log = fopen("ap_log.txt", "a");
    if (log)
    {
        fprintf(log, "[AP] Server parsed: %s\n", ap_server);
        fprintf(log, "[AP] Port parsed: %d\n", ap_port);
        fprintf(log, "[AP] Slot name parsed: %s\n", ap_slotname);
        fprintf(log, "[AP] Password parsed: %s\n", ap_password);
        fclose(log);
    }
}

void ap_reapply_item(int local_id)
{
	switch(local_id)
	{
		case AP_ITEM_POGO:
			ap_has_pogo = 1;
			break;
		case AP_ITEM_STUNNER:
			ap_has_stunner = 1;
			break;
		case AP_ITEM_WETSUIT:
			if (ap_current_episode == AP_EPISODE_CK4)
				ck_gameState.ep.ck4.wetsuit = 1;
			break;
	}
}

void ap_resync_items()
{
    ap_has_pogo = 0;
    ap_has_stunner = 0;
    memset(ck_gameState.keyGems, 0, sizeof(ck_gameState.keyGems));

	if (ap_current_episode == AP_EPISODE_CK4)
		ck_gameState.ep.ck4.wetsuit = 0;
	if (ap_current_episode == AP_EPISODE_CK5)
		ck_gameState.ep.ck5.securityCard = 0;

    for (int i = 0; i < AP_MAX_ITEMS; i++)
    {
        if (ap_items[i])
            ap_reapply_item(i);
    }

	if (ap_current_level > 0)
		ap_apply_level_items(ap_current_level, ap_current_episode);
}