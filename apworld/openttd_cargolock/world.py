from typing import Any
from BaseClasses import ItemClassification, Location, Region
from worlds.AutoWorld import World, WebWorld
from . import items, locations, options, rules

class OpenTTDWeb(WebWorld):
    """Web world for display and documentation."""
    theme = "ocean"
    option_groups = options.openttd_option_groups

class OpenTTDWorld(World):
    """
    OpenTTD is an open-source transport simulation game.
    Build transport networks using trains, road vehicles, aircraft, and ships.
    All vehicles and cargo types must be unlocked through Archipelago checks!
    """
    game = "OpenTTD Cargolock"
    web = OpenTTDWeb()
    
    options_dataclass = options.OpenTTDOptions
    options: options.OpenTTDOptions
    
    item_name_to_id = items.ITEM_NAME_TO_ID
    location_name_to_id = locations.LOCATION_NAME_TO_ID

    origin_region_name = "Overworld"

    topology_present = True
    ut_can_gen_without_yaml = True

    def create_regions(self) -> None:
        locations.create_all_locations(self)
        
    def set_rules(self) -> None:
        rules.set_all_rules(self)

    def create_items(self) -> None:
        items.create_all_items(self)

    def create_item(self, name: str) -> items.OpenTTDItem:
        return items.create_item_with_correct_classification(self, name)

    def get_filler_item_name(self) -> str:
        return items.get_random_filler_item_name(self)

    @classmethod
    def interpret_slot_data(cls, slot_data: dict[str, Any]) -> dict[str, Any]:
        """Reconstruct relevant options from slot_data for yaml-less trackers (e.g. Universal Tracker).
        Only options that affect the location pool or access rules are needed."""
        result = {
            "enable_shop": slot_data.get("enable_shop", 1),
            "shop_slots":  len(slot_data.get("shop_locations", [])),
            "shop_tiers": slot_data.get("shop_tiers", 5),
            "shop_cost_min": slot_data.get("shop_cost_min", 50000),
            "shop_cost_max": slot_data.get("shop_cost_max", 1000000),
            "global_tiered_mission_count": slot_data.get("global_tiered_mission_count", 3),
            "cargo_vehicle_mission_count": slot_data.get("cargo_vehicle_mission_count", 10),
            "lock_bridges":           slot_data.get("lock_bridges", 1),
            "lock_tunnels":           slot_data.get("lock_tunnels", 1),
            "lock_canals":            slot_data.get("lock_canals", 1),
            "lock_terraforming":      slot_data.get("lock_terraforming", 1),
            "utilities_required_tier": slot_data.get("utilities_required_tier", 2),
        }
        for cargo in locations.CARGO_TYPES:
            key = locations._cargo_option_key(cargo)
            result[key] = slot_data.get(key, 0)
        return result

    def fill_slot_data(self) -> dict[str, Any]:
        # Provide an explicit id->name map so clients can always resolve
        # ReceivedItems (including precollected items) by item ID.
        item_id_to_name = {
            str(item_id): item_name
            for item_name, item_id in self.item_name_to_id.items()
        }

        location_name_to_id = locations.get_slot_location_name_to_id(self)

        shop_locations = locations.get_slot_shop_locations(self)
        reveal_shop_items = bool(self.options.reveal_shop_items.value)
        if reveal_shop_items:
            for shop in shop_locations:
                location_name = shop.get("location")
                if not isinstance(location_name, str):
                    continue
                location = self.multiworld.get_location(location_name, self.player)
                if location.item is not None:
                    item_name = location.item.name
                    recipient = self.multiworld.player_name[location.item.player]
                    shop["name"] = f"{item_name} ({recipient})"
        shop_tiers = locations.get_shop_tier_count(self)

        return {
            "item_id_to_name": item_id_to_name,
            "location_name_to_id": location_name_to_id,
            "mission_count": len(locations.MISSION_DEFINITIONS),
            "missions": locations.get_slot_missions(self),
            "starting_vehicle_type": self.options.starting_vehicle_type.value,
            "starting_cargo_type": self.options.starting_cargo_type.value,
            "starting_cash_bonus": self.options.starting_cash_bonus.value,
            "enable_shop": self.options.enable_shop.value,
            "reveal_shop_items": self.options.reveal_shop_items.value,
            "shop_tiers": shop_tiers,
            "shop_locations": shop_locations,
            "shop_cost_min": self.options.shop_cost_min.value,
            "shop_cost_max": self.options.shop_cost_max.value,

            # Tiered cargo mission counts
            "global_tiered_mission_count": self.options.global_tiered_mission_count.value,
            "cargo_vehicle_mission_count": self.options.cargo_vehicle_mission_count.value,
            **{
                locations._cargo_option_key(cargo): locations._get_tiered_mission_count(self, cargo)
                for cargo in locations.CARGO_TYPES
            },

            # World generation settings (match OpenTTD map generation menu)
            "start_year": self.options.start_year.value,
            "map_x": self.options.map_size_x.map_bits,
            "map_y": self.options.map_size_y.map_bits,
            "landscape": 0,  # Temperate only
            "land_generator": self.options.land_generator.value,
            "terrain_type": self.options.terrain_type.value,
            "variety": self.options.variety_distribution.value,
            "tgen_smoothness": self.options.smoothness.value,
            "amount_of_rivers": self.options.rivers.value,
            "water_border_presets": 2,  # Infinite Water only
            "town_name": self.options.town_names.value,
            "number_towns": self.options.number_of_towns.value,
            "quantity_sea_lakes": self.options.sea_level.value,
            "industry_density": self.options.industry_density.value,

            # Infrastructure locks
            "lock_bridges":           self.options.lock_bridges.value,
            "lock_tunnels":           self.options.lock_tunnels.value,
            "lock_canals":            self.options.lock_canals.value,
            "lock_terraforming":      self.options.lock_terraforming.value,
            "utilities_required_tier": self.options.utilities_required_tier.value,
        }
