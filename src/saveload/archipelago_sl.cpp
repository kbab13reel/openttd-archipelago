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
void         AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out);
void         AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in);
void         AP_GetCumulStatsByVtype(uint64_t *flat_out, int vtype_count, int num_cargo);
void         AP_SetCumulStatsByVtype(const uint64_t *flat_in, int vtype_count, int num_cargo);
std::string  AP_GetMaintainCountersStr();
void         AP_SetMaintainCountersStr(const std::string &s);
std::string  AP_GetNamedEntityStr();
void         AP_SetNamedEntityStr(const std::string &s);

/* Per-company AP lock state (simulation-critical — must be in savegame). */
bool     AP_GetCompanyAPActiveIdx(uint8_t company_index);
uint64_t AP_GetCompanyCargoMaskIdx(uint8_t company_index);
void     AP_SetCompanyAPActiveIdx(uint8_t company_index, bool active);
void     AP_SetCompanyCargoMaskIdx(uint8_t company_index, uint64_t mask);

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
        KVSet(_ap_sl_blob, "progress_slot", AP_GetProgressSlotIdentity());

        constexpr int NC = 64;
        uint64_t cargo[NC] = {};
        int64_t  profit    = 0;
        AP_GetCumulStats(cargo, NC, &profit);
        KVSet(_ap_sl_blob, "profit",   IStr(profit));
        KVSet(_ap_sl_blob, "cargo",    PackCargo_AP(cargo, NC));

        constexpr int NV = 4;
        uint64_t cargov[NV * NC] = {};
        AP_GetCumulStatsByVtype(cargov, NV, NC);
        KVSet(_ap_sl_blob, "cargov",   PackCargo_AP(cargov, NV * NC));

        KVSet(_ap_sl_blob, "maintain", AP_GetMaintainCountersStr());
        KVSet(_ap_sl_blob, "named",    AP_GetNamedEntityStr());

        /* Save per-company AP lock state so any machine that loads this
         * savegame (host restart, client reconnect, etc.) immediately has
         * the correct cargo-restriction state — preventing the first-tick
         * simulation desync where ap_active=false lets locked cargo load. */
        for (uint8_t c = 0; c < AP_SL_MAX_COMPANIES; c++) {
            if (!AP_GetCompanyAPActiveIdx(c)) continue;
            KVSet(_ap_sl_blob, fmt::format("apf_{}", c), "1");
            uint64_t cmask = AP_GetCompanyCargoMaskIdx(c);
            if (cmask != 0) KVSet(_ap_sl_blob, fmt::format("apc_{}", c), fmt::format("{}", cmask));
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

        try {
            _ap_last_host = KVGet(_ap_sl_blob, "host");
            _ap_last_port = ParseU16(KVGet(_ap_sl_blob, "port", "38281"), 38281);
            _ap_last_slot = KVGet(_ap_sl_blob, "slot");
            _ap_last_pass = KVGet(_ap_sl_blob, "pass");

            const std::string expected_progress_slot = _ap_last_host.empty() || _ap_last_slot.empty()
                ? std::string()
                : fmt::format("{}:{}|{}", _ap_last_host, _ap_last_port, _ap_last_slot);
            const std::string loaded_progress_slot = KVGet(_ap_sl_blob, "progress_slot");
            const bool progress_slot_matches = loaded_progress_slot.empty() || loaded_progress_slot == expected_progress_slot;

            AP_SetProgressSlotIdentity(expected_progress_slot);

            if (!progress_slot_matches) {
                AP_SetCompletedMissionsStr("");
                AP_SetPurchasedShopLocationsStr("");
                AP_SetGoalSent(false);
                AP_SetStartingGrantsAppliedSlot("");

                constexpr int NC = 64;
                uint64_t cargo[NC] = {};
                AP_SetCumulStats(cargo, NC, 0);
                constexpr int NV = 4;
                uint64_t cargov[NV * NC] = {};
                AP_SetCumulStatsByVtype(cargov, NV, NC);
                AP_SetMaintainCountersStr("");
                AP_SetNamedEntityStr("");
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

            constexpr int NC = 64;
            uint64_t cargo[NC] = {};
            UnpackCargo_AP(KVGet(_ap_sl_blob, "cargo"), cargo, NC);
            AP_SetCumulStats(cargo, NC, getint64("profit"));

            constexpr int NV = 4;
            uint64_t cargov[NV * NC] = {};
            UnpackCargo_AP(KVGet(_ap_sl_blob, "cargov"), cargov, NV * NC);
            AP_SetCumulStatsByVtype(cargov, NV, NC);

            AP_SetMaintainCountersStr(KVGet(_ap_sl_blob, "maintain"));
            AP_SetNamedEntityStr(KVGet(_ap_sl_blob, "named"));

            /* Restore per-company AP lock state — must happen during Load()
             * so the simulation starts with correct cargo restrictions before
             * the AP timer fires and DoCommands are re-applied. */
            for (uint8_t c = 0; c < AP_SL_MAX_COMPANIES; c++) {
                if (KVGet(_ap_sl_blob, fmt::format("apf_{}", c), "0") != "1") continue;
                AP_SetCompanyAPActiveIdx(c, true);
                uint64_t cmask = ParseU64(KVGet(_ap_sl_blob, fmt::format("apc_{}", c), "0"), 0);
                if (cmask != 0) AP_SetCompanyCargoMaskIdx(c, cmask);
            }
        } catch (...) {
            /* Parsing failed — AP progress lost but game loads. */
        }
    }
};

static const APSTChunkHandler APST;
static const ChunkHandlerRef ap_chunk_handlers[] = { APST };
extern const ChunkHandlerTable _ap_chunk_handlers(ap_chunk_handlers);
