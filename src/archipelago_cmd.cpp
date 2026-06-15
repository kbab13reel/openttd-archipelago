/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file archipelago_cmd.cpp Command handlers for Archipelago synchronized state. */

#include "stdafx.h"
#include <charconv>
#include "archipelago_cmd.h"
#include "company_base.h"
#include "cargotype.h"
#include "command_type.h"
#include "engine_base.h"
#include "window_func.h"
#include "vehicle_type.h"
#include "airport.h"
#include "newgrf_airport.h"
#include "rail.h"
#include "road_func.h"
#include "settings_type.h"
#include "town.h"
#include "tile_map.h"
#include "clear_map.h"
#include "station_base.h"
#include "map_func.h"
#include "tilearea_type.h"
#include "viewport_func.h"
#include "disaster_vehicle.h"

#include "safeguards.h"

/**
 * Per-company cargo unlock state, synchronized across all machines via DoCommand.
 */
static std::array<std::array<bool, NUM_CARGO>, MAX_COMPANIES> _ap_company_cargo_unlocked{};

/**
 * Per-company AP-active flag. When true, this company has AP restrictions
 * and the GUI should filter engines/airports/colours accordingly.
 */
static std::array<bool, MAX_COMPANIES> _ap_company_ap_active{};

/**
 * Per-company engine unlock sets, synchronized across all machines via DoCommand.
 */
static std::array<std::set<uint16_t>, MAX_COMPANIES> _ap_company_engine_unlocked{};

/**
 * Per-company airport tier (Progressive Aircrafts tier count).
 */
static std::array<uint8_t, MAX_COMPANIES> _ap_company_airport_tier{};

/**
 * Per-company utility unlock bitmask (bit 0=Bridges, 1=Tunnels, 2=Canals, 3=Terraforming).
 * Synchronized across all machines via CmdAPSetUtilityUnlock.
 */
static std::array<uint8_t, MAX_COMPANIES> _ap_company_utility_unlocked{};

struct APAirportTierEntry {
	uint8_t airport_type;
};

static const std::vector<std::vector<APAirportTierEntry>> _ap_cmd_airport_tiers = {
	{{AT_HELIPORT}, {AT_HELISTATION}, {AT_HELIDEPOT}, {AT_SMALL}},
	{{AT_LARGE}, {AT_COMMUTER}},
	{{AT_METROPOLITAN}, {AT_INTERNATIONAL}},
	{{AT_INTERCON}},
};

/**
 * Query whether a cargo type is unlocked for a specific company.
 * Called from CanMoveGoodsToStation in station_cmd.cpp.
 */
bool AP_IsCompanyCargoUnlocked(CompanyID company, uint8_t cargo_type)
{
	if (company >= MAX_COMPANIES) return true;
	if (!_ap_company_ap_active[company.base()]) return true;  /* AP not active for this company — no restriction */
	if (cargo_type >= NUM_CARGO) return true;
	return _ap_company_cargo_unlocked[company.base()][cargo_type];
}

/**
 * Reset all per-company cargo unlocks (e.g. at AP session start).
 */
void AP_ResetCompanyCargoUnlocks(CompanyID company)
{
	if (company >= MAX_COMPANIES) return;
	_ap_company_cargo_unlocked[company.base()].fill(false);
}

/**
 * Command handler: set a cargo type as unlocked/locked for a company.
 * Executed identically on all machines (host + all clients) via the
 * command system, so no desync.
 */
CommandCost CmdAPSetCargoUnlock(DoCommandFlags flags, CompanyID company, uint8_t cargo_type, bool unlocked)
{
	if (company >= MAX_COMPANIES) return CommandCost();
	if (cargo_type >= NUM_CARGO) return CommandCost();
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		_ap_company_cargo_unlocked[company.base()][cargo_type] = unlocked;
	}

	return CommandCost();
}

/**
 * Command handler: add or subtract money for the current company.
 * Positive amount = add money, negative amount = subtract money.
 * Works like CmdMoneyCheat but without CommandFlag::Offline,
 * so it functions in multiplayer.
 */
CommandCost CmdAPMoney(DoCommandFlags, Money amount)
{
	return CommandCost(EXPENSES_OTHER, -amount);
}

/* ---- Per-company AP-active flag ---- */

bool AP_IsCompanyAPActive(CompanyID company)
{
	if (company >= MAX_COMPANIES) return false;
	return _ap_company_ap_active[company.base()];
}

void AP_ResetCompanyAPState(CompanyID company)
{
	if (company >= MAX_COMPANIES) return;
	_ap_company_ap_active[company.base()] = false;
	_ap_company_engine_unlocked[company.base()].clear();
	_ap_company_airport_tier[company.base()] = 0;
	_ap_company_utility_unlocked[company.base()] = 0;
}

CommandCost CmdAPSetCompanyAPActive(DoCommandFlags flags, CompanyID company, bool active)
{
	if (company >= MAX_COMPANIES) return CommandCost();
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		_ap_company_ap_active[company.base()] = active;

		if (active) {
			/* Disable vehicle/airport expiry so all engines stay available. */
			_settings_game.vehicle.never_expire_vehicles = true;
			_settings_game.station.never_expire_airports = true;

			/* Make ALL engines available to ALL companies in the simulation.
			 * CmdBuildVehicle (IsEngineBuildable) always succeeds; the GUI
			 * filter is the sole AP restriction.  Runs inside the DoCommand
			 * handler so all machines apply it identically. */
			for (Engine *e : Engine::Iterate()) {
				if (!e->IsEnabled()) continue;
				e->company_avail.Set();
				e->flags.Set(EngineFlag::Available);
			}

			/* Update railtypes/roadtypes so monorail/maglev depots can be
			 * built once AP unlocks those engines. */
			for (Company *co : Company::Iterate()) {
				co->avail_railtypes = GetCompanyRailTypes(co->index);
				co->avail_roadtypes = GetCompanyRoadTypes(co->index);
			}

			/* Make ALL airports available by clearing the min_year gate.
			 * The GUI filter (AP_IsAirportTypeUnlocked) is the sole restriction. */
			for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
				AirportSpec *as = AirportSpec::GetWithoutOverride(i);
				if (as == nullptr || !as->enabled) continue;
				as->min_year = TimerGameCalendar::Year{0};
			}
		}

		InvalidateWindowClassesData(WC_BUILD_VEHICLE);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
	}

	return CommandCost();
}

/* ---- Per-company engine unlock ---- */

bool AP_IsCompanyEngineUnlocked(CompanyID company, EngineID eid)
{
	if (company >= MAX_COMPANIES) return true;
	if (!_ap_company_ap_active[company.base()]) return true;

	/* Wagons are unlocked by cargo type, not by engine item */
	const Engine *e = Engine::GetIfValid(eid);
	if (e != nullptr && e->type == VEH_TRAIN &&
	    e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) {
		CargoType ct = e->GetDefaultCargoType();
		if (!IsValidCargoType(ct) || ct >= NUM_CARGO) return false;
		return _ap_company_cargo_unlocked[company.base()][ct];
	}

	return _ap_company_engine_unlocked[company.base()].count(eid.base()) > 0;
}

CommandCost CmdAPSetEngineUnlock(DoCommandFlags flags, CompanyID company, uint16_t engine_id, bool unlocked)
{
	if (company >= MAX_COMPANIES) return CommandCost();
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		if (unlocked) {
			_ap_company_engine_unlocked[company.base()].insert(engine_id);
		} else {
			_ap_company_engine_unlocked[company.base()].erase(engine_id);
		}
		InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
	}

	return CommandCost();
}

/* ---- Per-company airport tier ---- */

uint8_t AP_GetCompanyAirportTier(CompanyID company)
{
	if (company >= MAX_COMPANIES) return 0;
	return _ap_company_airport_tier[company.base()];
}

CommandCost CmdAPSetAirportTier(DoCommandFlags flags, CompanyID company, uint8_t tier)
{
	if (company >= MAX_COMPANIES) return CommandCost();
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		_ap_company_airport_tier[company.base()] = tier;
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
	}

	return CommandCost();
}

bool AP_IsCompanyAirportTypeUnlocked(CompanyID company, uint8_t airport_type)
{
	if (company >= MAX_COMPANIES) return true;
	if (!_ap_company_ap_active[company.base()]) return true;
	if (airport_type == AT_OILRIG) return true;

	int tier = _ap_company_airport_tier[company.base()];
	bool known_airport_type = false;
	for (const auto &tier_entries : _ap_cmd_airport_tiers) {
		for (const auto &entry : tier_entries) {
			if (entry.airport_type == airport_type) {
				known_airport_type = true;
				break;
			}
		}
		if (known_airport_type) break;
	}
	if (!known_airport_type) {
		/* NewGRF airport types not listed in curated tiers: allow once
		 * at least one Progressive Aircrafts tier is unlocked. */
		return tier > 0;
	}

	for (int t = 0; t < tier && t < (int)_ap_cmd_airport_tiers.size(); t++) {
		for (const auto &entry : _ap_cmd_airport_tiers[t]) {
			if (entry.airport_type == airport_type) return true;
		}
	}
	return false;
}

/* ---- Per-company utility (infrastructure lock) unlock ---- */

bool AP_IsCompanyUtilityUnlocked(CompanyID company, uint8_t utility_index)
{
	if (company >= MAX_COMPANIES) return true;
	if (!_ap_company_ap_active[company.base()]) return true;
	return (_ap_company_utility_unlocked[company.base()] & (1u << utility_index)) != 0;
}

void AP_ResetCompanyUtilityUnlocks(CompanyID company)
{
	if (company >= MAX_COMPANIES) return;
	_ap_company_utility_unlocked[company.base()] = 0;
}

/* ---- Savegame-restore helpers (raw index, no CompanyID) ---- */

bool AP_GetCompanyAPActiveIdx(uint8_t idx)
{
	if (idx >= MAX_COMPANIES) return false;
	return _ap_company_ap_active[idx];
}

uint64_t AP_GetCompanyCargoMaskIdx(uint8_t idx)
{
	if (idx >= MAX_COMPANIES) return 0;
	uint64_t mask = 0;
	for (int i = 0; i < (int)NUM_CARGO && i < 64; i++) {
		if (_ap_company_cargo_unlocked[idx][i]) mask |= (1ULL << i);
	}
	return mask;
}

void AP_SetCompanyAPActiveIdx(uint8_t idx, bool active)
{
	if (idx >= MAX_COMPANIES) return;
	_ap_company_ap_active[idx] = active;
}

void AP_SetCompanyCargoMaskIdx(uint8_t idx, uint64_t mask)
{
	if (idx >= MAX_COMPANIES) return;
	for (int i = 0; i < (int)NUM_CARGO && i < 64; i++) {
		_ap_company_cargo_unlocked[idx][i] = (mask & (1ULL << i)) != 0;
	}
}

CommandCost CmdAPSetUtilityUnlock(DoCommandFlags flags, CompanyID company, uint8_t utility_index, bool unlocked)
{
	if (company >= MAX_COMPANIES) return CommandCost();
	if (utility_index >= 4) return CommandCost();
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		if (unlocked) {
			_ap_company_utility_unlocked[company.base()] |= (1u << utility_index);
		} else {
			_ap_company_utility_unlocked[company.base()] &= ~(1u << utility_index);
		}
		/* Refresh build toolbars so greyed-out buttons update. */
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
		InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
		InvalidateWindowData(WC_SCEN_LAND_GEN, 0);
	}

	return CommandCost();
}

/* ---- Engine unlock serialization helpers (used by archipelago_sl.cpp) ---- */

std::string AP_GetCompanyEngineUnlockStr(uint8_t idx)
{
	if (idx >= MAX_COMPANIES) return {};
	std::string result;
	result.reserve(_ap_company_engine_unlocked[idx].size() * 5);
	for (uint16_t id : _ap_company_engine_unlocked[idx]) {
		if (!result.empty()) result += ',';
		char buf[8];
		auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), id);
		result.append(buf, ptr);
	}
	return result;
}

void AP_SetCompanyEngineUnlockStr(uint8_t idx, const std::string &s)
{
	if (idx >= MAX_COMPANIES || s.empty()) return;
	const char *p = s.data();
	const char *end = p + s.size();
	while (p < end) {
		uint16_t id = 0;
		auto [next, ec] = std::from_chars(p, end, id);
		if (ec == std::errc{}) _ap_company_engine_unlocked[idx].insert(id);
		p = next;
		if (p < end && *p == ',') ++p;
	}
}

/* ---- Trap / filler effect commands --------------------------------------- */

/** extern for the global payment multiplier defined in economy.cpp */
extern int _ap_payment_multiplier_pct;

/**
 * AP trap: set all town authority ratings for a company to the minimum.
 * @param flags  Command execution flags.
 * @param company  Company whose ratings are trashed.
 */
CommandCost CmdAPCompanyScandal(DoCommandFlags flags, CompanyID company)
{
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		for (Town *t : Town::Iterate()) {
			/* Affect every town in the game — not just ones the company already serves */
			if (!t->have_ratings.Test(company)) {
				t->have_ratings.Set(company);
				t->ratings[company] = RATING_INITIAL;
			}
			t->ratings[company] = std::max((int)RATING_APPALLING, t->ratings[company] - 500);
		}
	}
	return CommandCost();
}

/**
 * AP trap: clear trees within ~20 tiles of one specific station owned by a company.
 * @param flags       Command execution flags.
 * @param company     The company that owns the station.
 * @param station_id  The station to scorch.
 */
CommandCost CmdAPWildfire(DoCommandFlags flags, CompanyID company, StationID station_id)
{
	if (!Company::IsValidID(company)) return CommandCost();

	if (flags.Test(DoCommandFlag::Execute)) {
		const Station *st = Station::GetIfValid(station_id);
		if (st != nullptr && st->owner == company) {
			static constexpr int WILDFIRE_RADIUS = 20;
			TileIndex centre = st->xy;
			int cx = (int)TileX(centre);
			int cy = (int)TileY(centre);
			int x0 = std::max(0, cx - WILDFIRE_RADIUS);
			int y0 = std::max(0, cy - WILDFIRE_RADIUS);
			int x1 = std::min((int)Map::MaxX(), cx + WILDFIRE_RADIUS);
			int y1 = std::min((int)Map::MaxY(), cy + WILDFIRE_RADIUS);
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) {
					TileIndex tile = TileXY(x, y);
					if (IsTileType(tile, MP_TREES)) {
						MakeClear(tile, CLEAR_GRASS, 0);
						MarkTileDirtyByTile(tile);
					}
				}
			}
		}
	}
	return CommandCost();
}

/**
 * AP effect: set the global cargo payment multiplier (High Demand Period / Recession).
 * @param flags           Command execution flags.
 * @param multiplier_pct  New multiplier in percent (100 = normal, 150 = +50%, 70 = recession).
 */
CommandCost CmdAPSetPaymentMult(DoCommandFlags flags, int32_t multiplier_pct)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		_ap_payment_multiplier_pct = multiplier_pct;
	}
	return CommandCost();
}

/**
 * AP trap: trigger a specific disaster by index, synchronized across all MP machines.
 * @param flags           Command execution flags.
 * @param disaster_index  Index into _disasters[] (0=Zeppelin, 1=Small UFO, 2=Airplane,
 *                        3=Helicopter, 4=Big UFO, 5=Small Sub, 6=Big Sub, 7=Coal Mine).
 */
CommandCost CmdAPTriggerDisaster(DoCommandFlags flags, uint8_t disaster_index)
{
	if (flags.Test(DoCommandFlag::Execute)) {
		AP_TriggerDisaster(disaster_index);
	}
	return CommandCost();
}
