/*
 * OpenTTD Archipelago — Savegame chunk (APST)
 *
 * Serialisation strategy: ALL state is packed into a single std::string
 * field ("apstate") as a key=value plain-text format.  This makes the
 * chunk immune to type-mismatch errors across beta versions.
 *
 * KVGet/KVSet/Parse* use only std::string and std::from_chars — no banned
 * functions.  IStr/PackCargo/UnpackCargo use fmt::format and from_chars,
 * placed AFTER safeguards.h where fmt is available.
 */

/* ── Standard headers only — no banned functions here ──────────────── */
#include <string>
#include <vector>
#include <charconv>
#include <algorithm>

/* Minimal key=value serialiser — values may not contain '|' or '=' */
static std::string KVGet(const std::string &blob, const std::string &key, const std::string &def = "")
{
    std::string search = key + "=";
    auto pos = blob.find(search);
    if (pos == std::string::npos) return def;
    pos += search.size();
    auto end = blob.find('|', pos);
    return (end == std::string::npos) ? blob.substr(pos) : blob.substr(pos, end - pos);
}

static void KVSet(std::string &blob, const std::string &key, const std::string &val)
{
    if (!blob.empty()) blob += '|';
    blob += key + '=' + val;
}

/* string -> int/int64/uint16 helpers using std::from_chars (not banned) */
static int ParseInt(const std::string &s, int def = 0)
{
    int r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}
static uint16_t ParseU16(const std::string &s, uint16_t def = 0)
{
    uint16_t r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}
static int64_t ParseI64(const std::string &s, int64_t def = 0)
{
    int64_t r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}
static uint64_t ParseU64(const std::string &s, uint64_t def = 0)
{
    uint64_t r = def;
    std::from_chars(s.data(), s.data() + s.size(), r);
    return r;
}

/* ==================================================================== */
/* OpenTTD headers — safeguards.h bans snprintf/sscanf/to_string etc.  */
/* ALL remaining helpers use fmt::format (allowed) and from_chars only. */
/* ==================================================================== */
#include "../stdafx.h"
#include "saveload.h"
#include "../safeguards.h"
#include "../core/format.hpp"

/* int -> string helpers using fmt (no snprintf allowed past safeguards) */
static std::string IStr(int v)      { return fmt::format("{}", v); }
static std::string IStr(uint16_t v) { return fmt::format("{}", v); }
static std::string IStr(int64_t v)  { return fmt::format("{}", v); }
static std::string IStr(bool v)     { return v ? "1" : "0"; }

/* Pack uint64 cargo array as "idx:val,..." (sparse, skip zeroes) */
static std::string PackCargo_AP(const uint64_t *arr, int n)
{
    std::string out;
    for (int i = 0; i < n; i++) {
        if (arr[i] == 0) continue;
        if (!out.empty()) out += ',';
        out += fmt::format("{}:{}", i, arr[i]);
    }
    return out;
}

/* Unpack "idx:val,..." back into cargo array */
static void UnpackCargo_AP(const std::string &s, uint64_t *arr, int n)
{
    std::fill(arr, arr + n, static_cast<uint64_t>(0));
    if (s.empty()) return;
    const char *p   = s.data();
    const char *end = p + s.size();
    while (p < end) {
        int idx = 0;
        auto [p2, ec1] = std::from_chars(p, end, idx);
        if (ec1 != std::errc() || p2 >= end || *p2 != ':') break;
        p = p2 + 1;
        uint64_t val = 0;
        auto [p3, ec2] = std::from_chars(p, end, val);
        if (ec2 == std::errc() && idx >= 0 && idx < n) arr[idx] = val;
        p = p3;
        if (p < end && *p == ',') p++;
    }
}

/* ── External state from archipelago_manager.cpp ───────────────────── */
extern std::string  _ap_last_host;
extern uint16_t     _ap_last_port;
extern std::string  _ap_last_slot;
extern std::string  _ap_last_pass;

std::string  AP_GetCompletedMissionsStr();
void         AP_SetCompletedMissionsStr(const std::string &s);
std::string  AP_GetPurchasedShopLocationsStr();
void         AP_SetPurchasedShopLocationsStr(const std::string &s);
bool         AP_GetGoalSent();
void         AP_SetGoalSent(bool v);
std::string  AP_GetStartingGrantsAppliedSlot();
void         AP_SetStartingGrantsAppliedSlot(const std::string &s);
std::string  AP_GetProgressSlotIdentity();
void         AP_SetProgressSlotIdentity(const std::string &s);
void         AP_GetCumulStatsByVtype(uint8_t cid, uint64_t *flat_out, int vtype_count, int num_cargo);
void         AP_SetCumulStatsByVtype(uint8_t cid, const uint64_t *flat_in, int vtype_count, int num_cargo);

int64_t      AP_GetSavedItemsIndex();
void         AP_SetSavedItemsIndex(int64_t v);

/* Per-company AP lock state (simulation-critical — must be in savegame). */
bool        AP_GetCompanyAPActiveIdx(uint8_t company_index);
uint64_t    AP_GetCompanyCargoMaskIdx(uint8_t company_index);
void        AP_SetCompanyAPActiveIdx(uint8_t company_index, bool active);
void        AP_SetCompanyCargoMaskIdx(uint8_t company_index, uint64_t mask);
std::string AP_GetCompanyEngineUnlockStr(uint8_t company_index);
void        AP_SetCompanyEngineUnlockStr(uint8_t company_index, const std::string &s);

static constexpr uint8_t AP_SL_MAX_COMPANIES = 15; /* matches MAX_COMPANIES */

/* ── Scratch variable — single string holds all AP state ────────────── */
static std::string _ap_sl_blob;

/* ── SaveLoad table: one field only ─────────────────────────────────── */
static const SaveLoad _ap_desc[] = {
    SLEG_SSTR("apstate", _ap_sl_blob, SLE_STR),
};

struct APSTChunkHandler : ChunkHandler {
    APSTChunkHandler() : ChunkHandler('APST', CH_TABLE) {}

    void Save() const override
    {
        _ap_sl_blob.clear();

        KVSet(_ap_sl_blob, "host",  _ap_last_host);
        KVSet(_ap_sl_blob, "port",  IStr(_ap_last_port));
        KVSet(_ap_sl_blob, "slot",  _ap_last_slot);
        KVSet(_ap_sl_blob, "pass",  _ap_last_pass);

        KVSet(_ap_sl_blob, "completed",   AP_GetCompletedMissionsStr());
        KVSet(_ap_sl_blob, "shop",        AP_GetPurchasedShopLocationsStr());
        KVSet(_ap_sl_blob, "goal_sent",   IStr(AP_GetGoalSent()));
        KVSet(_ap_sl_blob, "starting_grants_slot", AP_GetStartingGrantsAppliedSlot());
        /* Store host and slot as separate keys — the KV separator is '|' so a
         * combined "host|slot" value would be silently truncated on read. */
        KVSet(_ap_sl_blob, "progress_host",      _ap_last_host);
        KVSet(_ap_sl_blob, "progress_slot_name", _ap_last_slot);

        KVSet(_ap_sl_blob, "items_index", fmt::format("{}", AP_GetSavedItemsIndex()));

        /* Save per-company AP state (cargo lock + per-vtype delivery counters).
         * cargov_N = flat [vtype][cargo] array; contains the ground truth totals.
         * ape_N   = comma-separated engine IDs that are individually unlocked. */
        constexpr int NC = 64;
        constexpr int NV = 5;
        for (uint8_t c = 0; c < AP_SL_MAX_COMPANIES; c++) {
            if (!AP_GetCompanyAPActiveIdx(c)) continue;
            KVSet(_ap_sl_blob, fmt::format("apf_{}", c), "1");
            uint64_t cmask = AP_GetCompanyCargoMaskIdx(c);
            if (cmask != 0) KVSet(_ap_sl_blob, fmt::format("apc_{}", c), fmt::format("{}", cmask));

            std::string estr = AP_GetCompanyEngineUnlockStr(c);
            if (!estr.empty()) KVSet(_ap_sl_blob, fmt::format("ape_{}", c), estr);

            uint64_t cargov[NV * NC] = {};
            AP_GetCumulStatsByVtype(c, cargov, NV, NC);
            KVSet(_ap_sl_blob, fmt::format("cargov_{}", c), PackCargo_AP(cargov, NV * NC));
        }

        SlTableHeader(_ap_desc);
        SlSetArrayIndex(0);
        SlGlobList(_ap_desc);
    }

    void Load() const override
    {
        _ap_sl_blob.clear();

        const std::vector<SaveLoad> slt = SlCompatTableHeader(_ap_desc, {});
        if (SlIterateArray() == -1) return;
        SlGlobList(slt);
        /* Consume end-of-array marker to reset _next_offs to 0.
         * Without this, SlLoadChunk() sees _next_offs != 0 and throws
         * "Invalid array length". See animated_tile_sl.cpp for reference. */
        if (SlIterateArray() != -1) SlErrorCorrupt("Too many APST entries");

        if (_ap_sl_blob.empty()) return;

        /* When loading the main menu background world, only restore the connection
         * credentials (host/port/slot/pass).  Restoring progress state (completed
         * missions, cargo counters, etc.) from an arbitrary background save would
         * pollute _ap_completed_mission_locations and _ap_pending_sd.missions with
         * stale data that then bleeds into the next real game session. */
        const bool is_menu_load = (_game_mode == GM_MENU);

        try {
            _ap_last_host = KVGet(_ap_sl_blob, "host");
            _ap_last_port = ParseU16(KVGet(_ap_sl_blob, "port", "38281"), 38281);
            _ap_last_slot = KVGet(_ap_sl_blob, "slot");
            _ap_last_pass = KVGet(_ap_sl_blob, "pass");

            /* Progress slot matching: host and slot are stored as separate keys
             * to avoid collision with the KV separator '|'.
             * Legacy saves used a combined "progress_slot" key whose value was
             * silently truncated at the '|' — treat missing/unreadable as match. */
            const std::string loaded_ph = KVGet(_ap_sl_blob, "progress_host");
            const std::string loaded_ps = KVGet(_ap_sl_blob, "progress_slot_name");
            const bool progress_slot_matches = (loaded_ph.empty() && loaded_ps.empty())
                || (loaded_ph == _ap_last_host && loaded_ps == _ap_last_slot);

            /* Skip progress restoration for main menu background loads. */
            if (is_menu_load) return;

            /* Do NOT call AP_SetProgressSlotIdentity here — that would poison the
             * mismatch check in AP_OnSlotData, which fires after the load completes
             * and compares the live connection against what we just set from the save,
             * causing a false "slot changed" warning that wipes the loaded stats. */

            if (!progress_slot_matches) {
                AP_SetCompletedMissionsStr("");
                AP_SetPurchasedShopLocationsStr("");
                AP_SetGoalSent(false);
                AP_SetStartingGrantsAppliedSlot("");
                AP_SetSavedItemsIndex(0);
                return;
            }

            AP_SetCompletedMissionsStr(KVGet(_ap_sl_blob, "completed"));
            AP_SetPurchasedShopLocationsStr(KVGet(_ap_sl_blob, "shop"));
            AP_SetStartingGrantsAppliedSlot(KVGet(_ap_sl_blob, "starting_grants_slot"));

            auto getint = [&](const std::string &key) -> int {
                return ParseInt(KVGet(_ap_sl_blob, key, "0"));
            };
            auto getint64 = [&](const std::string &key) -> int64_t {
                return ParseI64(KVGet(_ap_sl_blob, key, "0"));
            };

            AP_SetGoalSent(KVGet(_ap_sl_blob, "goal_sent", "0") == "1");

            AP_SetSavedItemsIndex(ParseI64(KVGet(_ap_sl_blob, "items_index", "0")));

            /* Restore per-company AP state (cargo lock + delivery counters).
             * Current format: cargov_N per AP-active company; ape_N for engine unlocks.
             * Legacy fallback A: cargo_N + cargov_N keys (pre-direct-counter saves) —
             *   we load cargov_N if present, ignoring cargo_N (it was timer-based).
             * Legacy fallback B: single "cargov" key (single-company pre-multiAP) —
             *   loaded into the sole AP-active company. */
            constexpr int NC = 64;
            constexpr int NV = 5;
            bool any_new_format = false;
            for (uint8_t c = 0; c < AP_SL_MAX_COMPANIES; c++) {
                if (KVGet(_ap_sl_blob, fmt::format("apf_{}", c), "0") != "1") continue;
                AP_SetCompanyAPActiveIdx(c, true);
                uint64_t cmask = ParseU64(KVGet(_ap_sl_blob, fmt::format("apc_{}", c), "0"), 0);
                if (cmask != 0) AP_SetCompanyCargoMaskIdx(c, cmask);

                std::string estr = KVGet(_ap_sl_blob, fmt::format("ape_{}", c));
                if (!estr.empty()) AP_SetCompanyEngineUnlockStr(c, estr);

                std::string cargov_str = KVGet(_ap_sl_blob, fmt::format("cargov_{}", c));
                if (!cargov_str.empty()) {
                    any_new_format = true;
                    uint64_t cargov[NV * NC] = {};
                    UnpackCargo_AP(cargov_str, cargov, NV * NC);
                    AP_SetCumulStatsByVtype(c, cargov, NV, NC);
                }
            }

            /* Legacy B: single-company "cargov" key */
            if (!any_new_format) {
                std::string legacy = KVGet(_ap_sl_blob, "cargov");
                if (!legacy.empty()) {
                    uint8_t target = 0;
                    int active = 0;
                    for (uint8_t c = 0; c < AP_SL_MAX_COMPANIES; c++)
                        if (AP_GetCompanyAPActiveIdx(c)) { target = c; active++; }
                    if (active <= 1) {
                        uint64_t cargov[NV * NC] = {};
                        UnpackCargo_AP(legacy, cargov, NV * NC);
                        AP_SetCumulStatsByVtype(target, cargov, NV, NC);
                    }
                }
            }
        } catch (...) {
            /* Parsing failed — AP progress lost but game loads. */
        }
    }
};

static const APSTChunkHandler APST;
static const ChunkHandlerRef ap_chunk_handlers[] = { APST };
extern const ChunkHandlerTable _ap_chunk_handlers(ap_chunk_handlers);
