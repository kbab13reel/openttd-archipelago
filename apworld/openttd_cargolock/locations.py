from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING
from BaseClasses import Location, Region, ItemClassification
from . import items
from .items import OpenTTDItem
if TYPE_CHECKING:
    from .world import OpenTTDWorld

CARGO_TYPES = [  # Temperate
        "Passengers", "Mail", "Coal", "Oil",
        "Livestock", "Goods", "Grain", "Wood",
        "Iron Ore", "Steel", "Valuables",
    ]

# ── Tiered cargo mission constants ────────────────────────────────────────
# Maximum number of tiered missions that can ever be registered per cargo.
# Must be at least as large as the largest per-cargo YAML option allows.
CARGO_TIER_MAX = 10

# Default number of tiers generated when no YAML option overrides.
CARGO_TIER_DEFAULT = 3

# First tier amount; each subsequent tier is ×10.
CARGO_TIER_BASE = 1_000

# Per-cargo YAML key suffixes match the lowercase, underscored cargo name.
# e.g. "Iron Ore" → option key "iron_ore_tiered_mission_count"
def _cargo_option_key(cargo: str) -> str:
    return cargo.lower().replace(" ", "_") + "_tiered_mission_count"

# Pre-registered tiered location names (cargo × max tiers).
# Location names take the form "Transport <Cargo> <N>" where N is 1-based.
# The hardcoded "Transport 1 unit of X" locations remain under their original names.
_TIERED_LOCATION_NAMES: List[str] = [
    f"Transport {cargo} {tier}"
    for cargo in CARGO_TYPES
    for tier in range(1, CARGO_TIER_MAX + 1)
]

# ── Cargo-by-vehicle mission constants ────────────────────────────────────
# All valid (cargo, vehicle_key, vehicle_display) combinations.
# Aircraft can only carry Passengers, Mail, Goods, Valuables → 4 combos.
# Trains/Road Vehicles/Ships carry all 11 → 33 combos.  Total: 37.
_VEHICLE_DISPLAY: Dict[str, str] = {
    "train":        "Train",
    "road_vehicle": "Road Vehicle",
    "ship":         "Ship",
    "aircraft":     "Aircraft",
}
_AIRCRAFT_CARGOS = ["Passengers", "Mail", "Goods", "Valuables"]

CARGO_VEHICLE_COMBINATIONS: List[tuple] = [
    (cargo, vkey, _VEHICLE_DISPLAY[vkey])
    for vkey, compat in [
        ("train",        CARGO_TYPES),
        ("road_vehicle", CARGO_TYPES),
        ("ship",         CARGO_TYPES),
        ("aircraft",     _AIRCRAFT_CARGOS),
    ]
    for cargo in compat
]

_ALL_CARGO_VEHICLE_LOCATION_NAMES: List[str] = [
    f"Transport {cargo} by {vdisp}"
    for cargo, vkey, vdisp in CARGO_VEHICLE_COMBINATIONS
]

MISSION_DEFINITIONS = [
    # EARLY MISSIONS
    {"location": "Buy your first vehicle", "description": "Buy your first vehicle", "type": "have", "cargo": "", "unit": "", "amount": 1, "difficulty": "normal"},
    {"location": "Build your first station", "description": "Build your first station", "type": "build", "cargo": "", "unit": "", "amount": 1, "difficulty": "normal"},

    # VEHICLE MISSIONS
    {"location": "Own 1 train", "description": "Own 1 train", "type": "have", "cargo": "", "unit": "trains", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 road vehicle", "description": "Own 1 road vehicle", "type": "have", "cargo": "", "unit": "road vehicles", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 ship", "description": "Own 1 ship", "type": "have", "cargo": "", "unit": "ships", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 aircraft", "description": "Own 1 aircraft", "type": "have", "cargo": "", "unit": "aircraft", "amount": 1, "difficulty": "normal"},
    {"location": "Own 2 trains", "description": "Own 2 trains", "type": "have", "cargo": "", "unit": "trains", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 road vehicles", "description": "Own 2 road vehicles", "type": "have", "cargo": "", "unit": "road vehicles", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 ships", "description": "Own 2 ships", "type": "have", "cargo": "", "unit": "ships", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 aircrafts", "description": "Own 2 aircrafts", "type": "have", "cargo": "", "unit": "aircraft", "amount": 2, "difficulty": "normal"},

    # STATION MULTI-MODE MISSIONS
    {"location": "Have a station handle 2 vehicle types", "description": "Have a station handle 2 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 2, "difficulty": "normal"},
    {"location": "Have a station handle 3 vehicle types", "description": "Have a station handle 3 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 3, "difficulty": "normal"},
    {"location": "Have a station handle 4 vehicle types", "description": "Have a station handle 4 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 4, "difficulty": "normal"},

    # CARGO FIRST-DELIVERY MISSIONS (1 unit — hardcoded, always present)
    {"location": "Transport 1 unit of Passengers", "description": "Transport 1 unit of Passengers", "type": "transport", "cargo": "Passengers", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Mail", "description": "Transport 1 unit of Mail", "type": "transport", "cargo": "Mail", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Coal", "description": "Transport 1 unit of Coal", "type": "transport", "cargo": "Coal", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Oil", "description": "Transport 1 unit of Oil", "type": "transport", "cargo": "Oil", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Livestock", "description": "Transport 1 unit of Livestock", "type": "transport", "cargo": "Livestock", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Goods", "description": "Transport 1 unit of Goods", "type": "transport", "cargo": "Goods", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Grain", "description": "Transport 1 unit of Grain", "type": "transport", "cargo": "Grain", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Wood", "description": "Transport 1 unit of Wood", "type": "transport", "cargo": "Wood", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Iron Ore", "description": "Transport 1 unit of Iron Ore", "type": "transport", "cargo": "Iron Ore", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Steel", "description": "Transport 1 unit of Steel", "type": "transport", "cargo": "Steel", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Valuables", "description": "Transport 1 unit of Valuables", "type": "transport", "cargo": "Valuables", "unit": "units", "amount": 1, "difficulty": "normal"},
]

# Legacy table removed — costs now randomized between ShopCostMin and ShopCostMax.
BASE_SHOP_COSTS: List[int] = []

MAX_SHOP_SLOTS = 100

MISSIONS = [mission["location"] for mission in MISSION_DEFINITIONS]

# Export location name to ID mapping for Archipelago.
# IDs 1..len(MISSIONS)                                     — static missions
# IDs ..+len(_TIERED_LOCATION_NAMES)                       — pre-registered tiered slots (all 110)
# IDs ..+len(_ALL_CARGO_VEHICLE_LOCATION_NAMES)            — pre-registered cargo-by-vehicle slots (all 37)
# IDs ..+MAX_SHOP_SLOTS                                    — shop slots
LOCATION_NAME_TO_ID: Dict[str, int] = {
    name: i for i, name in enumerate(MISSIONS, start=1)
}

_tiered_id_start = len(MISSIONS) + 1
for _i, _name in enumerate(_TIERED_LOCATION_NAMES, start=_tiered_id_start):
    LOCATION_NAME_TO_ID[_name] = _i

_cv_id_start = _tiered_id_start + len(_TIERED_LOCATION_NAMES)
for _i, _name in enumerate(_ALL_CARGO_VEHICLE_LOCATION_NAMES, start=_cv_id_start):
    LOCATION_NAME_TO_ID[_name] = _i

_shop_id_start = _cv_id_start + len(_ALL_CARGO_VEHICLE_LOCATION_NAMES)
for index in range(1, MAX_SHOP_SLOTS + 1):
    LOCATION_NAME_TO_ID[f"Purchase Shop {index}"] = _shop_id_start + index - 1


def get_shop_slot_count(world: OpenTTDWorld) -> int:
    if not world.options.enable_shop.value:
        return 0
    return world.options.shop_slots.value


def get_shop_tier_count(world: OpenTTDWorld) -> int:
    shop_slots = get_shop_slot_count(world)
    if shop_slots <= 0:
        return 0
    return min(shop_slots, world.options.shop_tiers.value)


def get_shop_definitions(world: OpenTTDWorld) -> List[Dict[str, object]]:
    """Generate shop locations with random costs between ShopCostMin and ShopCostMax.

    The generated list is cached on the world instance so all callers in one
    generation pass see the exact same shop costs and order.
    """
    cached = getattr(world, "_openttd_cached_shop_definitions", None)
    if cached is not None:
        return cached

    shop_slots = get_shop_slot_count(world)
    if shop_slots <= 0:
        setattr(world, "_openttd_cached_shop_definitions", [])
        return []
    
    min_cost = world.options.shop_cost_min.value
    max_cost = world.options.shop_cost_max.value
    
    # Ensure min <= max
    if min_cost > max_cost:
        min_cost, max_cost = max_cost, min_cost
    
    generated = [
        {
            "location": f"Purchase Shop {index}",
            "name": f"Shop {index}",
            "cost": _generate_random_shop_cost(world.random, min_cost, max_cost),
        }
        for index in range(1, shop_slots + 1)
    ]
    setattr(world, "_openttd_cached_shop_definitions", generated)
    return generated


def _generate_random_shop_cost(rng: "object", min_cost: int, max_cost: int) -> int:
    """Generate a random shop cost rounded to nearest 5000."""
    cost = rng.randint(min_cost, max_cost)
    return int(round(cost / 5000.0) * 5000)


def _get_tiered_mission_count(world: "OpenTTDWorld", cargo: str) -> int:
    """Return the number of tiered missions for this cargo from YAML options.
    If the per-cargo option is 0 (default), fall back to the global setting."""
    key = _cargo_option_key(cargo)
    opt = getattr(world.options, key, None)
    if opt is not None and int(opt.value) > 0:
        return int(opt.value)
    return int(world.options.global_tiered_mission_count.value)


def get_tiered_missions(world: "OpenTTDWorld") -> List[Dict[str, object]]:
    """Generate tiered transport missions for all cargo types.

    Tier N location name: "Transport <Cargo> <N>"
    Tier 1 amount = CARGO_TIER_BASE (1 000); each subsequent tier is ×10.
    The count per cargo comes from per-cargo YAML option, falling back to
    global_tiered_mission_count.
    """
    tiered: List[Dict[str, object]] = []
    for cargo in CARGO_TYPES:
        tier_count = _get_tiered_mission_count(world, cargo)
        tier_count = max(0, min(tier_count, CARGO_TIER_MAX))
        for tier in range(1, tier_count + 1):
            amount = CARGO_TIER_BASE * (10 ** (tier - 1))
            location = f"Transport {cargo} {tier}"
            tiered.append({
                "location": location,
                "description": f"Transport {amount:,} units of {cargo}",
                "type": "transport",
                "cargo": cargo,
                "tier": tier,
                "unit": "units",
                "amount": amount,
                "difficulty": "normal",
            })
    return tiered


def get_cargo_vehicle_missions(world: "OpenTTDWorld") -> List[Dict[str, object]]:
    """Return the randomly-selected subset of cargo-by-vehicle missions for this seed.

    YAML option `cargo_vehicle_mission_count`:
      0  → no missions generated
      37 → all 37 combinations
      N  → N randomly chosen from the 37 using world.random (deterministic per seed)
    """
    cached = getattr(world, "_openttd_cached_cv_missions", None)
    if cached is not None:
        return cached

    count = int(world.options.cargo_vehicle_mission_count.value)
    if count == 0:
        selected: List[tuple] = []
    elif count >= len(CARGO_VEHICLE_COMBINATIONS):
        selected = list(CARGO_VEHICLE_COMBINATIONS)
    else:
        selected = world.random.sample(CARGO_VEHICLE_COMBINATIONS, count)

    result = [
        {
            "location":    f"Transport {cargo} by {vdisp}",
            "description": f"Transport 1 unit of {cargo} using a {vdisp}",
            "type":        "transport",
            "cargo":       cargo,
            "unit":        "units",
            "amount":      1,
            "difficulty":  "normal",
            "vehicle_key": vkey,
        }
        for cargo, vkey, vdisp in selected
    ]
    setattr(world, "_openttd_cached_cv_missions", result)
    return result


def get_slot_missions(world: "OpenTTDWorld") -> List[Dict[str, object]]:
    return [dict(m) for m in MISSION_DEFINITIONS] + get_tiered_missions(world) + get_cargo_vehicle_missions(world)


def get_slot_shop_locations(world: OpenTTDWorld) -> List[Dict[str, object]]:
    return [dict(shop) for shop in get_shop_definitions(world)]


def get_slot_location_name_to_id(world: OpenTTDWorld) -> Dict[str, int]:
    selected_names = list(MISSIONS)
    selected_names.extend(m["location"] for m in get_tiered_missions(world))
    selected_names.extend(m["location"] for m in get_cargo_vehicle_missions(world))
    selected_names.extend(shop["location"] for shop in get_shop_definitions(world))
    return {name: LOCATION_NAME_TO_ID[name] for name in selected_names}

class OpenTTDLocation(Location):
    game = "OpenTTD Cargolock"


def create_all_locations(world: OpenTTDWorld) -> List[OpenTTDLocation]:
    overworld = Region("Overworld", world.player, world.multiworld)
    world.multiworld.regions += [overworld]
    for name in MISSIONS:
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))
    for mission in get_tiered_missions(world):
        name = mission["location"]
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))
        # Create a hidden event location for each tier so rules can gate the next tier.

        # Event locations have id=None and are invisible to the player.
        cargo = mission["cargo"]
        tier = mission["tier"]
        event_loc_name = f"Transport {cargo} {tier} Complete"
        event_item_name = f"Transport {cargo} {tier} Done"
        event_loc = OpenTTDLocation(world.player, event_loc_name, None, overworld)
        event_item = OpenTTDItem(event_item_name, ItemClassification.progression, None, world.player)
        event_loc.place_locked_item(event_item)
        overworld.locations.append(event_loc)
    for mission in get_cargo_vehicle_missions(world):
        name = mission["location"]
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))
    for shop in get_shop_definitions(world):
        name = shop["location"]
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))

