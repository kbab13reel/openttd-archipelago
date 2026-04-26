
from __future__ import annotations
from typing import TYPE_CHECKING, List
from BaseClasses import CollectionState
from .locations import get_shop_definitions, get_shop_slot_count, get_shop_tier_count, get_tiered_missions, get_cargo_vehicle_missions
if TYPE_CHECKING:
    from .world import OpenTTDWorld

# All cargo types in Temperate landscape
CARGO_TYPES = {"Passengers", "Mail", "Coal", "Oil", "Livestock", "Grain", "Wood", "Iron Ore", "Steel", "Goods", "Valuables"}

# Cargo dependencies in Temperate landscape based on industry chain:
CARGO_DEPENDENCIES = {
    "Passengers": set(),
    "Mail": set(),
    "Coal": set(),
    "Oil": set(),
    "Livestock": set(),
    "Grain": set(),
    "Wood": set(),
    "Iron Ore": set(),
    "Valuables": set(),            
    "Steel": {"Iron Ore"},
    "Goods": {"Wood", "Grain", "Livestock", "Steel", "Oil"},
}

VEHICULE_CARGO_COMPATIBILITY = {
    "train": CARGO_TYPES,
    "road_vehicle": CARGO_TYPES,
    "ship": CARGO_TYPES,
    "aircraft": {"Passengers", "Mail", "Goods", "Valuables"}
}


def has_vehicle_type(state: CollectionState, player: int, vehicle_type: str) -> bool:
    """Check if player has unlocked a specific vehicle type tier.
    
    Args:
        state: The current game state
        player: The player ID
        vehicle_type: One of "train", "road_vehicle", "ship", or "aircraft"
    """
    vehicle_items = {
        "train": "Progressive Trains",
        "road_vehicle": "Progressive Road Vehicles",
        "ship": "Progressive Ships",
        "aircraft": "Progressive Aircrafts",
    }
    
    item_name = vehicle_items.get(vehicle_type)
    return item_name and state.has(item_name, player)


def has_cargo(state: CollectionState, player: int, cargo: str) -> bool:
    """Check if player has unlocked a cargo type.
    
    For dependent cargos (Goods, Steel, Valuables), this also checks that 
    at least one required source cargo has been unlocked.
    
    Args:
        state: The current game state
        player: The player ID
        cargo: The cargo type name
    """
    if not state.has(cargo, player):
        return False
    
    # For dependent cargos, check that at least one source cargo is available
    dependencies = CARGO_DEPENDENCIES.get(cargo, set())
    if dependencies:
        # At least one dependency must be met
        if not any(state.has(dep, player) for dep in dependencies):
            return False
    
    # Must have a vehicle that can transport this cargo type
    vehicle_types = ["train", "road_vehicle", "ship", "aircraft"]
    for vehicle_type in vehicle_types:
        if has_vehicle_type(state, player, vehicle_type):
            if cargo in VEHICULE_CARGO_COMPATIBILITY[vehicle_type]:
                return True
    return False


def unlocked_vehicle_type_count(state: CollectionState, player: int) -> int:
    """Count currently unlocked transport modes from progressive items."""
    return int(has_vehicle_type(state, player, "train")) + \
        int(has_vehicle_type(state, player, "road_vehicle")) + \
        int(has_vehicle_type(state, player, "ship")) + \
        int(has_vehicle_type(state, player, "aircraft"))


def has_vehicle_tier_for_cargo(state: CollectionState, player: int, cargo: str, tier: int) -> bool:
    """Return True if the player has at least `tier` progressive vehicles that can carry this cargo.

    Tier mapping for tiered missions:
      transport tier 1 (1 000)   → vehicle tier 1 (covered by has_cargo)
      transport tier 2 (10 000)  → vehicle tier 2
      transport tier 3 (100 000) → vehicle tier 3
      transport tier 4+ (1 M+)   → vehicle tier 4
    """
    vehicle_items = {
        "train": "Progressive Trains",
        "road_vehicle": "Progressive Road Vehicles",
        "ship": "Progressive Ships",
        "aircraft": "Progressive Aircrafts",
    }
    return any(
        state.count(vehicle_items[vtype], player) >= tier
        for vtype, cargos in VEHICULE_CARGO_COMPATIBILITY.items()
        if cargo in cargos
    )


def has_any_utility(state: CollectionState, player: int, locked_utilities: List[str]) -> bool:
    """Return True if at least one of the given locked utility items is in the state.
    Always returns True when the list is empty (no utilities locked)."""
    if not locked_utilities:
        return True
    return state.has_any(locked_utilities, player)


def visible_shop_location_count(state: CollectionState, player: int, shop_slots: int, shop_tiers: int) -> int:
    if shop_slots <= 0 or shop_tiers <= 0:
        return 0
    slots_per_tier = (shop_slots + shop_tiers - 1) // shop_tiers
    unlocked_tiers = 1 + state.count("Progressive Shop Upgrade", player)
    return min(shop_slots, slots_per_tier * unlocked_tiers)


def set_all_rules(world: "OpenTTDWorld") -> None:
    """Set access rules for locations.
    
    Vehicle missions require the corresponding progressive vehicle item.
    Cargo missions require the cargo item and any necessary dependencies.
    """
    multiworld = world.multiworld
    player = world.player
    shop_slots = get_shop_slot_count(world)
    shop_tiers = get_shop_tier_count(world)
    shop_definitions = get_shop_definitions(world)
    
    for location in world.multiworld.get_locations(player):
        name = location.name

        # EARLY MISSIONS
        if name == "Build your first station" or name == "Buy your first vehicle":
            location.access_rule = lambda state: state.has_any(["Progressive Trains", "Progressive Road Vehicles", "Progressive Aircrafts", "Progressive Ships"], player)

        # VEHICLE MISSIONS
        if name.startswith("Own"):
            if "train" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "train")
            elif "road vehicle" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "road_vehicle")
            elif "ship" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "ship")
            elif "aircraft" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "aircraft")

        # STATION MULTI-MODE MISSIONS
        elif name.startswith("Have a station handle "):
            required_types = 0
            if "2 vehicle types" in name:
                required_types = 2
            elif "3 vehicle types" in name:
                required_types = 3
            elif "4 vehicle types" in name:
                required_types = 4
            if required_types > 0:
                location.access_rule = lambda state, required_types=required_types: unlocked_vehicle_type_count(state, player) >= required_types

        # CARGO MISSIONS
        elif name.startswith("Transport"):
            for cargo in CARGO_TYPES:
                if cargo in name:
                    location.access_rule = lambda state, cargo=cargo: has_cargo(state, player, cargo)
                    break

        elif name.startswith("Purchase Shop "):
            shop_index = next((index for index, shop in enumerate(shop_definitions, start=1) if shop["location"] == name), 0)
            if shop_index > 0:
                location.access_rule = lambda state, shop_index=shop_index: visible_shop_location_count(state, player, shop_slots, shop_tiers) >= shop_index

    # ── TIERED CARGO MISSION RULES ─────────────────────────────────────────
    # Overrides the generic "has_cargo" rule set above for Transport X N names.
    # Each tier N requires:
    #   - has_cargo (unlock + dependency + any compatible vehicle at tier 1)
    #   - previous tier's event item (ensures in-order completion)
    #   - at least `min(N, 4)` copies of a compatible vehicle progressive item
    #   - at tier >= utilities_required_tier: at least one locked utility unlocked
    #
    # The matching event location gets the same rule so the event only fires
    # once the tier is actually reachable.

    # Determine which utility items are locked for this slot.
    utility_option_keys = [
        ("Bridges",      "lock_bridges"),
        ("Tunnels",      "lock_tunnels"),
        ("Canals",       "lock_canals"),
        ("Terraforming", "lock_terraforming"),
    ]
    locked_utilities: List[str] = [
        item for item, opt_key in utility_option_keys
        if getattr(world.options, opt_key).value
    ]
    utilities_required_tier = int(world.options.utilities_required_tier.value)

    for mission in get_tiered_missions(world):
        cargo = mission["cargo"]
        tier = mission["tier"]
        loc_name = mission["location"]
        event_loc_name = f"Transport {cargo} {tier} Complete"

        # Vehicle tier required: tier 1 is already handled by has_cargo; cap at 4.
        vtier = min(tier, 4)

        if tier == 1:
            if locked_utilities and tier >= utilities_required_tier:
                rule = lambda state, c=cargo, lu=locked_utilities: (
                    has_cargo(state, player, c)
                    and has_any_utility(state, player, lu)
                )
            else:
                rule = lambda state, c=cargo: has_cargo(state, player, c)
        else:
            prev_event_item = f"Transport {cargo} {tier - 1} Done"
            if locked_utilities and tier >= utilities_required_tier:
                rule = lambda state, c=cargo, vt=vtier, prev=prev_event_item, lu=locked_utilities: (
                    has_cargo(state, player, c)
                    and state.has(prev, player)
                    and has_vehicle_tier_for_cargo(state, player, c, vt)
                    and has_any_utility(state, player, lu)
                )
            else:
                rule = lambda state, c=cargo, vt=vtier, prev=prev_event_item: (
                    has_cargo(state, player, c)
                    and state.has(prev, player)
                    and has_vehicle_tier_for_cargo(state, player, c, vt)
                )

        multiworld.get_location(loc_name, player).access_rule = rule
        multiworld.get_location(event_loc_name, player).access_rule = rule

    # ── CARGO-BY-VEHICLE MISSION RULES ───────────────────────────────────────────
    # Each "Transport <Cargo> by <Vehicle>" location requires:
    #   - has_cargo(cargo)  — cargo unlocked + industry chain deps + any vehicle tier 1
    #   - has_vehicle_type(vehicle_key)  — that specific vehicle type unlocked
    for mission in get_cargo_vehicle_missions(world):
        cargo   = mission["cargo"]
        vkey    = mission["vehicle_key"]
        loc_name = mission["location"]
        rule = lambda state, c=cargo, v=vkey: (
            has_cargo(state, player, c)
            and has_vehicle_type(state, player, v)
        )
        multiworld.get_location(loc_name, player).access_rule = rule

    world.multiworld.completion_condition[player] = lambda state: state.has_all(("Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods", "Grain", "Wood", "Iron Ore", "Steel", "Valuables"), world.player)
