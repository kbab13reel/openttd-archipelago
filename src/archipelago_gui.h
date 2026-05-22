/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#ifndef ARCHIPELAGO_GUI_H
#define ARCHIPELAGO_GUI_H

#include "archipelago.h"
#include "gfx_type.h"
#include <string>
#include <cstdint>
#include <atomic>
#include <deque>
#include <map>

struct APConsoleEntry {
	std::string text;
	TextColour  colour;
};

void ShowArchipelagoConnectWindow();
void ShowArchipelagoStatusWindow();
void ShowArchipelagoMissionsWindow();
void ShowArchipelagoInventoryWindow();
void ShowArchipelagoShopWindow();
void ShowArchipelagoConsoleWindow();
void AP_ShowConsole(const std::string &msg);

extern std::string _ap_last_host;
extern uint16_t    _ap_last_port;
extern std::string _ap_last_slot;
extern std::string _ap_last_pass;
extern bool        _ap_last_ssl;

extern std::atomic<uint32_t> _ap_status_generation;
extern std::atomic<uint32_t> _ap_console_generation;

/* AP console log — ring buffer of messages pushed by AP_ShowNews and AP_OnPrint */
const std::deque<APConsoleEntry> &AP_GetConsoleLog();
void AP_SendConsoleInput(const std::string &text);

/* Accessor functions from manager */
const APSlotData &AP_GetSlotData();
const std::map<std::string, int> &AP_GetReceivedItemCounts();
const std::map<std::string, std::vector<std::vector<std::string>>> &AP_GetProgressiveTiers();
const std::map<std::string, int> &AP_GetUnlockedTierCounts();
void AP_SaveConnectionConfig();
void AP_LoadConnectionConfig();
void AP_RestoreItemsIndexBeforeConnect();
void AP_EnsureBasesets();
bool              AP_IsConnected();
int64_t AP_GetLiveMissionProgress(const APMission &m);

struct APActiveEffect {
	std::string name;          ///< Short display name (e.g. "Recession", "High Demand")
	int64_t     months_left;   ///< Months remaining (clamped to >= 0)
	int64_t     end_flat_month; ///< Absolute flat econ month when effect expires
};
std::vector<APActiveEffect> AP_GetActiveEffects();

/* World-start handshake — called from intro_gui.cpp ONLY.
 * StartNewGameWithoutGUI must never be called from inside a timer callback. */
void     EnsureHandlersRegistered();
void     AP_SendCheckByName(const std::string &location_name);
bool     AP_ShouldStartWorld();
void     AP_ConsumeWorldStart();   /* applies settings, clears flag */
uint32_t AP_GetWorldSeed();        /* seed to pass to StartNewGameWithoutGUI */

#endif /* ARCHIPELAGO_GUI_H */
