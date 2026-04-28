/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#include "stdafx.h"
#include "core/format.hpp"
#include "core/string_consumer.hpp"
#include "archipelago.h"
#include "archipelago_gui.h"
#include "company_func.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "querystring_gui.h"
#include "textbuf_gui.h"
#include "fontcache.h"
#include "cargotype.h"
#include "currency.h"
#include "fios.h"
#include "network/network.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "safeguards.h"

/* =========================================================================
 * CONNECT WINDOW
 * ========================================================================= */

enum APWidgets : WidgetID {
	WAPGUI_LABEL_SERVER,
	WAPGUI_EDIT_SERVER,
	WAPGUI_LABEL_SLOT,
	WAPGUI_EDIT_SLOT,
	WAPGUI_LABEL_PASS,
	WAPGUI_EDIT_PASS,
	WAPGUI_SEL_MENU_OPTIONS,
	WAPGUI_LABEL_MULT,
	WAPGUI_STATUS,
	WAPGUI_SLOT_INFO,
	WAPGUI_CHECKBOX_MULT,
	WAPGUI_SEL_ACTIONS,
	WAPGUI_BTN_CONNECT_MENU,
	WAPGUI_BTN_CONNECT_INGAME,
	WAPGUI_BTN_DISCONNECT_INGAME,
};

enum APStartFlowWidgets : WidgetID {
	WAPSF_TEXT,
	WAPSF_BTN_GENERATE,
	WAPSF_BTN_LOAD,
	WAPSF_BTN_CANCEL,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SERVER), SetStringTip(STR_ARCHIPELAGO_LABEL_SERVER), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SERVER), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SLOT), SetStringTip(STR_ARCHIPELAGO_LABEL_SLOT), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SLOT), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_PASS), SetStringTip(STR_ARCHIPELAGO_LABEL_PASS), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_PASS), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WAPGUI_SEL_MENU_OPTIONS),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_MULT), SetStringTip(STR_EMPTY), SetMinimalSize(260, 14), SetFill(1, 0),
					NWidget(WWT_BOOLBTN, COLOUR_GREY, WAPGUI_CHECKBOX_MULT), SetStringTip(STR_EMPTY), SetMinimalSize(18, 14), SetFill(0, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL),
					NWidget(WWT_EMPTY, INVALID_COLOUR), SetMinimalSize(0, 0), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WAPGUI_SEL_ACTIONS),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPGUI_BTN_CONNECT_MENU), SetStringTip(STR_ARCHIPELAGO_BTN_CONNECT), SetMinimalSize(220, 14), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPGUI_BTN_CONNECT_INGAME), SetStringTip(STR_ARCHIPELAGO_BTN_CONNECT), SetMinimalSize(108, 14), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WAPGUI_BTN_DISCONNECT_INGAME), SetStringTip(STR_ARCHIPELAGO_BTN_DISCONNECT), SetMinimalSize(108, 14), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_STATUS),    SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_SLOT_INFO), SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
		EndContainer(),
	EndContainer(),
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_start_flow_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPSF_TEXT), SetMinimalSize(320, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPSF_BTN_GENERATE), SetStringTip(STR_EMPTY, STR_NULL), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPSF_BTN_LOAD),     SetStringTip(STR_EMPTY, STR_INTRO_TOOLTIP_LOAD_GAME), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPSF_BTN_CANCEL),   SetStringTip(STR_EMPTY, STR_NULL), SetMinimalSize(102, 14), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoConnectWindow;
static void ShowAPStartFlowChoiceWindow();

struct ArchipelagoConnectWindow : public Window {
	QueryString server_buf;
	QueryString slot_buf;
	QueryString pass_buf;
	std::string server_str, slot_str, pass_str;
	APState  last_state  = APState::DISCONNECTED;
	bool     last_has_sd = false;
	int      last_checked_locations = -1;
	int      last_total_locations = -1;
	bool     menu_multiplayer_mode = false;

	static std::string StatusStr(APState s, bool has_sd, const std::string &err) {
		switch (s) {
			case APState::DISCONNECTED:   return "";
			case APState::CONNECTING:     return "Connecting...";
			case APState::CONNECTED:      return "Authenticating...";
			case APState::AUTHENTICATING: return "Authenticating...";
			case APState::AUTHENTICATED:  return has_sd ? "Connected! Slot data loaded." : "Connected. Waiting for data...";
			case APState::AP_ERROR:       return "Error: " + err;
			default:                      return "...";
		}
	}

	static std::string SlotInfoStr(bool has_sd) {
		if (!has_sd || _ap_client == nullptr) return "";
		APSlotData sd = _ap_client->GetSlotData();

		int total_locations = 0;
		int checked_locations = 0;
		for (const auto &[name, location_id] : sd.location_name_to_id) {
			(void)name;
			total_locations++;
			if (sd.checked_locations.count(location_id) != 0) checked_locations++;
		}

		if (total_locations <= 0) return "Completion: 0% (0/0)";
		const int percent = (checked_locations * 100) / total_locations;
		return fmt::format("Completion: {}% ({}/{})", percent, checked_locations, total_locations);
	}

	void StartConnection(bool start_new_mode)
	{
		if (_ap_client == nullptr) return;

		AP_SetMenuConnectMultiplayer(this->menu_multiplayer_mode);
		AP_SetMenuConnectStartNew(start_new_mode);

		/* Strip any scheme prefix the user may have typed — auto-detect handles it */
		std::string raw = server_str;
		if (raw.rfind("wss://", 0) == 0)    raw = raw.substr(6);
		else if (raw.rfind("ws://", 0) == 0) raw = raw.substr(5);
		std::string host = raw;
		uint16_t port = 38281;
		auto colon = raw.rfind(':');
		if (colon != std::string::npos) {
			host = raw.substr(0, colon);
			int p = ParseInteger<int>(raw.substr(colon + 1)).value_or(0);
			if (p > 0 && p < 65536) port = (uint16_t)p;
		}

		_ap_last_host = host; _ap_last_port = port;
		_ap_last_slot = slot_str; _ap_last_pass = pass_str;
		AP_SaveConnectionConfig(); /* persist for next session */
		_ap_last_ssl = false; /* unused — auto-detect in WorkerThread */
		AP_RestoreItemsIndexBeforeConnect();
		_ap_client->Connect(host, port, slot_str, pass_str, "OpenTTD Cargolock", false);

		if (!start_new_mode && _game_mode == GM_MENU) {
			if (this->menu_multiplayer_mode) {
				/* Host a multiplayer server loaded from a save file.
				 * Setting _is_network_server = true before ShowSaveLoadDialog
				 * makes OpenTTD call NetworkServerStart() automatically when
				 * the save finishes loading (same as the normal Start Server flow). */
				_is_network_server = true;
			}
			ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD);
		}

		this->SetDirty();
	}

	void OnMenuPlayModeChosen(bool multiplayer_mode)
	{
		this->menu_multiplayer_mode = multiplayer_mode;
		ShowAPStartFlowChoiceWindow();
	}

	ArchipelagoConnectWindow(WindowDesc &desc, WindowNumber wnum)
		: Window(desc), server_buf(256), slot_buf(64), pass_buf(64)
	{
		this->CreateNestedTree();
		this->querystrings[WAPGUI_EDIT_SERVER] = &server_buf;
		this->querystrings[WAPGUI_EDIT_SLOT]   = &slot_buf;
		this->querystrings[WAPGUI_EDIT_PASS]   = &pass_buf;
		this->FinishInitNested(wnum);

		/* Restore last connection settings from ap_connection.cfg,
		 * but only when the in-memory state is empty (first window open).
		 * If _ap_last_host is already set (e.g. we're connected, or a second
		 * instance on the same PC just overwrote the cfg), keep the in-memory
		 * values so the window reflects THIS instance's connection. */
		if (_ap_last_host.empty()) AP_LoadConnectionConfig();

		std::string full = _ap_last_host.empty() ? "archipelago.gg:38281"
		                 : _ap_last_host + ":" + fmt::format("{}", _ap_last_port);
		server_buf.text.Assign(full.c_str());
		server_str = full;
		if (!_ap_last_slot.empty()) { slot_buf.text.Assign(_ap_last_slot.c_str()); slot_str = _ap_last_slot; }
		if (!_ap_last_pass.empty()) { pass_buf.text.Assign(_ap_last_pass.c_str()); pass_str = _ap_last_pass; }
	}

	void OnEditboxChanged(WidgetID wid) override {
		switch (wid) {
			case WAPGUI_EDIT_SERVER: server_str = server_buf.text.GetText().data(); break;
			case WAPGUI_EDIT_SLOT:   slot_str   = slot_buf.text.GetText().data();   break;
			case WAPGUI_EDIT_PASS:   pass_str   = pass_buf.text.GetText().data();   break;
		}
	}

	void OnGameTick() override {
		if (_ap_client == nullptr) return;

		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		int checked_locations = -1;
		int total_locations = -1;
		if (h) {
			APSlotData sd = _ap_client->GetSlotData();
			total_locations = (int)sd.location_name_to_id.size();
			checked_locations = 0;
			for (const auto &[name, location_id] : sd.location_name_to_id) {
				(void)name;
				if (sd.checked_locations.count(location_id) != 0) checked_locations++;
			}
		}
		if (s != last_state || h != last_has_sd || checked_locations != last_checked_locations || total_locations != last_total_locations) {
			last_state = s;
			last_has_sd = h;
			last_checked_locations = checked_locations;
			last_total_locations = total_locations;
			this->SetDirty();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (_ap_client == nullptr) return;
		const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
		switch (widget) {
				case WAPGUI_LABEL_MULT:
					DrawString(r.left, r.right, text_y, "Host multiplayer server", TC_BLACK);
					break;
			case WAPGUI_STATUS:
				DrawString(r.left, r.right, r.top,
				    StatusStr(_ap_client->GetState(), _ap_client->HasSlotData(), _ap_client->GetLastError()),
				    TC_BLACK);
				break;
			case WAPGUI_SLOT_INFO:
				DrawString(r.left, r.right, r.top, SlotInfoStr(_ap_client->HasSlotData()), TC_DARK_GREEN);
				break;
			case WAPGUI_BTN_CONNECT_MENU:
			case WAPGUI_BTN_CONNECT_INGAME:
				DrawString(r.left, r.right, text_y, "Connect", TC_BLACK, SA_HOR_CENTER);
				break;
			case WAPGUI_BTN_DISCONNECT_INGAME:
				DrawString(r.left, r.right, text_y, "Disconnect", TC_BLACK, SA_HOR_CENTER);
				break;
		}
	}

	void OnPaint() override
	{
		const bool in_menu = _game_mode == GM_MENU;
		this->GetWidget<NWidgetStacked>(WAPGUI_SEL_MENU_OPTIONS)->SetDisplayedPlane(in_menu ? 0 : 1);
		this->GetWidget<NWidgetStacked>(WAPGUI_SEL_ACTIONS)->SetDisplayedPlane(in_menu ? 0 : 1);
		this->SetWidgetLoweredState(WAPGUI_CHECKBOX_MULT, this->menu_multiplayer_mode);
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPGUI_BTN_CONNECT_MENU:
			case WAPGUI_BTN_CONNECT_INGAME: {
				if (_ap_client == nullptr) break;
				if (_game_mode == GM_MENU) {
					ShowAPStartFlowChoiceWindow();
				} else {
					/* In an active game, reconnect should never trigger world generation. */
					this->menu_multiplayer_mode = _networking;
					this->StartConnection(false);
				}
				break;
			}
			case WAPGUI_CHECKBOX_MULT:
				if (_game_mode == GM_MENU) {
					this->menu_multiplayer_mode = !this->menu_multiplayer_mode;
					this->SetDirty();
				}
				break;
			case WAPGUI_BTN_DISCONNECT_INGAME:
				if (_ap_client) {
					_ap_client->Disconnect();
				}
				this->SetDirty();
				break;
		}
	}
};

struct APStartFlowChoiceWindow : public Window {
	APStartFlowChoiceWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
		if (widget == WAPSF_TEXT) {
			DrawString(r.left, r.right, r.top, "Generate a new AP map or load an AP save?", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_GENERATE) {
			DrawString(r.left, r.right, text_y, "Generate", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_LOAD) {
			DrawString(r.left, r.right, text_y, "Load", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_CANCEL) {
			DrawString(r.left, r.right, text_y, "Cancel", TC_BLACK, SA_HOR_CENTER);
			return;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto *connect = static_cast<ArchipelagoConnectWindow *>(FindWindowById(WC_ARCHIPELAGO, 0));
		if (connect == nullptr) {
			this->Close();
			return;
		}

		switch (widget) {
			case WAPSF_BTN_GENERATE:
				this->Close();
				connect->StartConnection(true);
				break;
			case WAPSF_BTN_LOAD:
				this->Close();
				connect->StartConnection(false);
				break;
			case WAPSF_BTN_CANCEL:
				this->Close();
				break;
		}
	}
};

static WindowDesc _ap_start_flow_desc(
	WDP_CENTER, {"ap_start_flow"}, 360, 96,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_start_flow_widgets
);

static void ShowAPStartFlowChoiceWindow()
{
	CloseWindowById(WC_ARCHIPELAGO, 5);
	AllocateWindowDescFront<APStartFlowChoiceWindow>(_ap_start_flow_desc, 5);
}

static WindowDesc _ap_connect_menu_desc(
	WDP_CENTER, {}, 380, 170,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_widgets
);

static WindowDesc _ap_connect_ingame_desc(
	WDP_CENTER, {}, 380, 152,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_widgets
);

static void ShowArchipelagoConnectWindowWithDesc(WindowDesc &desc)
{
	if (_ap_client == nullptr) InitArchipelago();
	/* Reset runtime-resized dimensions so the chosen default size is applied. */
	desc.pref_width = 0;
	desc.pref_height = 0;
	CloseWindowById(WC_ARCHIPELAGO, 0);
	AllocateWindowDescFront<ArchipelagoConnectWindow>(desc, 0);
}

void ShowArchipelagoConnectWindow()
{
	ShowArchipelagoConnectWindowWithDesc(_ap_connect_menu_desc);
}

static void ShowArchipelagoInGameConnectWindow()
{
	ShowArchipelagoConnectWindowWithDesc(_ap_connect_ingame_desc);
}

/* =========================================================================
 * STATUS WINDOW — persistent top-right overlay
 * ========================================================================= */

enum APStatusWidgets : WidgetID {
	WAPST_STATUS_LINE,
	WAPST_GOAL_LINE,
	WAPST_BTN_MISSIONS,
	WAPST_BTN_INVENTORY,
	WAPST_BTN_SHOP,
	WAPST_SEL_SETTINGS,
	WAPST_BTN_SETTINGS_CONNECTED,
	WAPST_BTN_SETTINGS_DISCONNECTED,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_status_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_ARCHIPELAGO_STATUS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(2, 2, 2), SetPadding(4),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_STATUS_LINE), SetMinimalSize(276, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_GOAL_LINE),   SetMinimalSize(276, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPST_BTN_INVENTORY),  SetStringTip(STR_ARCHIPELAGO_BTN_INVENTORY),  SetMinimalSize(58, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPST_BTN_SHOP),       SetStringTip(STR_ARCHIPELAGO_BTN_SHOP),       SetMinimalSize(58, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPST_BTN_MISSIONS),   SetStringTip(STR_ARCHIPELAGO_BTN_MISSIONS),   SetMinimalSize(58, 14),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WAPST_SEL_SETTINGS),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPST_BTN_SETTINGS_CONNECTED), SetStringTip(STR_EMPTY), SetMinimalSize(74, 14),
					NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WAPST_BTN_SETTINGS_DISCONNECTED), SetStringTip(STR_EMPTY), SetMinimalSize(74, 14),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoStatusWindow : public Window {
	APState  last_state  = APState::DISCONNECTED;
	bool     last_has_sd = false;
	uint32_t last_gen    = UINT32_MAX;

	ArchipelagoStatusWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	static std::string StatusLine() {
		if (_ap_client == nullptr) return "AP: No client";
		const std::string slot = _ap_last_slot.empty() ? std::string("?") : _ap_last_slot;
		switch (_ap_client->GetState()) {
			case APState::AUTHENTICATED:  return "AP: Connected (slot: " + slot + ")";
			case APState::CONNECTING:     return "AP: Connecting...";
			case APState::AUTHENTICATING: return "AP: Authenticating...";
			case APState::AP_ERROR:       return "AP: Error - " + _ap_client->GetLastError();
			case APState::DISCONNECTED:   return "AP: Disconnected";
			default:                      return "AP: ...";
		}
	}

	static std::string GoalLine() {
		if (_ap_client == nullptr || !_ap_client->HasSlotData()) return "No slot data";

		/* New AP world goal: collect all 11 cargo unlock items. */
		static constexpr const char *CARGO_WIN_ITEMS[] = {
			"Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods",
			"Grain", "Wood", "Iron Ore", "Steel", "Valuables"
		};
		const std::map<std::string, int> &counts = AP_GetReceivedItemCounts();
		int collected = 0;
		for (const char *cargo : CARGO_WIN_ITEMS) {
			if (counts.count(cargo) > 0) collected++;
		}
		return fmt::format("Goal: Cargo unlocks {}/11", collected);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		uint32_t g = _ap_status_generation.load(std::memory_order_relaxed);
		bool     d = (g != last_gen); last_gen = g;
		if (s != last_state || h != last_has_sd || d) {
			last_state = s; last_has_sd = h;
			this->SetDirty();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		switch (widget) {
			case WAPST_STATUS_LINE: {
				bool ok = (_ap_client && _ap_client->GetState() == APState::AUTHENTICATED);
				DrawString(r.left, r.right, r.top, StatusLine(), ok ? TC_GREEN : TC_RED);
				break;
			}
			case WAPST_GOAL_LINE:
				DrawString(r.left, r.right, r.top, GoalLine(), TC_GOLD);
				break;
			case WAPST_BTN_SETTINGS_CONNECTED:
			case WAPST_BTN_SETTINGS_DISCONNECTED:
				DrawString(r.left, r.right,
				    r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2),
				    "AP Settings", TC_BLACK, SA_HOR_CENTER);
				break;
		}
	}

	void OnPaint() override {
		bool connected = AP_IsConnected();
		const bool shop_enabled = connected && AP_GetSlotData().enable_shop && !AP_GetSlotData().shop_locations.empty();
		this->GetWidget<NWidgetStacked>(WAPST_SEL_SETTINGS)->SetDisplayedPlane(connected ? 0 : 1);
		this->SetWidgetDisabledState(WAPST_BTN_MISSIONS, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_INVENTORY, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_SHOP, !shop_enabled);
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPST_BTN_MISSIONS:
				ShowArchipelagoMissionsWindow();
				break;
			case WAPST_BTN_INVENTORY:
				ShowArchipelagoInventoryWindow();
				break;
			case WAPST_BTN_SHOP:
				ShowArchipelagoShopWindow();
				break;
			case WAPST_BTN_SETTINGS_CONNECTED:
			case WAPST_BTN_SETTINGS_DISCONNECTED:
				ShowArchipelagoInGameConnectWindow();
				break;
		}
	}
};

static WindowDesc _ap_status_desc(
	WDP_AUTO, {"ap_status"}, 284, 82,
	WC_ARCHIPELAGO_TICKER, WC_NONE, {},
	_nested_ap_status_widgets
);

void ShowArchipelagoStatusWindow()
{
	AllocateWindowDescFront<ArchipelagoStatusWindow>(_ap_status_desc, 0);
}

/* =========================================================================
 * MISSIONS WINDOW (Bug F fix)
 *
 * Scrollable list of all missions from slot_data.
 * Shows: description and completed/pending status.
 * ========================================================================= */

enum APMissionsWidgets : WidgetID {
	WAPM_CAPTION,
	WAPM_SCROLLBAR,
	WAPM_HSCROLLBAR,
	WAPM_LIST,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_missions_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_MISSIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		/* Mission list + vertical scrollbar */
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WAPM_LIST), SetMinimalSize(220, 120), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPM_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPM_SCROLLBAR),
		EndContainer(),
		/* Horizontal scrollbar + resize box */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HSCROLLBAR, COLOUR_BROWN, WAPM_HSCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/* ---------------------------------------------------------------------------
 * Shared currency formatting helpers (used by mission UI text).
 * Uses GetCurrency() so the symbol and rate are always in sync with game settings.
 * --------------------------------------------------------------------------- */

/** Compact money string: "$50M", "£200k", "1,5M kr" etc. */
static std::string AP_FormatMoneyCompact(int64_t amount)
{
	const CurrencySpec &cs = GetCurrency();
	int64_t scaled = amount * cs.rate;

	std::string num;
	if (scaled >= 1000000) num = fmt::format("{:.1f}M", scaled / 1000000.0);
	else if (scaled >= 1000) num = fmt::format("{}k", scaled / 1000);
	else num = fmt::format("{}", scaled);

	if (cs.symbol_pos == 0) return cs.prefix + num + cs.suffix;
	return num + cs.suffix;
}

/** Resolve a cargo AP name (e.g. "Coal") to its CargoSpec, or nullptr if not found. */
static const CargoSpec *AP_GetCargoSpecByName(const std::string &name)
{
	if (name.empty()) return nullptr;
	std::string lower = name;
	for (char &c : lower) c = (char)tolower((unsigned char)c);
	for (const CargoSpec *cs : _sorted_cargo_specs) {
		if (!cs->IsValid()) continue;
		std::string cs_name = GetString(cs->name);
		for (char &c : cs_name) c = (char)tolower((unsigned char)c);
		if (cs_name == lower) return cs;
	}
	return nullptr;
}

struct ArchipelagoMissionsWindow : public Window {
	/* A single display row — either a section/cargo header or a mission entry. */
	struct MissionRow {
		enum Type { SECTION_HEADER, CARGO_HEADER, MISSION } type;
		std::string      text;       ///< header display text
		const APMission *mission;    ///< MISSION rows only
		std::string      label;      ///< "by Train" / "by Ship" / "1k" / description
		CargoType        cargo_type = INVALID_CARGO; ///< cargo icon (CARGO_HEADER only)
		int64_t          delivered  = 0;             ///< units delivered so far (CARGO_HEADER only)
	};

	int row_height  = 0;
	int max_line_px = 0;
	std::vector<MissionRow> rows;
	Scrollbar *scrollbar  = nullptr;
	Scrollbar *hscrollbar = nullptr;
	uint32_t  last_gen    = UINT32_MAX;

	static std::string VehicleKeyDisplay(const std::string &key)
	{
		if (key == "train")        return "Train";
		if (key == "road_vehicle") return "Road Vehicle";
		if (key == "ship")         return "Ship";
		if (key == "aircraft")     return "Aircraft";
		return key;
	}

	static int VehicleKeyOrder(const std::string &key)
	{
		if (key == "train")        return 0;
		if (key == "road_vehicle") return 1;
		if (key == "ship")         return 2;
		if (key == "aircraft")     return 3;
		return 4;
	}

	static std::string FmtNum(int64_t v)
	{
		if (v >= 1000000 && v % 1000000 == 0) return fmt::format("{}M", v / 1000000);
		if (v >= 1000000)                      return fmt::format("{:.1f}M", v / 1000000.0);
		if (v >= 1000    && v % 1000    == 0)  return fmt::format("{}k", v / 1000);
		if (v >= 1000)                         return fmt::format("{:.1f}k", v / 1000.0);
		return fmt::format("{}", v);
	}

	void RebuildRows()
	{
		rows.clear();
		const APSlotData &sd = AP_GetSlotData();

		/* Bucket each mission into: general, per-cargo vehicle, per-cargo tiered. */
		static const char * const CARGO_ORDER[] = {
			"Passengers", "Mail", "Valuables", "Coal", "Oil", "Livestock",
			"Grain", "Wood", "Iron Ore", "Steel", "Goods",
		};

		std::map<std::string, std::vector<const APMission *>> cv_map;   /* transport_by_vehicle */
		std::map<std::string, std::vector<const APMission *>> tier_map; /* transport */
		std::vector<const APMission *> general;

		for (const APMission &m : sd.missions) {
			if (m.type == "transport_by_vehicle") {
				cv_map[m.cargo].push_back(&m);
			} else if (m.type == "transport" && !m.cargo.empty()) {
				tier_map[m.cargo].push_back(&m);
			} else {
				general.push_back(&m);
			}
		}

		/* Sort vehicle missions by canonical vehicle order within each cargo. */
		for (auto &[cargo, vec] : cv_map) {
			std::sort(vec.begin(), vec.end(), [](const APMission *a, const APMission *b) {
				return VehicleKeyOrder(a->vehicle_key) < VehicleKeyOrder(b->vehicle_key);
			});
		}

		/* Sort tiered missions ascending by amount. */
		for (auto &[cargo, vec] : tier_map) {
			std::sort(vec.begin(), vec.end(), [](const APMission *a, const APMission *b) {
				return a->amount < b->amount;
			});
		}

		/* ── General section ──────────────────────────────────────────────── */
		if (!general.empty()) {
			rows.push_back({ MissionRow::SECTION_HEADER, "— Missions —", nullptr, "" });
			for (const APMission *m : general) {
				std::string lbl = m->description;
				if (m->named_entity.tile != UINT32_MAX) lbl += " \xe2\x86\x91";
				rows.push_back({ MissionRow::MISSION, "", m, std::move(lbl) });
			}
		}

		/* ── Per-cargo sections ───────────────────────────────────────────── */
		for (const char *cargo : CARGO_ORDER) {
			auto vit = cv_map.find(cargo);
			auto tit = tier_map.find(cargo);
			bool has_v = vit != cv_map.end()   && !vit->second.empty();
			bool has_t = tit != tier_map.end() && !tit->second.empty();
			if (!has_v && !has_t) continue;

			{
				const CargoSpec *cs = AP_GetCargoSpecByName(cargo);
				CargoType ct = (cs != nullptr) ? (CargoType)cs->Index() : INVALID_CARGO;
				int64_t delivered = has_t ? tit->second[0]->current_value : 0;
				MissionRow hdr;
				hdr.type       = MissionRow::CARGO_HEADER;
				hdr.text       = fmt::format("— {} —", cargo);
				hdr.mission    = nullptr;
				hdr.label      = "";
				hdr.cargo_type = ct;
				hdr.delivered  = delivered;
				rows.push_back(std::move(hdr));
			}

			/* Vehicle missions first (each is "tier 0" — just deliver 1 unit by that mode). */
			if (has_v) {
				for (const APMission *m : vit->second) {
					rows.push_back({ MissionRow::MISSION, "", m,
					    fmt::format("by {}", VehicleKeyDisplay(m->vehicle_key)) });
				}
			}
			/* Tiered volume missions in ascending order. */
			if (has_t) {
				for (const APMission *m : tit->second) {
					rows.push_back({ MissionRow::MISSION, "", m, FmtNum(m->amount) });
				}
			}
		}

		/* Compute max line pixel width for hscrollbar range. */
		const int icon_slot = (int)GetLargestCargoIconSize().width + 4;
		const int dash_w = (int)GetStringBoundingBox("\u2014 ").width + (int)GetStringBoundingBox(" \u2014").width;
		max_line_px = 0;
		for (const auto &row : rows) {
			std::string line;
			if (row.type == MissionRow::CARGO_HEADER) {
				line = row.text;
				if (row.delivered > 0) line += fmt::format("  {}", FmtNum(row.delivered));
			} else if (row.type == MissionRow::SECTION_HEADER) {
				line = row.text;
			} else if (row.mission) {
				line = (row.mission->completed ? "[X] " : "[ ] ") + row.label;
			}
			/* Mission rows indented 8 px; cargo headers: two icon slots + dashes on each side. */
			int indent = (row.type == MissionRow::MISSION) ? 8 : 0;
			int extra = (row.type == MissionRow::CARGO_HEADER && IsValidCargoType(row.cargo_type)) ? (icon_slot * 2 + dash_w + 8) : 0;
			int w = GetStringBoundingBox(line).width + indent + extra;
			if (w > max_line_px) max_line_px = w;
		}

		if (this->scrollbar) this->scrollbar->SetCount((int)rows.size());
		UpdateHScrollbar();
		this->SetDirty();
	}

	void UpdateHScrollbar()
	{
		if (!this->hscrollbar) return;
		NWidgetBase *nw = this->GetWidget<NWidgetBase>(WAPM_LIST);
		int visible_w = (nw != nullptr) ? (int)nw->current_x - 8 : 460;
		int total = std::max(max_line_px + 16, visible_w);
		this->hscrollbar->SetCount(total);
		this->hscrollbar->SetCapacity(visible_w);
	}

	ArchipelagoMissionsWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3;
		this->CreateNestedTree();
		this->scrollbar  = this->GetScrollbar(WAPM_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->hscrollbar = this->GetScrollbar(WAPM_HSCROLLBAR);
		this->hscrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		this->resize.step_width  = 1;
		RebuildRows();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		uint32_t g = _ap_status_generation.load(std::memory_order_relaxed);
		if (g != last_gen) { last_gen = g; RebuildRows(); this->SetDirty(); }
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget != WAPM_LIST) return;
		int rh = GetCharacterHeight(FS_NORMAL) + 3;
		const Rect &rect = this->GetWidget<NWidgetBase>(WAPM_LIST)->GetCurrentRect();
		int idx = this->scrollbar->GetPosition() + (pt.y - rect.top - 2) / rh;
		if (idx < 0 || idx >= (int)rows.size()) return;
		const auto &mr = rows[idx];
		if (mr.type == MissionRow::MISSION && mr.mission &&
		    mr.mission->named_entity.tile != UINT32_MAX) {
			ScrollMainWindowToTile(TileIndex{mr.mission->named_entity.tile});
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPM_LIST) return;

		int rh    = GetCharacterHeight(FS_NORMAL) + 3;
		int y     = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)rows.size(); i++) {
			const auto &row = rows[i];
			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;

			if (row.type == MissionRow::SECTION_HEADER) {
				DrawString(r.left + 4 + x_off, r.right + max_line_px, y,
				    row.text, TC_YELLOW, SA_LEFT | SA_FORCE);
			} else if (row.type == MissionRow::CARGO_HEADER) {
				std::string header = row.text;
				if (row.delivered > 0) header += fmt::format("  {}", FmtNum(row.delivered));
				int cur_x = r.left + 4 + x_off;
				if (IsValidCargoType(row.cargo_type)) {
					const CargoSpec *cs = CargoSpec::Get(row.cargo_type);
					SpriteID spr = cs->GetCargoIcon();
					Dimension max_d = GetLargestCargoIconSize();
					Dimension d = (spr != 0) ? GetSpriteSize(spr) : Dimension{0, 0};
					/* "— " prefix */
					cur_x = DrawString(cur_x, r.right + max_line_px, y, "\u2014 ", TC_ORANGE, SA_LEFT | SA_FORCE) + 2;
					/* left icon */
					if (spr != 0) DrawSprite(spr, PAL_NONE,
						CentreBounds(cur_x, cur_x + (int)max_d.width, d.width),
						CentreBounds(y, y + rh, d.height));
					cur_x += (int)max_d.width + 4;
					/* cargo name */
					cur_x = DrawString(cur_x, r.right + max_line_px, y, header, TC_ORANGE, SA_LEFT | SA_FORCE) + 4;
					/* right icon */
					if (spr != 0) DrawSprite(spr, PAL_NONE,
						CentreBounds(cur_x, cur_x + (int)max_d.width, d.width),
						CentreBounds(y, y + rh, d.height));
					cur_x += (int)max_d.width + 2;
					/* " —" suffix */
					DrawString(cur_x, r.right + max_line_px, y, " \u2014", TC_ORANGE, SA_LEFT | SA_FORCE);
				} else {
					DrawString(cur_x, r.right + max_line_px, y, header, TC_ORANGE, SA_LEFT | SA_FORCE);
				}
			} else {
				const APMission *m = row.mission;
				if (!m) { y += rh; continue; }

				std::string line = (m->completed ? "[X] " : "[ ] ") + row.label;
				TextColour tc = m->completed ? TC_DARK_GREEN : TC_WHITE;
				DrawString(r.left + 4 + 8 + x_off, r.right + max_line_px, y,
				    line, tc, SA_LEFT | SA_FORCE);
			}
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override
	{
		UpdateHScrollbar();
		this->SetDirty();
	}

	void OnResize() override
	{
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPM_LIST)->current_y / rh);
		}
		UpdateHScrollbar();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPM_LIST) {
			resize.height = row_height;
			resize.width  = 1;
			size.height = std::max(size.height, (uint)(row_height * 15));
		}
	}
};

static WindowDesc _ap_missions_desc(
	WDP_AUTO, {"ap_missions"}, 480, 340,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_missions_widgets
);

void ShowArchipelagoMissionsWindow()
{
	AllocateWindowDescFront<ArchipelagoMissionsWindow>(_ap_missions_desc, 2);
}

/* =========================================================================
 * INVENTORY WINDOW — all AP items received this session
 * ========================================================================= */

enum APInventoryWidgets : WidgetID {
	WAPINV_CAPTION,
	WAPINV_LIST,
	WAPINV_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_inventory_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WAPINV_CAPTION), SetStringTip(STR_ARCHIPELAGO_INVENTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WAPINV_LIST), SetMinimalSize(220, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPINV_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WAPINV_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

/** All cargo types in display order. */
static const char * const AP_CARGO_ORDER[] = {
	"Passengers", "Mail", "Valuables", "Coal", "Oil", "Livestock",
	"Grain", "Wood", "Iron Ore", "Steel", "Goods",
};
/** Utility items in display order. */
static const char * const AP_UTILITY_ORDER[] = {
	"Bridges", "Tunnels", "Canals", "Terraforming",
};
static const char * const AP_VEHICLE_ORDER[] = {
	"Progressive Trains", "Progressive Road Vehicles",
	"Progressive Aircrafts", "Progressive Ships",
};

struct ArchipelagoInventoryWindow : public Window {
	struct InventoryRow {
		enum Type { HEADER, VEHICLE, VEHICLE_TIER, VEHICLE_NAME, CARGO, UTILITY, FILLER } type;
		std::string text;
		bool owned = false;
		int  count = 0;
		CargoType cargo_type = INVALID_CARGO; ///< resolved CargoType for CARGO rows
	};

	std::vector<InventoryRow> rows;
	Scrollbar *scrollbar = nullptr;
	int row_height = 0;
	std::map<std::string, bool> expanded; /* vehicle item name → fold state */
	uint32_t last_gen = UINT32_MAX;

	void RebuildRows()
	{
		rows.clear();
		const std::map<std::string, int> &counts = AP_GetReceivedItemCounts();

		/* ── Vehicles ──────────────────────────────────────────── */
		bool any_vehicle = false;
		for (const char *name : AP_VEHICLE_ORDER) {
			auto it = counts.find(name);
			if (it != counts.end() && it->second > 0) { any_vehicle = true; break; }
		}
		if (any_vehicle) {
			rows.push_back({ InventoryRow::HEADER, "— Progression —" });
			const auto &prog_tiers  = AP_GetProgressiveTiers();
			const auto &tier_counts = AP_GetUnlockedTierCounts();
			for (const char *name : AP_VEHICLE_ORDER) {
				auto it = counts.find(name);
				if (it == counts.end() || it->second == 0) continue;
				rows.push_back({ InventoryRow::VEHICLE, name, true, it->second });
				/* Emit tier sub-rows only when the item is expanded */
				auto exp_it = expanded.find(name);
				bool is_expanded = (exp_it != expanded.end()) && exp_it->second;
				if (is_expanded) {
					auto tier_def = prog_tiers.find(name);
					auto tier_cnt = tier_counts.find(name);
					if (tier_def != prog_tiers.end() && tier_cnt != tier_counts.end()) {
						int unlocked = tier_cnt->second;
						for (int t = 0; t < unlocked && t < (int)tier_def->second.size(); t++) {
							std::string tier_label = fmt::format("Tier {}", t + 1);
							rows.push_back({ InventoryRow::VEHICLE_TIER, tier_label, true, t + 1 });
							for (const std::string &veh : tier_def->second[t]) {
								rows.push_back({ InventoryRow::VEHICLE_NAME, veh, true, 0 });
							}
						}
					}
				}
			}
		}

		/* ── Cargo ─────────────────────────────────────────────── */
		int unlocked_cargo = 0;
		for (const char *name : AP_CARGO_ORDER) {
			bool owned = counts.count(name) > 0;
			if (owned) unlocked_cargo++;
		}
		rows.push_back({ InventoryRow::HEADER, fmt::format("— Cargo ({}/{}) —", unlocked_cargo, (int)std::size(AP_CARGO_ORDER)) });
		for (const char *name : AP_CARGO_ORDER) {
			bool owned = counts.count(name) > 0;
			const CargoSpec *cs = AP_GetCargoSpecByName(name);
			CargoType ct = (cs != nullptr) ? (CargoType)cs->Index() : INVALID_CARGO;
			rows.push_back({ InventoryRow::CARGO, name, owned, owned ? 1 : 0, ct });
		}

		/* ── Utilities ──────────────────────────────────────────── */
		{
			const APSlotData &sd = AP_GetSlotData();
			const bool any_locked = sd.lock_bridges || sd.lock_tunnels || sd.lock_canals || sd.lock_terraforming;
			if (any_locked) {
				const bool locked_arr[4] = { sd.lock_bridges, sd.lock_tunnels, sd.lock_canals, sd.lock_terraforming };
				rows.push_back({ InventoryRow::HEADER, "— Utilities —" });
				for (int i = 0; i < 4; i++) {
					if (!locked_arr[i]) continue;
					bool owned = counts.count(AP_UTILITY_ORDER[i]) > 0;
					rows.push_back({ InventoryRow::UTILITY, AP_UTILITY_ORDER[i], owned, owned ? 1 : 0 });
				}
			}
		}

		/* ── Filler ────────────────────────────────────────────── */
		static const char * const FILLER_ITEMS[] = { "Cash Injection", "Choo chooo!" };
		bool any_filler = false;
		for (const char *name : FILLER_ITEMS) {
			auto it = counts.find(name);
			if (it != counts.end() && it->second > 0) { any_filler = true; break; }
		}
		if (any_filler) {
			rows.push_back({ InventoryRow::HEADER, "— Filler —" });
			for (const char *name : FILLER_ITEMS) {
				auto it = counts.find(name);
				if (it != counts.end() && it->second > 0) {
					rows.push_back({ InventoryRow::FILLER, name, true, it->second });
				}
			}
		}

		if (this->scrollbar) this->scrollbar->SetCount((int)rows.size());
		this->SetDirty();
	}

	ArchipelagoInventoryWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3;
		this->CreateNestedTree();
		this->scrollbar = this->GetScrollbar(WAPINV_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		RebuildRows();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		uint32_t g = _ap_status_generation.load(std::memory_order_relaxed);
		if (g != last_gen) { last_gen = g; RebuildRows(); this->SetDirty(); }
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPINV_LIST) return;

		int rh    = GetCharacterHeight(FS_NORMAL) + 3;
		int y     = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)rows.size(); i++) {
			const InventoryRow &row = rows[i];
			switch (row.type) {
				case InventoryRow::HEADER:
					DrawString(r.left + 4, r.right - 4, y, row.text, TC_GOLD, SA_CENTER | SA_FORCE);
					break;

				case InventoryRow::VEHICLE: {
					auto exp_it = expanded.find(row.text);
					bool is_exp = (exp_it != expanded.end()) && exp_it->second;
					const int total_tiers = AP_GetProgressiveTiers().count(row.text)
					    ? (int)AP_GetProgressiveTiers().at(row.text).size() : row.count;
					std::string arrow = is_exp ? "[-] " : "[+] ";
					std::string line  = fmt::format("{}{} ({} / {} tiers)", arrow, row.text, row.count, total_tiers);
					DrawString(r.left + 8, r.right - 4, y, line, TC_YELLOW, SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::VEHICLE_TIER: {
					/* Alternate colours by tier number so they're visually distinct */
					static constexpr TextColour TIER_COLOURS[] = {
					    TC_LIGHT_BLUE, TC_GREEN, TC_YELLOW, TC_ORANGE, TC_RED
					};
					int idx = std::max(0, row.count - 1) % 5;
					DrawString(r.left + 20, r.right - 4, y, row.text, TIER_COLOURS[idx], SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::VEHICLE_NAME:
					DrawString(r.left + 36, r.right - 4, y, row.text, TC_WHITE, SA_LEFT | SA_FORCE);
					break;

				case InventoryRow::CARGO: {
					TextColour tc = row.owned ? TC_GREEN : TC_GREY;
					std::string marker = row.owned ? "[X]" : "[ ]";
					std::string line = marker + " " + row.text;
					int text_right = DrawString(r.left + 8, r.right - 4, y, line, tc, SA_LEFT | SA_FORCE);
					if (IsValidCargoType(row.cargo_type)) {
						const CargoSpec *cs = CargoSpec::Get(row.cargo_type);
						SpriteID spr = cs->GetCargoIcon();
						if (spr != 0) {
							Dimension max_d = GetLargestCargoIconSize();
							Dimension d = GetSpriteSize(spr);
							int slot_left = text_right + 4;
							DrawSprite(spr, PAL_NONE,
								CentreBounds(slot_left, slot_left + (int)max_d.width, d.width),
								CentreBounds(y, y + rh, d.height));
						}
					}
					break;
				}
				case InventoryRow::UTILITY: {
					TextColour tc = row.owned ? TC_GREEN : TC_GREY;
					std::string marker = row.owned ? "[X]" : "[ ]";
					std::string line = marker + " " + row.text;
					DrawString(r.left + 8, r.right - 4, y, line, tc, SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::FILLER: {
					std::string line = row.text;
					if (row.count > 1) line += fmt::format("  x{}", row.count);
					DrawString(r.left + 8, r.right - 4, y, line, TC_SILVER, SA_LEFT | SA_FORCE);
					break;
				}
			}
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget != WAPINV_LIST) return;

		int rh = GetCharacterHeight(FS_NORMAL) + 3;
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WAPINV_LIST);
		int rel_y = pt.y - wid->pos_y - 2;
		if (rel_y < 0) return;

		int clicked_row = this->scrollbar->GetPosition() + rel_y / rh;
		if (clicked_row < 0 || clicked_row >= (int)rows.size()) return;

		const InventoryRow &row = rows[clicked_row];
		if (row.type == InventoryRow::VEHICLE) {
			bool is_expanded = expanded.count(row.text) != 0 && expanded.at(row.text);
			expanded[row.text] = !is_expanded;
			RebuildRows();
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	void OnResize() override
	{
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPINV_LIST)->current_y / rh);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPINV_LIST) {
			resize.height = row_height;
			resize.width  = 1;
			size.height   = std::max(size.height, (uint)(row_height * 18));
		}
	}
};

static WindowDesc _ap_inventory_desc(
	WDP_AUTO, {"ap_inventory"}, 300, 360,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_inventory_widgets
);

void ShowArchipelagoInventoryWindow()
{
	AllocateWindowDescFront<ArchipelagoInventoryWindow>(_ap_inventory_desc, 3);
}

/* =========================================================================
 * SHOP WINDOW — AP checks bought with in-game money
 * ========================================================================= */

enum APShopWidgets : WidgetID {
	WAPSHOP_CAPTION,
	WAPSHOP_SUMMARY,
	WAPSHOP_LIST,
	WAPSHOP_SCROLLBAR,
	WAPSHOP_BTN_BUY,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_shop_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY, WAPSHOP_CAPTION), SetTextStyle(TC_WHITE), SetStringTip(STR_ARCHIPELAGO_SHOP_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_SHADEBOX, COLOUR_GREY),
		NWidget(WWT_DEFSIZEBOX, COLOUR_GREY),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(NWID_VERTICAL),
		NWidget(WWT_PANEL, COLOUR_GREY, WAPSHOP_SUMMARY), SetMinimalSize(280, 18), SetFill(1, 0), SetResize(1, 0), SetStringTip(STR_EMPTY), EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WAPSHOP_LIST), SetMinimalSize(280, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPSHOP_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_GREY, WAPSHOP_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PUSHTXTBTN, COLOUR_GREY, WAPSHOP_BTN_BUY), SetMinimalSize(72, 14), SetResize(1, 0), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_RESIZEBOX, COLOUR_GREY),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoShopWindow : public Window {
	std::vector<const APShopLocation *> rows;
	Scrollbar *scrollbar = nullptr;
	int row_height = 0;
	int selected_row = -1;
	int      last_visible_count = -1;
	int      last_total_shop_count = -1;
	Money    last_money = INT64_MIN;
	int      last_purchased_visible_count = -1;
	uint32_t last_gen = UINT32_MAX;

	void RebuildRows()
	{
		/* Build in a temporary vector to avoid iterator invalidation during DrawWidget iteration */
		std::vector<const APShopLocation *> new_rows;
		const APSlotData &sd = AP_GetSlotData();
		const int visible_count = AP_GetVisibleShopLocationCount();
		for (int i = 0; i < visible_count && i < (int)sd.shop_locations.size(); i++) {
			new_rows.push_back(&sd.shop_locations[i]);
		}
		/* Atomically swap the rows — DrawWidget will see a consistent list */
		rows = new_rows;

		if (rows.empty()) {
			selected_row = -1;
		} else if (selected_row >= (int)rows.size()) {
			selected_row = (int)rows.size() - 1;
		}
		if (this->scrollbar != nullptr) this->scrollbar->SetCount((int)rows.size());
		this->SetDirty();
	}

	const APShopLocation *GetSelectedShop() const
	{
		if (selected_row < 0 || selected_row >= (int)rows.size()) return nullptr;
		return rows[selected_row];
	}

	bool CanBuySelectedShop() const
	{
		const APShopLocation *shop = GetSelectedShop();
		if (shop == nullptr) return false;
		if (AP_IsShopLocationPurchased(shop->location)) return false;
		return GetAvailableMoney(_local_company) >= (Money)shop->cost;
	}

	ArchipelagoShopWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 4;
		this->CreateNestedTree();
		this->scrollbar = this->GetScrollbar(WAPSHOP_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		RebuildRows();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		const APSlotData &sd = AP_GetSlotData();
		const int visible_count = AP_GetVisibleShopLocationCount();
		const int total_shop_count = (int)sd.shop_locations.size();
		const Money money = GetAvailableMoney(_local_company);
		int purchased_visible_count = 0;
		for (int i = 0; i < visible_count && i < total_shop_count; i++) {
			if (AP_IsShopLocationPurchased(sd.shop_locations[i].location)) purchased_visible_count++;
		}

		uint32_t g = _ap_status_generation.load(std::memory_order_relaxed);
		const bool status_changed = (g != last_gen); last_gen = g;
		if (status_changed ||
		    visible_count != last_visible_count ||
		    total_shop_count != last_total_shop_count ||
		    money != last_money ||
		    purchased_visible_count != last_purchased_visible_count) {
			last_visible_count = visible_count;
			last_total_shop_count = total_shop_count;
			last_money = money;
			last_purchased_visible_count = purchased_visible_count;
			RebuildRows();
			this->SetDirty();
		}
	}

	void OnPaint() override
	{
		this->SetWidgetDisabledState(WAPSHOP_BTN_BUY, !CanBuySelectedShop());
		this->DrawWidgets();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget == WAPSHOP_SUMMARY) {
			const APSlotData &sd = AP_GetSlotData();
			Money money = GetAvailableMoney(_local_company);
			const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
			DrawString(r.left + 6, r.right - 6, text_y,
			    fmt::format("Visible: {}/{}   Cash: {}", AP_GetVisibleShopLocationCount(), sd.shop_locations.size(), AP_FormatMoneyCompact(money)),
			    TC_GOLD);
			return;
		}

		if (widget == WAPSHOP_BTN_BUY) {
			const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
			DrawString(r.left, r.right, text_y, "Buy", TC_BLACK, SA_HOR_CENTER);
			return;
		}

		if (widget != WAPSHOP_LIST) return;

		int rh = GetCharacterHeight(FS_NORMAL) + 4;
		int y = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last = first + (r.Height() / rh) + 1;
		Money money = GetAvailableMoney(_local_company);

		for (int i = first; i < last && i < (int)rows.size(); i++) {
			const APShopLocation *shop = rows[i];
			/* Determine purchase state: check both explicitly purchased set AND checked locations */
			bool purchased = AP_IsShopLocationPurchased(shop->location);
			if (!purchased) {
				/* Also check if this location was checked by AP (authoritative source) */
				const APSlotData &sd = AP_GetSlotData();
				auto id_it = sd.location_name_to_id.find(shop->location);
				if (id_it != sd.location_name_to_id.end()) {
					purchased = (sd.checked_locations.count(id_it->second) != 0);
				}
			}
			const bool affordable = money >= (Money)shop->cost;
			const std::string selection = (i == selected_row) ? ">" : " ";
			const std::string marker = purchased ? "[X]" : "[ ]";
			const std::string line = fmt::format("{} {} {} - {}", selection, marker, shop->name, AP_FormatMoneyCompact(shop->cost));
			TextColour colour = purchased ? TC_DARK_GREEN : (affordable ? ((i == selected_row) ? TC_YELLOW : TC_WHITE) : TC_GREY);
			DrawString(r.left + 6, r.right - 4, y, line, colour, SA_LEFT | SA_FORCE);
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget == WAPSHOP_BTN_BUY) {
			const APShopLocation *shop = GetSelectedShop();
			if (shop != nullptr && AP_PurchaseShopLocation(shop->location)) RebuildRows();
			return;
		}

		if (widget != WAPSHOP_LIST) return;

		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WAPSHOP_LIST);
		int rel_y = pt.y - wid->pos_y - 2;
		if (rel_y < 0) return;

		int clicked_row = this->scrollbar->GetPosition() + rel_y / (GetCharacterHeight(FS_NORMAL) + 4);
		if (clicked_row < 0 || clicked_row >= (int)rows.size()) return;

		selected_row = clicked_row;
		this->SetDirty();
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	void OnResize() override
	{
		if (this->scrollbar != nullptr) {
			int rh = GetCharacterHeight(FS_NORMAL) + 4;
			this->scrollbar->SetCapacity(this->GetWidget<NWidgetBase>(WAPSHOP_LIST)->current_y / rh);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPSHOP_LIST) {
			resize.height = row_height;
			resize.width = 1;
			size.height = std::max(size.height, (uint)(row_height * 14));
		}
	}
};

static WindowDesc _ap_shop_desc(
	WDP_AUTO, {"ap_shop"}, 360, 320,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_shop_widgets
);

void ShowArchipelagoShopWindow()
{
	const APSlotData &sd = AP_GetSlotData();
	if (!sd.enable_shop || sd.shop_locations.empty()) return;
	AllocateWindowDescFront<ArchipelagoShopWindow>(_ap_shop_desc, 6);
}
