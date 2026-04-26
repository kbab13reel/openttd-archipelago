/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#include "stdafx.h"
#include "archipelago.h"
#include "debug.h"

#include "3rdparty/nlohmann/json.hpp"
using json = nlohmann::json;

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   typedef SOCKET sock_t;
#  define SOCK_INVALID INVALID_SOCKET
#  define sock_close(s) closesocket(s)
#  define sock_err()    WSAGetLastError()
#else
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
   typedef int sock_t;
#  define SOCK_INVALID (-1)
#  define sock_close(s) ::close(s)
#  define sock_err()    errno
#endif

#include <sstream>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstring>
#ifdef WITH_ZLIB
#  include <zlib.h>
#endif

/* =========================================================================
 * Windows Schannel TLS wrapper
 * Used when use_ssl=true so the WebSocket layer operates over wss://.
 * Requires Secur32.lib (already linked by MSVC on Windows).
 * ========================================================================= */
#ifdef _WIN32
#include <schannel.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#pragma comment(lib, "Secur32.lib")

struct ApTlsCtx {
	CredHandle hCred{};
	CtxtHandle hCtx{};
	bool       ctx_valid{ false };
	SecPkgContext_StreamSizes sizes{};

	/* Leftover encrypted bytes not yet decrypted (SECBUFFER_EXTRA), and
	   decrypted bytes not yet consumed by the caller. */
	std::vector<char> enc_extra;
	std::vector<char> dec_buf;

	~ApTlsCtx()
	{
		if (ctx_valid) {
			DeleteSecurityContext(&hCtx);
			FreeCredentialsHandle(&hCred);
		}
	}

	/** Perform TLS client handshake over an already-connected socket. */
	bool Handshake(sock_t s, const std::string &hostname)
	{
		/* ── Acquire credentials ── */
		SCHANNEL_CRED cred{};
		cred.dwVersion = SCHANNEL_CRED_VERSION;
		cred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;
		cred.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

		SECURITY_STATUS ss = AcquireCredentialsHandleA(
		    nullptr, const_cast<SEC_CHAR *>(UNISP_NAME_A),
		    SECPKG_CRED_OUTBOUND, nullptr, &cred,
		    nullptr, nullptr, &hCred, nullptr);
		if (ss != SEC_E_OK) return false;

		ULONG req = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
		            ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY |
		            ISC_REQ_STREAM;

		/* ── Handshake loop ── */
		std::vector<char> ibuf; /* accumulates server data */
		bool first = true;
		ULONG attrs = 0;

		while (true) {
			SecBuffer in_bufs[2] = {};
			SecBuffer out_buf    = {};
			SecBufferDesc in_desc  { SECBUFFER_VERSION, 2, in_bufs };
			SecBufferDesc out_desc { SECBUFFER_VERSION, 1, &out_buf };

			if (first) {
				in_bufs[0].BufferType = SECBUFFER_EMPTY;
				in_bufs[1].BufferType = SECBUFFER_EMPTY;
			} else {
				in_bufs[0].BufferType = SECBUFFER_TOKEN;
				in_bufs[0].pvBuffer   = ibuf.data();
				in_bufs[0].cbBuffer   = (ULONG)ibuf.size();
				in_bufs[1].BufferType = SECBUFFER_EMPTY;
			}
			out_buf.BufferType = SECBUFFER_TOKEN;

			ss = InitializeSecurityContextA(
			    &hCred,
			    first ? nullptr : &hCtx,
			    const_cast<SEC_CHAR *>(hostname.c_str()),
			    req, 0, 0,
			    first ? nullptr : &in_desc,
			    0, first ? &hCtx : nullptr,
			    &out_desc, &attrs, nullptr);
			first = false;

			/* Send any output token to the server */
			if (out_buf.pvBuffer && out_buf.cbBuffer > 0) {
				const char *p = (const char *)out_buf.pvBuffer;
				int len = (int)out_buf.cbBuffer;
				while (len > 0) {
					int r = ::send(s, p, len, 0);
					if (r <= 0) { FreeContextBuffer(out_buf.pvBuffer); goto fail; }
					p += r; len -= r;
				}
				FreeContextBuffer(out_buf.pvBuffer);
			}

			if (ss == SEC_E_OK) {
				/* Handshake complete — stash any EXTRA bytes */
				if (in_bufs[1].BufferType == SECBUFFER_EXTRA && in_bufs[1].cbBuffer > 0) {
					size_t off = ibuf.size() - in_bufs[1].cbBuffer;
					enc_extra.assign(ibuf.begin() + off, ibuf.end());
				}
				break;
			}
			if (ss != SEC_I_CONTINUE_NEEDED && ss != SEC_E_INCOMPLETE_MESSAGE) goto fail;

			/* Handle SECBUFFER_EXTRA — server sent more than one record */
			if (in_bufs[1].BufferType == SECBUFFER_EXTRA && in_bufs[1].cbBuffer > 0) {
				size_t off = ibuf.size() - in_bufs[1].cbBuffer;
				ibuf.erase(ibuf.begin(), ibuf.begin() + (ptrdiff_t)off);
			} else if (ss != SEC_E_INCOMPLETE_MESSAGE) {
				ibuf.clear();
			}

			/* Read more server data */
			char tmp[16384];
			int r = ::recv(s, tmp, sizeof(tmp), 0);
			if (r <= 0) goto fail;
			ibuf.insert(ibuf.end(), tmp, tmp + r);
		}

		ctx_valid = true;
		QueryContextAttributesA(&hCtx, SECPKG_ATTR_STREAM_SIZES, &sizes);
		return true;
	fail:
		FreeCredentialsHandle(&hCred);
		return false;
	}

	/** Encrypt and send `len` plain bytes. */
	bool Write(sock_t s, const char *buf, int len)
	{
		while (len > 0) {
			int chunk = std::min(len, (int)sizes.cbMaximumMessage);
			std::vector<char> msg(sizes.cbHeader + chunk + sizes.cbTrailer);
			memcpy(msg.data() + sizes.cbHeader, buf, chunk);

			SecBuffer bufs[4] = {};
			bufs[0].BufferType = SECBUFFER_STREAM_HEADER;
			bufs[0].pvBuffer   = msg.data();
			bufs[0].cbBuffer   = sizes.cbHeader;
			bufs[1].BufferType = SECBUFFER_DATA;
			bufs[1].pvBuffer   = msg.data() + sizes.cbHeader;
			bufs[1].cbBuffer   = chunk;
			bufs[2].BufferType = SECBUFFER_STREAM_TRAILER;
			bufs[2].pvBuffer   = msg.data() + sizes.cbHeader + chunk;
			bufs[2].cbBuffer   = sizes.cbTrailer;
			bufs[3].BufferType = SECBUFFER_EMPTY;

			SecBufferDesc desc { SECBUFFER_VERSION, 4, bufs };
			SECURITY_STATUS ss = EncryptMessage(&hCtx, 0, &desc, 0);
			if (ss != SEC_E_OK) return false;

			int total = (int)(bufs[0].cbBuffer + bufs[1].cbBuffer + bufs[2].cbBuffer);
			const char *p = msg.data();
			while (total > 0) {
				int r = ::send(s, p, total, 0);
				if (r <= 0) return false;
				p += r; total -= r;
			}
			buf += chunk; len -= chunk;
		}
		return true;
	}

	/** Receive exactly `len` decrypted bytes (blocking, with timeout pass-through). */
	bool Read(sock_t s, char *buf, int len)
	{
		while (len > 0) {
			/* Drain already-decrypted buffer first */
			if (!dec_buf.empty()) {
				int take = std::min(len, (int)dec_buf.size());
				memcpy(buf, dec_buf.data(), take);
				dec_buf.erase(dec_buf.begin(), dec_buf.begin() + take);
				buf += take; len -= take;
				continue;
			}

			/* Accumulate encrypted data (seed with enc_extra from handshake) */
			std::vector<char> rbuf = enc_extra;
			enc_extra.clear();

			while (true) {
				if (!rbuf.empty()) {
					SecBuffer bufs[4] = {};
					bufs[0].BufferType = SECBUFFER_DATA;
					bufs[0].pvBuffer   = rbuf.data();
					bufs[0].cbBuffer   = (ULONG)rbuf.size();
					bufs[1].BufferType = SECBUFFER_EMPTY;
					bufs[2].BufferType = SECBUFFER_EMPTY;
					bufs[3].BufferType = SECBUFFER_EMPTY;

					SecBufferDesc desc { SECBUFFER_VERSION, 4, bufs };
					SECURITY_STATUS ss = DecryptMessage(&hCtx, &desc, 0, nullptr);

					if (ss == SEC_E_OK) {
						/* Extract decrypted data */
						for (int i = 0; i < 4; i++) {
							if (bufs[i].BufferType == SECBUFFER_DATA && bufs[i].cbBuffer > 0) {
								const char *d = (const char *)bufs[i].pvBuffer;
								dec_buf.insert(dec_buf.end(), d, d + bufs[i].cbBuffer);
							}
						}
						/* Stash EXTRA for next call */
						for (int i = 0; i < 4; i++) {
							if (bufs[i].BufferType == SECBUFFER_EXTRA && bufs[i].cbBuffer > 0) {
								const char *d = (const char *)bufs[i].pvBuffer;
								enc_extra.assign(d, d + bufs[i].cbBuffer);
							}
						}
						break; /* have decrypted data; back to outer loop */
					}
					if (ss == SEC_I_RENEGOTIATE) break; /* treat as success for now */
					if (ss != SEC_E_INCOMPLETE_MESSAGE) return false;
					/* SEC_E_INCOMPLETE_MESSAGE → fall through to recv more */
				}

				/* Need more encrypted bytes from the wire */
				char tmp[16384];
				int r = ::recv(s, tmp, sizeof(tmp), 0);
				if (r <= 0) return false;
				rbuf.insert(rbuf.end(), tmp, tmp + r);
			}
		}
		return true;
	}
};
#endif /* _WIN32 */

#include "console_func.h"
#include "core/string_consumer.hpp"
#include "safeguards.h"

/* -------------------------------------------------------------------------
 * AP Console logging helper — visible in OpenTTD game console (~)
 * ---------------------------------------------------------------------- */
#define AP_LOG(msg)  IConsolePrint(CC_INFO,    "[AP] " + std::string(msg))
#define AP_OK(msg)   IConsolePrint(CC_WHITE,   "[AP] " + std::string(msg))
#define AP_WARN(msg) IConsolePrint(CC_WARNING, "[AP] WARNING: " + std::string(msg))
#define AP_ERR(msg)  IConsolePrint(CC_ERROR,   "[AP] ERROR: " + std::string(msg))



/* -------------------------------------------------------------------------
 * Global instance
 * ---------------------------------------------------------------------- */

ArchipelagoClient *_ap_client = nullptr;

void InitArchipelago()
{
	if (_ap_client == nullptr) _ap_client = new ArchipelagoClient();
}

void UninitArchipelago()
{
	delete _ap_client;
	_ap_client = nullptr;
}

/* -------------------------------------------------------------------------
 * ArchipelagoClient
 * ---------------------------------------------------------------------- */

ArchipelagoClient::ArchipelagoClient() = default;

ArchipelagoClient::~ArchipelagoClient()
{
	Disconnect();
}

void ArchipelagoClient::Connect(const std::string &h, uint16_t p,
                                const std::string &slot, const std::string &pw,
                                const std::string &game, bool ssl)
{
	Disconnect();

	host      = h;
	port      = p;
	slot_name = slot;
	password  = pw;
	game_name = game;
	use_ssl   = ssl;

	has_slot_data.store(false);

	stop_requested.store(false);
	state.store(APState::CONNECTING);

	worker_thread = std::thread(&ArchipelagoClient::WorkerThread, this);
}

void ArchipelagoClient::Disconnect()
{
	stop_requested.store(true);
	if (worker_thread.joinable()) worker_thread.join();
	has_slot_data.store(false);
	state.store(APState::DISCONNECTED);
}

void ArchipelagoClient::SendCheck(int64_t location_id)
{
	json pkt = json::array();
	pkt.push_back({{"cmd", "LocationChecks"}, {"locations", json::array({location_id})}});
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ pkt.dump() });
}

void ArchipelagoClient::SendCheckByName(const std::string &location_name)
{
	auto it = location_ids.find(location_name);
	if (it != location_ids.end()) {
		SendCheck(it->second);
	}
}

void ArchipelagoClient::SendGoal()
{
	json pkt = json::array();
	pkt.push_back({{"cmd", "StatusUpdate"}, {"status", 30}});
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ pkt.dump() });
}

void ArchipelagoClient::SendSay(const std::string &text)
{
	json pkt = json::array();
	pkt.push_back({{"cmd", "Say"}, {"text", text}});
	std::lock_guard<std::mutex> lg(outbound_mutex);
	outbound_queue.push_back({ pkt.dump() });
}

void ArchipelagoClient::Tick()
{
	std::lock_guard<std::mutex> lg(inbound_mutex);
	if (!inbound_queue.empty()) {
		AP_LOG(fmt::format("[Tick] Processing {} queued events", inbound_queue.size()));
	}
	while (!inbound_queue.empty()) {
		InboundEvent ev = std::move(inbound_queue.front());
		inbound_queue.pop_front();
		switch (ev.type) {
			case InboundEvent::CONNECTED:
				AP_LOG("[Tick] Dispatching CONNECTED event");
				if (callbacks.on_connected) callbacks.on_connected();
				else AP_ERR("[Tick] on_connected callback is NULL!");
				break;
			case InboundEvent::DISCONNECTED:
				AP_LOG(fmt::format("[Tick] Dispatching DISCONNECTED: {}", ev.text));
				if (callbacks.on_disconnected) callbacks.on_disconnected(ev.text);
				else AP_ERR("[Tick] on_disconnected callback is NULL!");
				break;
			case InboundEvent::ITEM:
				if (callbacks.on_item_received) callbacks.on_item_received(ev.item);
				break;
			case InboundEvent::PRINT:
					if (callbacks.on_print) callbacks.on_print(ev.text, ev.colour);
				break;
			case InboundEvent::SLOT_DATA:
				AP_LOG("[Tick] Dispatching SLOT_DATA event");
				if (callbacks.on_slot_data) callbacks.on_slot_data(ev.slot);
				else AP_ERR("[Tick] on_slot_data callback is NULL!");
				break;
			case InboundEvent::LOCATIONS_UPDATED:
				if (callbacks.on_locations_updated) callbacks.on_locations_updated();
				break;
		}
	}
}

void ArchipelagoClient::PushEvent(InboundEvent ev)
{
	std::lock_guard<std::mutex> lg(inbound_mutex);
	inbound_queue.push_back(std::move(ev));
}

std::string ArchipelagoClient::PopOutbound()
{
	std::lock_guard<std::mutex> lg(outbound_mutex);
	if (outbound_queue.empty()) return {};
	std::string s = std::move(outbound_queue.front().json);
	outbound_queue.pop_front();
	return s;
}

/* -------------------------------------------------------------------------
 * Slot data parser
 * ---------------------------------------------------------------------- */

static APSlotData ParseSlotData(const json &msg)
{
	APSlotData sd;

	if (!msg.contains("slot_data") || !msg["slot_data"].is_object()) {
		Debug(misc, 0, "[AP] ParseSlotData: NO slot_data field in Connected message!");
		return sd;
	}
	const json &d = msg["slot_data"];

	auto GetBoolLike = [&](const char *key, bool fallback) -> bool {
		if (!d.contains(key)) return fallback;
		const auto &value = d[key];
		if (value.is_boolean()) return value.get<bool>();
		if (value.is_number_integer()) return value.get<int>() != 0;
		return fallback;
	};

	sd.game_version         = d.value("game_version", "15.2");
	sd.mission_count        = d.value("mission_count", 100);
	sd.starting_vehicle     = d.value("starting_vehicle", "");
	/* starting_vehicles list — present for one_of_each mode.
	 * Falls back to a single-element list from starting_vehicle. */
	if (d.contains("starting_vehicles") && d["starting_vehicles"].is_array()) {
		for (const auto &v : d["starting_vehicles"]) {
			if (v.is_string()) sd.starting_vehicles.push_back(v.get<std::string>());
		}
	}
	if (sd.starting_vehicles.empty() && !sd.starting_vehicle.empty()) {
		sd.starting_vehicles.push_back(sd.starting_vehicle);
	}
	sd.enable_traps         = GetBoolLike("enable_traps", true);
	sd.start_year           = d.value("start_year", 1950);

	/* World generation parameters */
	sd.world_seed           = d.value("world_seed", (uint32_t)0);
	sd.map_x                = (uint8_t)d.value("map_x", 8);
	sd.map_y                = (uint8_t)d.value("map_y", 8);
	sd.landscape            = (uint8_t)d.value("landscape", 0);
	sd.land_generator       = (uint8_t)d.value("land_generator", 1);
	sd.terrain_type         = (uint8_t)d.value("terrain_type", 1);
	sd.quantity_sea_lakes   = (uint8_t)d.value("quantity_sea_lakes", 0);
	sd.variety              = (uint8_t)d.value("variety", 0);
	sd.tgen_smoothness      = (uint8_t)d.value("tgen_smoothness", 1);
	sd.amount_of_rivers     = (uint8_t)d.value("amount_of_rivers", 2);
	sd.water_border_presets = (uint8_t)d.value("water_border_presets", 0);
	sd.town_name            = (uint8_t)d.value("town_name", 0);
	sd.number_towns         = (uint8_t)d.value("number_towns", 2);
	sd.industry_density     = (uint8_t)d.value("industry_density", 4);
	sd.starting_cash_bonus       = GetBoolLike("starting_cash_bonus", false);

	/* NewGRF options */
	sd.enable_iron_horse         = GetBoolLike("enable_iron_horse", false);

	/* Starting cargo type (0=any, 1-9=specific cargo, matching Python StartingCargoType enum) */
	sd.starting_cargo_type       = d.value("starting_cargo_type", 0);

	/* Verbose log — visible in OpenTTD debug console (press ~ in game) */
	Debug(misc, 0, "[AP] SlotData: version={} missions={} start_year={} vehicle='{}'",
	      sd.game_version, sd.mission_count, sd.start_year, sd.starting_vehicle);
	Debug(misc, 0, "[AP] SlotData: map={}x{} landscape={} seed={} traps={}",
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape,
	      sd.world_seed, sd.enable_traps);

	/* item_id_to_name — APWorld sends this so we can resolve item IDs to names */
	if (d.contains("item_id_to_name") && d["item_id_to_name"].is_object()) {
		for (auto &[key, val] : d["item_id_to_name"].items()) {
			/* Manual parse — std::stoll is banned by safeguards.h */
			int64_t id = 0; bool valid = !key.empty();
			for (char c : key) { if (c < '0' || c > '9') { valid = false; break; } id = id * 10 + (int64_t)(c - '0'); }
			if (valid) sd.item_id_to_name[id] = val.get<std::string>();
		}
		Debug(misc, 0, "[AP] SlotData: {} item id->name mappings loaded", sd.item_id_to_name.size());
	} else {
		Debug(misc, 0, "[AP] SlotData: WARNING — no item_id_to_name! Items cannot be unlocked by name.");
	}

	/* location_name_to_id — authoritative mission location mapping from APWorld */
	if (d.contains("location_name_to_id") && d["location_name_to_id"].is_object()) {
		for (auto &[name, idv] : d["location_name_to_id"].items()) {
			if (idv.is_number_integer()) {
				sd.location_name_to_id[name] = idv.get<int64_t>();
			}
		}
		Debug(misc, 0, "[AP] SlotData: {} location name->id mappings loaded", sd.location_name_to_id.size());
	}

	/* locked_vehicles — the exact set of vehicle names to lock at session start.
	 * Only engines whose English name is in this set are locked; others
	 * (e.g. Iron Horse engines when enable_iron_horse=false) stay freely available. */
	if (d.contains("locked_vehicles") && d["locked_vehicles"].is_array()) {
		for (const auto &v : d["locked_vehicles"]) {
			if (v.is_string()) sd.locked_vehicles.insert(v.get<std::string>());
		}
		Debug(misc, 0, "[AP] SlotData: {} locked_vehicles loaded", sd.locked_vehicles.size());
	}

	sd.enable_shop = GetBoolLike("enable_shop", sd.enable_shop);
	if (d.contains("shop_tiers") && d["shop_tiers"].is_number_integer()) {
		sd.shop_tiers = d["shop_tiers"].get<int>();
	}

	/* Infrastructure locks (v0.2.0+) */
	sd.lock_bridges      = GetBoolLike("lock_bridges",      false);
	sd.lock_tunnels      = GetBoolLike("lock_tunnels",       false);
	sd.lock_canals       = GetBoolLike("lock_canals",        false);
	sd.lock_terraforming = GetBoolLike("lock_terraforming",  false);

	/* Parse missions array */
	if (d.contains("missions") && d["missions"].is_array()) {
		for (const auto &m : d["missions"]) {
			APMission mission;
			mission.location    = m.value("location",    "");
			mission.description = m.value("description", "");
			mission.type        = m.value("type",        "");
			mission.difficulty  = m.value("difficulty",  "easy");
			mission.cargo        = m.value("cargo",        "");
			mission.unit         = m.value("unit",         "units");
			mission.vehicle_key  = m.value("vehicle_key",  "");
			mission.amount       = m.value("amount",       (int64_t)0);
			mission.completed   = false;
			mission.current_value = 0;
			if (!mission.location.empty()) {
				sd.missions.push_back(std::move(mission));
			}
		}
	}

	if (d.contains("shop_locations") && d["shop_locations"].is_array()) {
		for (const auto &shop : d["shop_locations"]) {
			APShopLocation entry;
			entry.location = shop.value("location", "");
			entry.name     = shop.value("name", "");
			entry.cost     = shop.value("cost", (int64_t)0);
			if (!entry.location.empty()) sd.shop_locations.push_back(std::move(entry));
		}
	}

	return sd;
}

/* -------------------------------------------------------------------------
 * Base64 encode (for WebSocket handshake key)
 * ---------------------------------------------------------------------- */
static const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const uint8_t *data, size_t len)
{
	std::string out;
	out.reserve(((len + 2) / 3) * 4);
	for (size_t i = 0; i < len; i += 3) {
		uint32_t v = ((uint32_t)data[i] << 16)
		           | (i+1 < len ? (uint32_t)data[i+1] <<  8 : 0)
		           | (i+2 < len ? (uint32_t)data[i+2]       : 0);
		out += b64chars[(v >> 18) & 63];
		out += b64chars[(v >> 12) & 63];
		out += (i+1 < len) ? b64chars[(v >>  6) & 63] : '=';
		out += (i+2 < len) ? b64chars[(v      ) & 63] : '=';
	}
	return out;
}

/* -------------------------------------------------------------------------
 * Socket helpers
 * ---------------------------------------------------------------------- */

/* Forward-declare for use inside ApTlsCtx::Write */
static bool RawSendAll(sock_t s, const char *buf, int len)
{
	while (len > 0) {
		int sent = ::send(s, buf, len, 0);
		if (sent <= 0) return false;
		buf += sent;
		len -= sent;
	}
	return true;
}

#ifdef _WIN32
static thread_local ApTlsCtx *tls_ctx_tl = nullptr; ///< set by WorkerThread
#endif

static bool SendAll(sock_t s, const char *buf, int len)
{
#ifdef _WIN32
	if (tls_ctx_tl) return tls_ctx_tl->Write(s, buf, len);
#endif
	return RawSendAll(s, buf, len);
}

static bool sock_timed_out()
{
#ifdef _WIN32
	int e = WSAGetLastError();
	return e == WSAETIMEDOUT || e == WSAEWOULDBLOCK;
#else
	return errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT;
#endif
}

static bool RecvAll(sock_t s, char *buf, int len)
{
#ifdef _WIN32
	if (tls_ctx_tl) return tls_ctx_tl->Read(s, buf, len);
#endif
	while (len > 0) {
		int r = ::recv(s, buf, len, 0);
		if (r <= 0) return false;
		buf += r;
		len -= r;
	}
	return true;
}

/* -------------------------------------------------------------------------
 * WebSocket handshake
 * Requests permessage-deflate compression. If the server accepts it,
 * ws_deflate is set to true and RecvWsFrame will decompress RSV1 frames.
 * ---------------------------------------------------------------------- */
bool ArchipelagoClient::DoWebSocketHandshake(int sockfd, const std::string &h, uint16_t p)
{
	ws_deflate = false;

	auto _seed = std::chrono::steady_clock::now().time_since_epoch().count();
	std::mt19937 rng((uint32_t)(_seed ^ (_seed >> 32)));
	uint8_t key_bytes[16];
	for (auto &b : key_bytes) b = (uint8_t)(rng() & 0xFF);
	std::string ws_key = Base64Encode(key_bytes, 16);

	std::ostringstream req;
	req << "GET / HTTP/1.1\r\n"
	    << "Host: " << h << ":" << p << "\r\n"
	    << "Upgrade: websocket\r\n"
	    << "Connection: Upgrade\r\n"
	    << "Sec-WebSocket-Key: " << ws_key << "\r\n"
	    << "Sec-WebSocket-Version: 13\r\n"
#ifdef WITH_ZLIB
	    << "Sec-WebSocket-Extensions: permessage-deflate; client_no_context_takeover; server_no_context_takeover\r\n"
#endif
	    << "\r\n";

	std::string req_str = req.str();
	if (!SendAll(sockfd, req_str.c_str(), (int)req_str.size())) return false;

	std::string response;
	response.reserve(512);
	char c;
	while (response.size() < 4096) {
		if (!RecvAll(sockfd, &c, 1)) return false;
		response += c;
		if (response.size() >= 4 &&
		    response.substr(response.size() - 4) == "\r\n\r\n") break;
	}

	if (response.find("101") == std::string::npos) return false;

	/* Check if server accepted permessage-deflate */
#ifdef WITH_ZLIB
	std::string lower = response;
	for (auto &ch : lower) ch = (char)tolower((unsigned char)ch);
	if (lower.find("permessage-deflate") != std::string::npos) {
		ws_deflate = true;
		AP_OK("WebSocket connected — permessage-deflate compression enabled");
	} else {
		AP_OK("WebSocket connected (no compression)");
	}
#else
	AP_OK("WebSocket connected (compression disabled — build without zlib)");
#endif
	return true;
}

bool ArchipelagoClient::SendWsText(int sockfd, const std::string &text)
{
	size_t len = text.size();
	std::vector<char> frame;
	frame.reserve(len + 10);

	frame.push_back((char)0x81);

	if (len <= 125) {
		frame.push_back((char)(0x80 | len));
	} else if (len <= 65535) {
		frame.push_back((char)(0x80 | 126));
		frame.push_back((char)((len >> 8) & 0xFF));
		frame.push_back((char)(len & 0xFF));
	} else {
		frame.push_back((char)(0x80 | 127));
		for (int i = 7; i >= 0; i--)
			frame.push_back((char)((len >> (8*i)) & 0xFF));
	}

	auto _seed = std::chrono::steady_clock::now().time_since_epoch().count();
	std::mt19937 rng((uint32_t)(_seed ^ (_seed >> 32)));
	uint8_t mask[4];
	for (auto &b : mask) b = (uint8_t)(rng() & 0xFF);
	frame.push_back((char)mask[0]);
	frame.push_back((char)mask[1]);
	frame.push_back((char)mask[2]);
	frame.push_back((char)mask[3]);

	for (size_t i = 0; i < len; i++)
		frame.push_back((char)(text[i] ^ mask[i % 4]));

	return SendAll(sockfd, frame.data(), (int)frame.size());
}

bool ArchipelagoClient::RecvWsFrame(int sockfd, std::string &out_text, bool &out_closed)
{
	out_closed = false;

	char header[2];
	if (!RecvAll(sockfd, header, 2)) {
		if (sock_timed_out()) {
			return false; /* normal timeout — keep-alive */
		}
		out_closed = true;
		AP_WARN("RecvWsFrame: header recv failed (connection reset by server)");
		return false;
	}

	uint8_t opcode = header[0] & 0x0F;
	[[maybe_unused]] bool fin = (header[0] & 0x80) != 0;
	bool masked    = (header[1] & 0x80) != 0;
	uint64_t plen  = header[1] & 0x7F;

	if (plen == 126) {
		char ext[2]; if (!RecvAll(sockfd, ext, 2)) { out_closed = true; return false; }
		plen = ((uint8_t)ext[0] << 8) | (uint8_t)ext[1];
	} else if (plen == 127) {
		char ext[8]; if (!RecvAll(sockfd, ext, 8)) { out_closed = true; return false; }
		plen = 0;
		for (int i = 0; i < 8; i++) plen = (plen << 8) | (uint8_t)ext[i];
	}



	uint8_t mask[4] = {};
	if (masked) { if (!RecvAll(sockfd, (char*)mask, 4)) { out_closed = true; return false; } }

	if (opcode == 0x8) {
		/* Close frame — read close code if present */
		if (plen >= 2) {
			char close_buf[2] = {};
			RecvAll(sockfd, close_buf, 2);
			uint16_t close_code = ((uint8_t)close_buf[0] << 8) | (uint8_t)close_buf[1];
			AP_ERR(fmt::format("Server sent WebSocket CLOSE frame (code={})", (int)close_code));
		} else {
			AP_ERR("Server sent WebSocket CLOSE frame (no code)");
		}
		out_closed = true;
		return false;
	}
	if (opcode == 0x9) {
		/* Ping — send MASKED Pong (RFC 6455 §5.1: client->server MUST be masked) */

		std::vector<char> ping_payload(plen);
		if (plen > 0) RecvAll(sockfd, ping_payload.data(), (int)plen);

		/* Build masked pong frame */
		auto _seed = std::chrono::steady_clock::now().time_since_epoch().count();
		std::mt19937 rng_p((uint32_t)(_seed ^ (_seed >> 32)));
		uint8_t pmask[4];
		for (auto &b : pmask) b = (uint8_t)(rng_p() & 0xFF);

		std::vector<char> pong;
		pong.push_back((char)0x8A);                      /* FIN + opcode 0xA */
		pong.push_back((char)(0x80 | (plen & 0x7F)));    /* MASK bit set + length */
		pong.push_back((char)pmask[0]);
		pong.push_back((char)pmask[1]);
		pong.push_back((char)pmask[2]);
		pong.push_back((char)pmask[3]);
		for (size_t i = 0; i < plen; i++)
			pong.push_back((char)(ping_payload[i] ^ pmask[i % 4]));
		SendAll(sockfd, pong.data(), (int)pong.size());
		return true;
	}
	if (opcode == 0xA) {
		/* Pong — ignore */
		if (plen > 0) { std::vector<char> tmp(plen); RecvAll(sockfd, tmp.data(), (int)plen); }
		return true;
	}
	if (opcode != 0x1 && opcode != 0x0) {
		AP_WARN(fmt::format("Unexpected WS opcode 0x{:X}, plen={} — skipping", (int)opcode, (uint64_t)plen));
		/* Drain the payload so the stream stays in sync */
		if (plen > 0 && plen < 16*1024*1024ULL) {
			std::vector<char> drain(plen);
			RecvAll(sockfd, drain.data(), (int)plen);
		}
		return true;
	}

	/* Guard against absurdly large frames (>16 MB) */
	if (plen > 16 * 1024 * 1024ULL) {
		AP_ERR(fmt::format("Frame too large ({} bytes) — closing connection", (uint64_t)plen));
		out_closed = true;
		return false;
	}

	std::vector<char> payload(plen);
	if (plen > 0 && !RecvAll(sockfd, payload.data(), (int)plen)) {
		AP_ERR(fmt::format("RecvAll failed reading {}-byte payload (opcode=0x{:X})", (uint64_t)plen, (int)opcode));
		out_closed = true;
		return false;
	}
	if (masked) for (size_t i = 0; i < plen; i++) payload[i] ^= mask[i % 4];

	bool rsv1 = (header[0] & 0x40) != 0;
#ifdef WITH_ZLIB
	if (rsv1 && ws_deflate) {
		/* permessage-deflate: append RFC 7692 sync tail then inflate */
		payload.push_back(0x00);
		payload.push_back(0x00);
		payload.push_back((char)0xFF);
		payload.push_back((char)0xFF);

		z_stream zs{};
		inflateInit2(&zs, -15); /* raw deflate, no zlib header */
		zs.next_in  = (Bytef*)payload.data();
		zs.avail_in = (uInt)payload.size();

		std::string decompressed;
		char zbuf[32768];
		int zret;
		do {
			zs.next_out  = (Bytef*)zbuf;
			zs.avail_out = sizeof(zbuf);
			zret = inflate(&zs, Z_SYNC_FLUSH);
			if (zret != Z_OK && zret != Z_STREAM_END && zret != Z_BUF_ERROR) {
				inflateEnd(&zs);
				AP_ERR(fmt::format("zlib inflate error: {}", zret));
				out_closed = true;
				return false;
			}
			decompressed.append(zbuf, sizeof(zbuf) - zs.avail_out);
		} while (zs.avail_out == 0);
		inflateEnd(&zs);

		out_text = std::move(decompressed);
	} else {
		out_text.assign(payload.data(), plen);
	}
#else
	(void)rsv1;
	out_text.assign(payload.data(), plen);
#endif
	return true;
}

/* -------------------------------------------------------------------------
 * AP JSON protocol
 * ---------------------------------------------------------------------- */

void ArchipelagoClient::ProcessAPMessage(const std::string &text)
{
	json msgs;
	try { msgs = json::parse(text); } catch (...) { return; }
	if (!msgs.is_array()) return;

	for (auto &msg : msgs) {
		std::string cmd = msg.value("cmd", "");

		if (cmd == "RoomInfo") {
			AP_LOG("RoomInfo received — sending Connect packet...");
			/* Send Connect packet */
			json connect = json::array();
			connect.push_back({
				{"cmd",           "Connect"},
				{"game",          game_name},
				{"name",          slot_name},
				{"password",      password},
				{"uuid",          "openttd-archipelago-01"},
				{"version",       {{"major",0},{"minor",6},{"build",0},{"class","Version"}}},
				{"tags",          json::array({"DeathLink"})},
				{"items_handling", 7}
			});
			std::lock_guard<std::mutex> lg(outbound_mutex);
			outbound_queue.push_front({ connect.dump() });

		} else if (cmd == "Connected") {
			AP_OK("Authenticated! Parsing slot data...");
			state.store(APState::AUTHENTICATED);

			APSlotData sd = ParseSlotData(msg);
			if (msg.contains("checked_locations") && msg["checked_locations"].is_array()) {
				for (const auto &idv : msg["checked_locations"]) {
					if (idv.is_number_integer()) sd.checked_locations.insert(idv.get<int64_t>());
				}
				AP_LOG(fmt::format("Connected: {} checked locations restored", sd.checked_locations.size()));
			}
			{
				std::lock_guard<std::mutex> lg(slot_mutex);
				slot_data = sd;

				/* Build location name -> ID map from APWorld-provided slot_data. */
				location_ids.clear();
				location_ids = sd.location_name_to_id;

				/* Build reverse map: ID → name */
				location_id_to_name.clear();
				for (auto &[name, id] : location_ids)
					location_id_to_name[id] = name;

				/* Parse player slot info: slot_info is object of {slot_id: {name, alias, game}} */
				player_id_to_name.clear();
				player_id_to_game.clear();
				if (msg.contains("slot_info") && msg["slot_info"].is_object()) {
					for (auto &[key, val] : msg["slot_info"].items()) {
						auto pid_opt = ParseInteger<int64_t>(key);
						if (!pid_opt.has_value()) continue;
						int64_t pid = *pid_opt;
						std::string alias = val.value("name", key);
						if (val.contains("alias")) alias = val["alias"].get<std::string>();
						player_id_to_name[pid] = alias;
						player_id_to_game[pid] = val.value("game", "");
					}
				}
			}
			has_slot_data.store(true);
			AP_OK(fmt::format("Slot data parsed: {} missions, start_year={}, vehicle='{}'",
			      sd.mission_count, sd.start_year, sd.starting_vehicle));

			PushEvent({ InboundEvent::CONNECTED, {}, {}, {} });

			/* Location scouting request removed in current apworld payload. */
			InboundEvent sdev;
			sdev.type = InboundEvent::SLOT_DATA;
			sdev.slot = sd;
			PushEvent(std::move(sdev));

		} else if (cmd == "ConnectionRefused") {
			std::string reason;
			if (msg.contains("errors") && msg["errors"].is_array()) {
				for (auto &e : msg["errors"]) reason += e.get<std::string>() + " ";
			}
			last_error = "Connection refused: " + reason;
			AP_ERR("Server refused connection: " + reason);
			state.store(APState::AP_ERROR);
			PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });

		} else if (cmd == "ReceivedItems") {
			/* AP protocol sends item_id, not item_name.
			 * We resolve the name from the item_id_to_name map built in ParseSlotData.
			 * The 'index' field is the starting index in the server's full item list for this
			 * player — used to detect replayed items on reconnect so the client can apply
			 * their effects silently without re-showing news messages. */
			if (!msg.contains("items") || !msg["items"].is_array()) continue;

			APSlotData current_sd;
			{
				std::lock_guard<std::mutex> lg(slot_mutex);
				current_sd = slot_data;
			}

			int64_t batch_index    = msg.contains("index") ? msg["index"].get<int64_t>() : 0;
			int64_t already_known  = items_received_index.load();
			int64_t item_i         = 0;

			for (auto &item : msg["items"]) {
				APItem ap_item;
				ap_item.item_id   = item.value("item", (int64_t)0);
				ap_item.is_replay = (batch_index + item_i < already_known);

				/* Resolve name from our id->name map */
				auto name_it = current_sd.item_id_to_name.find(ap_item.item_id);
				if (name_it != current_sd.item_id_to_name.end()) {
					ap_item.item_name = name_it->second;
				} else {
					/* Fallback: AP might include item_name directly in some versions */
					ap_item.item_name = item.value("item_name", "");
					if (ap_item.item_name.empty()) {
						Debug(misc, 1, "[AP] WARNING: unknown item id {}, no name mapping",
						      ap_item.item_id);
					}
				}

				InboundEvent ev;
				ev.type = InboundEvent::ITEM;
				ev.item = ap_item;
				PushEvent(std::move(ev));
				item_i++;
			}

			/* Advance high-water mark */
			int64_t new_mark = batch_index + item_i;
			if (new_mark > already_known) items_received_index.store(new_mark);

		} else if (cmd == "LocationInfo") {
			/* Response to LocationScouts — build location_id_to_hint map */
			if (msg.contains("locations") && msg["locations"].is_array()) {
				for (auto &loc : msg["locations"]) {
					int64_t loc_id = loc.value("location", (int64_t)0);
					int64_t player = loc.value("player",   (int64_t)0);
					/* Build "player (game)" label */
					std::string pname, pgame;
					{
						auto it = player_id_to_name.find(player);
						pname = (it != player_id_to_name.end()) ? it->second : fmt::format("Player {}", player);
					}
					{
						auto it = player_id_to_game.find(player);
						if (it != player_id_to_game.end()) pgame = it->second;
					}
					location_id_to_hint[loc_id] = pgame.empty()
					    ? pname
					    : fmt::format("{} ({})", pname, pgame);
				}
				AP_LOG(fmt::format("LocationInfo: {} locations resolved", msg["locations"].size()));
			}

		} else if (cmd == "PrintJSON") {
			std::string text_out;
			APPrintColour msg_colour = APPrintColour::DEFAULT;
			bool colour_set = false;

			auto ParseColourName = [](const std::string &name) -> APPrintColour {
				if (name == "red") return APPrintColour::RED;
				if (name == "green") return APPrintColour::GREEN;
				if (name == "yellow") return APPrintColour::YELLOW;
				if (name == "blue") return APPrintColour::BLUE;
				if (name == "magenta") return APPrintColour::MAGENTA;
				if (name == "cyan") return APPrintColour::CYAN;
				if (name == "white") return APPrintColour::WHITE;
				if (name == "orange") return APPrintColour::ORANGE;
				if (name == "plum") return APPrintColour::PLUM;
				if (name == "slateblue") return APPrintColour::SLATEBLUE;
				if (name == "salmon") return APPrintColour::SALMON;
				return APPrintColour::DEFAULT;
			};

			auto DerivePartColour = [&](const json &part, const std::string &ptype) -> APPrintColour {
				if (part.contains("color") && part["color"].is_string()) {
					return ParseColourName(part["color"].get<std::string>());
				}
				if (ptype == "item_name" || ptype == "item_id") {
					int flags = part.value("flags", 0);
					if (flags == 0) return APPrintColour::CYAN;
					if ((flags & 0b001) != 0) return APPrintColour::PLUM;
					if ((flags & 0b010) != 0) return APPrintColour::SLATEBLUE;
					if ((flags & 0b100) != 0) return APPrintColour::SALMON;
					return APPrintColour::CYAN;
				}
				if (ptype == "location_name" || ptype == "location_id") return APPrintColour::GREEN;
				if (ptype == "entrance_name") return APPrintColour::BLUE;
				if (ptype == "player_id") return APPrintColour::YELLOW;
				if (ptype == "player_name") return APPrintColour::YELLOW;
				if (ptype == "hint_status") {
					int hs = part.value("hint_status", 0);
					switch (hs) {
						case 40: return APPrintColour::GREEN;     /* found */
						case 10: return APPrintColour::SLATEBLUE; /* no priority */
						case 20: return APPrintColour::SALMON;    /* avoid */
						case 30: return APPrintColour::PLUM;      /* priority */
						case 0:  return APPrintColour::WHITE;     /* unspecified */
						default: return APPrintColour::DEFAULT;
					}
				}
				return APPrintColour::DEFAULT;
			};

			if (msg.contains("data") && msg["data"].is_array()) {
				APSlotData current_sd;
				{ std::lock_guard<std::mutex> lg(slot_mutex); current_sd = slot_data; }

				for (auto &part : msg["data"]) {
					std::string ptype = part.value("type", "text");
					std::string ptext = part.value("text", "");

					if (!colour_set) {
						APPrintColour part_colour = DerivePartColour(part, ptype);
						if (part_colour != APPrintColour::DEFAULT) {
							msg_colour = part_colour;
							colour_set = true;
						}
					}

					if (ptype == "item_id" || ptype == "item_name") {
						/* Resolve item ID → human name */
						auto iid_opt = ParseInteger<int64_t>(ptext);
						if (iid_opt.has_value()) {
							auto it = current_sd.item_id_to_name.find(*iid_opt);
							if (it != current_sd.item_id_to_name.end())
								ptext = it->second;
							else
								ptext = "an item"; /* Foreign game item — ID not in our map */
						}
						text_out += ptext;
					} else if (ptype == "location_id" || ptype == "location_name") {
						/* Resolve location ID → human name */
						auto lid_opt = ParseInteger<int64_t>(ptext);
						if (lid_opt.has_value()) {
							auto it = location_id_to_name.find(*lid_opt);
							if (it != location_id_to_name.end())
								ptext = it->second;
							else
								ptext = "a location"; /* Foreign game location — ID not in our map */
						}
						text_out += ptext;
					} else if (ptype == "player_id") {
						/* Resolve player slot ID → alias */
						auto pid_opt = ParseInteger<int64_t>(ptext);
						if (pid_opt.has_value()) {
							auto it = player_id_to_name.find(*pid_opt);
							if (it != player_id_to_name.end())
								ptext = it->second;
						}
						text_out += ptext;
					} else {
						text_out += ptext;
					}
				}
			}
			if (!text_out.empty()) PushEvent({ InboundEvent::PRINT, text_out, msg_colour, {}, {} });

		} else if (cmd == "RoomUpdate") {
			/* Sent when another client for this slot checks locations.
			 * Merge the new locations into our slot_data so completion %
			 * and shop purchased state reflect them without a reconnect. */
			if (msg.contains("checked_locations") && msg["checked_locations"].is_array()) {
				size_t added = 0;
				{
					std::lock_guard<std::mutex> lg(slot_mutex);
					for (const auto &idv : msg["checked_locations"]) {
						if (idv.is_number_integer()) {
							if (slot_data.checked_locations.insert(idv.get<int64_t>()).second)
								++added;
						}
					}
				}
				if (added > 0) {
					AP_LOG(fmt::format("RoomUpdate: {} new checked location(s) merged", added));
					PushEvent({ InboundEvent::LOCATIONS_UPDATED, {}, {}, {}, {} });
				}
			}

		}
	}
}

/* -------------------------------------------------------------------------
 * Worker thread
 * ---------------------------------------------------------------------- */

#ifdef _WIN32
void ArchipelagoClient::InitWinsock()
{
	WSADATA wd;
	WSAStartup(MAKEWORD(2, 2), &wd);
}

/** Detect Wine by checking for its registry key.
 *  Wine's Schannel implementation is incomplete — AcquireCredentialsHandleA
 *  with UNISP_NAME_A (the SChannel SSL/TLS provider) either hangs or returns
 *  an error, making the WSS probe in WorkerThread crash the connection.
 *  When running under Wine we skip the WSS probe entirely and go straight
 *  to plain WS. */
static bool IsRunningUnderWine()
{
	HKEY hk;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wine",
	                  0, KEY_READ, &hk) == ERROR_SUCCESS) {
		RegCloseKey(hk);
		return true;
	}
	return false;
}
#endif

void ArchipelagoClient::WorkerThread()
{
#ifdef _WIN32
	InitWinsock();
#endif

	struct addrinfo hints{}, *res = nullptr;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	std::string port_str = fmt::format("{}", port);

	AP_LOG(fmt::format("Resolving host {}:{}...", host, port));
	if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || res == nullptr) {
		last_error = "Could not resolve host: " + host;
		AP_ERR("DNS lookup failed for " + host);
		state.store(APState::AP_ERROR);
		PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
		return;
	}

	sock_t s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (s == SOCK_INVALID) {
		freeaddrinfo(res);
		last_error = "Socket creation failed";
		state.store(APState::AP_ERROR);
		PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
		return;
	}

	AP_LOG(fmt::format("TCP connecting to {}:{}...", host, port));
	if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
		freeaddrinfo(res);
		sock_close(s);
		last_error = "Could not connect to " + host + ":" + fmt::format("{}", port);
		AP_ERR("TCP connection refused — is the AP server running?");
		state.store(APState::AP_ERROR);
		PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
		return;
	}
	freeaddrinfo(res);
	AP_OK("TCP connected! Starting WebSocket handshake...");

/* ── Auto-detect WSS vs WS ──────────────────────────────────────────────
 * Strategy: try WSS (TLS) first. If TLS or WS handshake fails, reconnect
 * and retry as plain WS. This means users never need to type wss:// or ws://.
 * Exception: Wine does not implement Schannel properly, so we skip the WSS
 * probe entirely when running under Wine and go straight to plain WS.
 * ──────────────────────────────────────────────────────────────────────── */
#ifdef _WIN32
	ApTlsCtx *tls = nullptr;
	bool ws_ok = false;
	bool under_wine = IsRunningUnderWine();

	if (under_wine) {
		AP_WARN("Running under Wine — skipping WSS probe, using plain WS directly.");
	}

	/* Attempt 1: WSS — skipped on Wine */
	if (!under_wine) {
		AP_LOG("Trying WSS (TLS)...");
		tls = new ApTlsCtx();
		if (tls->Handshake(s, host)) {
			tls_ctx_tl = tls;
			AP_OK("TLS handshake OK — trying WebSocket upgrade...");
			if (DoWebSocketHandshake((int)s, host, port)) {
				ws_ok = true;
				AP_OK("WSS connection established.");
			} else {
				AP_WARN("WSS WebSocket handshake failed — falling back to plain WS.");
				delete tls; tls = nullptr; tls_ctx_tl = nullptr;
			}
		} else {
			AP_WARN("TLS handshake failed — falling back to plain WS.");
			delete tls; tls = nullptr;
		}
	}

	if (!ws_ok) {
		/* Attempt 2: plain WS — reconnect the socket */
		sock_close(s);
		if (tls_ctx_tl) { delete tls_ctx_tl; tls_ctx_tl = nullptr; }

		struct addrinfo hints2{}, *res2 = nullptr;
		hints2.ai_family   = AF_UNSPEC;
		hints2.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(host.c_str(), port_str.c_str(), &hints2, &res2) != 0 || res2 == nullptr) {
			last_error = "Could not resolve host: " + host;
			state.store(APState::AP_ERROR);
			PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
			return;
		}
		s = socket(res2->ai_family, res2->ai_socktype, res2->ai_protocol);
		if (s == SOCK_INVALID) {
			freeaddrinfo(res2);
			last_error = "Socket creation failed (WS retry)";
			state.store(APState::AP_ERROR);
			PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
			return;
		}
		AP_LOG(fmt::format("Retrying TCP connect to {}:{} (plain WS)...", host, port));
		if (connect(s, res2->ai_addr, (int)res2->ai_addrlen) != 0) {
			freeaddrinfo(res2);
			sock_close(s);
			last_error = "Could not connect to " + host + ":" + fmt::format("{}", port);
			state.store(APState::AP_ERROR);
			PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
			return;
		}
		freeaddrinfo(res2);
		AP_LOG("TCP reconnected — trying plain WebSocket...");
		if (!DoWebSocketHandshake((int)s, host, port)) {
			sock_close(s);
			last_error = "WebSocket handshake failed (both WSS and WS attempted)";
			AP_ERR("Both WSS and WS failed — check server address.");
			state.store(APState::AP_ERROR);
			PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
			return;
		}
		AP_OK("Plain WS connection established.");
	}
#else
	/* Non-Windows: plain WS only */
	if (!DoWebSocketHandshake((int)s, host, port)) {
		sock_close(s);
		last_error = "WebSocket handshake failed";
		state.store(APState::AP_ERROR);
		PushEvent({ InboundEvent::DISCONNECTED, last_error, {}, {} });
		return;
	}
	AP_OK("WS connection established.");
#endif

	/* Set recv timeout AFTER handshake — 200ms so disconnect/stop_requested is noticed quickly */
#ifdef _WIN32
	DWORD timeout_ms = 200;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
	struct timeval tv{0, 200000};
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

	state.store(APState::AUTHENTICATING);
	AP_LOG("Waiting for AP server messages...");

	while (!stop_requested.load()) {
		for (;;) {
			std::string msg = PopOutbound();
			if (msg.empty()) break;
			if (!SendWsText((int)s, msg)) {
				AP_ERR("SendWsText failed — outbound send error, closing");
				stop_requested.store(true);
				break;
			}
		}
		if (stop_requested.load()) break;

		std::string frame_text;
		bool closed = false;
		if (!RecvWsFrame((int)s, frame_text, closed)) {
			if (closed) {
				AP_ERR("RecvWsFrame: closed=true — exiting worker loop");
				break;
			}
			continue; /* timeout — keep alive */
		}

		if (!frame_text.empty()) {
			ProcessAPMessage(frame_text);
		}
	}

	sock_close(s);
#ifdef _WIN32
	if (tls_ctx_tl) { delete tls_ctx_tl; tls_ctx_tl = nullptr; }
#endif
	AP_WARN("Worker thread exiting — connection lost");
	if (state.load() != APState::AP_ERROR) state.store(APState::DISCONNECTED);
	PushEvent({ InboundEvent::DISCONNECTED, "Disconnected", {}, {} });
}
