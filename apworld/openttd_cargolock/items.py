from __future__ import annotations
from typing import List, TYPE_CHECKING
from BaseClasses import Item, ItemClassification
if TYPE_CHECKING:
    from .world import OpenTTDWorld

DEFAULT_PROGRESSIVE_VEHICLE_TIERS = {
    "Progressive Trains": 5,
    "Progressive Road Vehicles": 4,
    "Progressive Aircrafts": 4,
    "Progressive Ships": 3,
    "Progressive Trams": 3,
}

CARGO_TYPES = [  # Temperate
        "Passengers", 
        "Mail", 
        "Coal", 
        "Oil",
        "Livestock", 
        "Goods", 
        "Grain", 
        "Wood",
        "Iron Ore", 
        "Steel", 
        "Valuables",
]

# 16 unlockable company colours.
COMPANY_COLOUR_ITEMS = [
    "Company Colour: Dark Blue",
    "Company Colour: Pale Green",
    "Company Colour: Pink",
    "Company Colour: Yellow",
    "Company Colour: Red",
    "Company Colour: Light Blue",
    "Company Colour: Green",
    "Company Colour: Dark Green",
    "Company Colour: Blue",
    "Company Colour: Cream",
    "Company Colour: Mauve",
    "Company Colour: Purple",
    "Company Colour: Orange",
    "Company Colour: Brown",
    "Company Colour: Grey",
    "Company Colour: White",
]

FILLER_ITEM_IDS = {
    "Cash Injection": 16,
    "Choo chooo!": 17,
    "Town Development Fund": 39,
    "Civic Honours": 40,
    "High Demand Period": 41,
    "Industry Boost": 42,
    "New CEO": 43,
}

FILLER_ITEMS: List[str] = list(FILLER_ITEM_IDS)

FILLER_DEFAULT_WEIGHTS = {
    "Cash Injection": 10,
    "Choo chooo!": 1,
    "Town Development Fund": 10,
    "Civic Honours": 10,
    "High Demand Period": 10,
    "Industry Boost": 10,
    "New CEO": 10,
}

# Trap items are registered here as they are implemented.
TRAP_ITEM_IDS = {
    "Town Roadworks": 44,
    "Recession": 45,
    "Industry Strike": 46,
    "Driver Strike": 47,
    "Breakdown Crisis": 48,
    "Company Scandal": 49,
    "Wildfire": 50,
    "Zeppelin Crash": 51,
    "Small UFO": 52,
    "Military Aircraft": 53,
    "Military Helicopter": 54,
    "Large UFO": 55,
    "Small Submarine": 56,
    "Large Submarine": 57,
    "Coal Mine Collapse": 58,
}

TRAP_ITEMS: List[str] = list(TRAP_ITEM_IDS)

TRAP_DEFAULT_WEIGHTS = {
    "Town Roadworks": 5,
    "Recession": 1,
    "Industry Strike": 1,
    "Driver Strike": 1,
    "Breakdown Crisis": 1,
    "Company Scandal": 5,
    "Wildfire": 5,
    "Zeppelin Crash": 5,
    "Small UFO": 5,
    "Military Aircraft": 5,
    "Military Helicopter": 5,
    "Large UFO": 5,
    "Small Submarine": 5,
    "Large Submarine": 5,
    "Coal Mine Collapse": 5,
}

ITEM_NAME_TO_ID = {
    "Progressive Trains": 1,
    "Progressive Road Vehicles": 2,
    "Progressive Aircrafts": 3,
    "Progressive Ships": 4,
    "Passengers": 5,
    "Mail": 6,
    "Coal": 7,
    "Oil": 8,
    "Livestock": 9,
    "Goods": 10,
    "Grain": 11,
    "Wood": 12,
    "Iron Ore": 13,
    "Steel": 14,
    "Valuables": 15,
    "Progressive Shop Upgrade": 18,
    "Progressive Trams": 59,
}

ITEM_NAME_TO_ID.update(FILLER_ITEM_IDS)
ITEM_NAME_TO_ID.update(TRAP_ITEM_IDS)

for i, colour_item in enumerate(COMPANY_COLOUR_ITEMS, start=19):
    ITEM_NAME_TO_ID[colour_item] = i

# ── Infrastructure / utility items ────────────────────────────────────────
UTILITY_ITEMS = ["Bridges", "Tunnels", "Canals", "Terraforming"]

for i, utility_item in enumerate(UTILITY_ITEMS, start=35):
    ITEM_NAME_TO_ID[utility_item] = i

DEFAULT_ITEM_CLASSIFICATION = {
    "Progressive Trains": ItemClassification.progression,
    "Progressive Road Vehicles": ItemClassification.progression,
    "Progressive Aircrafts": ItemClassification.progression,
    "Progressive Ships": ItemClassification.progression,
    "Progressive Trams": ItemClassification.progression,
    "Progressive Shop Upgrade": ItemClassification.progression,
    "Passengers": ItemClassification.progression | ItemClassification.useful,
    "Mail": ItemClassification.progression | ItemClassification.useful,
    "Coal": ItemClassification.progression | ItemClassification.useful,
    "Oil": ItemClassification.progression | ItemClassification.useful,
    "Livestock": ItemClassification.progression | ItemClassification.useful,
    "Goods": ItemClassification.progression | ItemClassification.useful,
    "Grain": ItemClassification.progression | ItemClassification.useful,
    "Wood": ItemClassification.progression | ItemClassification.useful,
    "Iron Ore": ItemClassification.progression | ItemClassification.useful,
    "Steel": ItemClassification.progression | ItemClassification.useful,
    "Valuables": ItemClassification.progression | ItemClassification.useful,
}

for filler_item in FILLER_ITEMS:
    DEFAULT_ITEM_CLASSIFICATION[filler_item] = ItemClassification.filler

for colour_item in COMPANY_COLOUR_ITEMS:
    DEFAULT_ITEM_CLASSIFICATION[colour_item] = ItemClassification.filler

for utility_item in UTILITY_ITEMS:
    DEFAULT_ITEM_CLASSIFICATION[utility_item] = ItemClassification.progression | ItemClassification.useful

for trap_item in TRAP_ITEMS:
    DEFAULT_ITEM_CLASSIFICATION[trap_item] = ItemClassification.trap

class OpenTTDItem(Item):
    game = "OpenTTD Cargolock"

def get_random_trap_item_name(world: OpenTTDWorld) -> str:
    weights_opt = getattr(world.options, "trap_weights", None)
    if weights_opt is not None:
        weights = {k: v for k, v in weights_opt.value.items() if k in TRAP_ITEMS and v > 0}
        if weights:
            names = list(weights.keys())
            wvals = [weights[n] for n in names]
            return world.random.choices(names, weights=wvals, k=1)[0]
    return world.random.choice(TRAP_ITEMS)

def get_random_filler_item_name(world: OpenTTDWorld) -> str:
    # If traps are configured, randomly replace this filler slot with a trap.
    trap_freq = getattr(world.options, "trap_frequency", None)
    if trap_freq is not None and TRAP_ITEMS and trap_freq.value > 0:
        if world.random.randrange(100) < trap_freq.value:
            return get_random_trap_item_name(world)
    weights_opt = getattr(world.options, "filler_weights", None)
    if weights_opt is not None:
        weights = {k: v for k, v in weights_opt.value.items() if k in FILLER_ITEMS and v > 0}
        if weights:
            names = list(weights.keys())
            wvals = [weights[n] for n in names]
            return world.random.choices(names, weights=wvals, k=1)[0]
    return world.random.choice(FILLER_ITEMS)

def create_item_with_correct_classification(world: OpenTTDWorld, name: str) -> OpenTTDItem:
    classification = DEFAULT_ITEM_CLASSIFICATION[name]
    return OpenTTDItem(name, classification, ITEM_NAME_TO_ID[name], world.player)

def create_all_items(world: OpenTTDWorld) -> None:
    # Work on local copies; module-level constants must stay immutable across worlds.
    progressive_counts = {
        "Progressive Trains": max(1, world.options.train_tier_count.value),
        "Progressive Road Vehicles": max(1, world.options.road_vehicle_tier_count.value),
        "Progressive Aircrafts": max(1, world.options.aircraft_tier_count.value),
        "Progressive Ships": max(1, world.options.ship_tier_count.value),
    }
    if world.options.enable_trams.value:
        progressive_counts["Progressive Trams"] = max(1, world.options.tram_tier_count.value)
    available_cargo_types = list(CARGO_TYPES)
    available_company_colours = list(COMPANY_COLOUR_ITEMS)

    # Aircraft starts should only precollect cargo types that aircraft can haul early.
    # (The reported softlock case was: Progressive Aircrafts + Coal.)
    compatible_starting_cargo = {
        "Progressive Trains": set(available_cargo_types),
        "Progressive Road Vehicles": set(available_cargo_types),
        "Progressive Ships": set(available_cargo_types),
        "Progressive Aircrafts": {"Passengers", "Mail", "Goods", "Valuables"},
        "Progressive Trams": set(available_cargo_types),
    }

    vehicle_start_choices = [
        "Progressive Trains",
        "Progressive Road Vehicles",
        "Progressive Aircrafts",
        "Progressive Ships",
    ]

    # world.options.starting_vehicle_type
    if world.options.starting_vehicle_type == 0:
        starting_vehicle_type = world.random.choice(vehicle_start_choices)
    elif world.options.starting_vehicle_type == 1:
        starting_vehicle_type = "Progressive Trains"
    elif world.options.starting_vehicle_type == 2:
        starting_vehicle_type = "Progressive Road Vehicles"
    elif world.options.starting_vehicle_type == 3:
        starting_vehicle_type = "Progressive Aircrafts"
    elif world.options.starting_vehicle_type == 4:
        starting_vehicle_type = "Progressive Ships"

    world.multiworld.push_precollected(world.create_item(starting_vehicle_type))
    # Remove one tier of the chosen vehicle type from the item pool, since it's precollected.
    progressive_counts[starting_vehicle_type] -= 1

    allowed_cargo = compatible_starting_cargo[starting_vehicle_type]

    # Build requested cargo from option first; compatibility is checked below.
    requested_starting_cargo_type = None

    # world.options.starting_cargo_type
    if world.options.starting_cargo_type == 0:
        random_pool = [c for c in available_cargo_types if c not in {"Goods", "Steel"}]
        requested_starting_cargo_type = world.random.choice(random_pool)
    elif world.options.starting_cargo_type == 1:
        requested_starting_cargo_type = "Passengers"
    elif world.options.starting_cargo_type == 2:    
        requested_starting_cargo_type = "Mail"
    elif world.options.starting_cargo_type == 3:
        requested_starting_cargo_type = "Coal"
    elif world.options.starting_cargo_type == 4:
        requested_starting_cargo_type = "Oil"
    elif world.options.starting_cargo_type == 5:
        requested_starting_cargo_type = "Livestock"
    elif world.options.starting_cargo_type == 6:
        requested_starting_cargo_type = "Grain"
    elif world.options.starting_cargo_type == 7:
        requested_starting_cargo_type = "Wood"
    elif world.options.starting_cargo_type == 8:
        requested_starting_cargo_type = "Iron Ore"
    elif world.options.starting_cargo_type == 9:
        requested_starting_cargo_type = "Valuables"

    if requested_starting_cargo_type in allowed_cargo:
        starting_cargo_type = requested_starting_cargo_type
    else:
        # Force a compatible fallback to avoid unwinnable starts.
        # Exclude Goods and Steel (secondary cargos requiring prerequisites).
        fallback_pool = [c for c in available_cargo_types if c in allowed_cargo and c not in {"Goods", "Steel"}]
        if not fallback_pool:
            fallback_pool = [c for c in available_cargo_types if c in allowed_cargo]
        starting_cargo_type = world.random.choice(fallback_pool)

    world.multiworld.push_precollected(world.create_item(starting_cargo_type))
    # Remove the chosen cargo type from the item pool, since it's precollected.
    available_cargo_types.remove(starting_cargo_type)

    # Start with exactly one company colour unlocked.
    starting_company_colour = world.random.choice(available_company_colours)
    world.multiworld.push_precollected(world.create_item(starting_company_colour))
    available_company_colours.remove(starting_company_colour)
    
    itempool: list[Item] = []
    for name, count in progressive_counts.items():
        for _ in range(1, count + 1):
            itempool.append(create_item_with_correct_classification(world, name))
    shop_upgrade_count = max(0, world.options.shop_tiers.value - 1) if world.options.enable_shop.value else 0
    for _ in range(shop_upgrade_count):
        itempool.append(create_item_with_correct_classification(world, "Progressive Shop Upgrade"))
    for name in available_cargo_types:
        itempool.append(create_item_with_correct_classification(world, name))
    for name in available_company_colours:
        itempool.append(create_item_with_correct_classification(world, name))

    # Add locked utility items (Bridges, Tunnels, Canals, Terraforming)
    utility_option_keys = {
        "Bridges":      "lock_bridges",
        "Tunnels":      "lock_tunnels",
        "Canals":       "lock_canals",
        "Terraforming": "lock_terraforming",
    }
    for item_name, opt_key in utility_option_keys.items():
        opt = getattr(world.options, opt_key, None)
        if opt is not None and opt.value:
            itempool.append(create_item_with_correct_classification(world, item_name))
    
    number_of_unfilled_locations = len(world.multiworld.get_unfilled_locations(world.player))
    needed_number_of_filler_items = number_of_unfilled_locations - len(itempool)
    itempool += [world.create_filler() for _ in range(needed_number_of_filler_items)]
    world.multiworld.itempool += itempool

    
# Flatten all vehicles into a single list
#ALL_VEHICLES: List[str] = []
#for tier in PROGRESSIVE_TRAINS:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_ROAD_VEHICLES:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_AIRCRAFT:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_SHIPS:
#    ALL_VEHICLES.extend(tier)