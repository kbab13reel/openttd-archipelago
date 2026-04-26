/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file archipelago_cmd.h Command definitions for Archipelago synchronized state. */

#ifndef ARCHIPELAGO_CMD_H
#define ARCHIPELAGO_CMD_H

#include "command_type.h"
#include "company_type.h"
#include "cargotype.h"
#include "economy_type.h"
#include "engine_type.h"

CommandCost CmdAPSetCargoUnlock(DoCommandFlags flags, CompanyID company, uint8_t cargo_type, bool unlocked);
CommandCost CmdAPMoney(DoCommandFlags flags, Money amount);
CommandCost CmdAPSetCompanyAPActive(DoCommandFlags flags, CompanyID company, bool active);
CommandCost CmdAPSetEngineUnlock(DoCommandFlags flags, CompanyID company, uint16_t engine_id, bool unlocked);
CommandCost CmdAPSetAirportTier(DoCommandFlags flags, CompanyID company, uint8_t tier);
CommandCost CmdAPSetUtilityUnlock(DoCommandFlags flags, CompanyID company, uint8_t utility_index, bool unlocked);

bool AP_IsCompanyUtilityUnlocked(CompanyID company, uint8_t utility_index);
void AP_ResetCompanyUtilityUnlocks(CompanyID company);

/**
 * Savegame-restore helpers — raw index accessors used by archipelago_sl.cpp.
 * These write directly to the per-company arrays WITHOUT going through a
 * DoCommand, so they are only safe to call from savegame Load().
 */
bool     AP_GetCompanyAPActiveIdx(uint8_t company_index);
uint64_t AP_GetCompanyCargoMaskIdx(uint8_t company_index);
void     AP_SetCompanyAPActiveIdx(uint8_t company_index, bool active);
void     AP_SetCompanyCargoMaskIdx(uint8_t company_index, uint64_t mask);

DEF_CMD_TRAIT(CMD_AP_SET_CARGO_UNLOCK, CmdAPSetCargoUnlock, {}, CommandType::ServerSetting)
DEF_CMD_TRAIT(CMD_AP_MONEY, CmdAPMoney, CommandFlag::NoEst, CommandType::ServerSetting)
DEF_CMD_TRAIT(CMD_AP_SET_COMPANY_AP_ACTIVE, CmdAPSetCompanyAPActive, {}, CommandType::ServerSetting)
DEF_CMD_TRAIT(CMD_AP_SET_ENGINE_UNLOCK, CmdAPSetEngineUnlock, {}, CommandType::ServerSetting)
DEF_CMD_TRAIT(CMD_AP_SET_AIRPORT_TIER, CmdAPSetAirportTier, {}, CommandType::OtherManagement)
DEF_CMD_TRAIT(CMD_AP_SET_UTILITY_UNLOCK, CmdAPSetUtilityUnlock, {}, CommandType::ServerSetting)

#endif /* ARCHIPELAGO_CMD_H */
