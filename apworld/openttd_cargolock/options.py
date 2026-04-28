from dataclasses import dataclass
from Options import (
    Choice, Range, Toggle, PerGameCommonOptions,
    OptionGroup, ItemDict
)


# ═══════════════════════════════════════════════════════════════
#  RANDOMIZER OPTIONS
# ═══════════════════════════════════════════════════════════════

class StartingVehicleType(Choice):
    """Which vehicle type you start with."""
    display_name = "Starting Vehicle Type"
    option_any = 0
    option_train = 1
    option_road_vehicle = 2
    option_aircraft = 3
    option_ship = 4
    default = 0


class StartingCargoType(Choice):
    """Which cargo type you start with.
    Cannot start with Goods or Steel as they are dependent on other cargo types."""
    display_name = "Starting Cargo Type"
    option_any = 0
    option_passengers = 1
    option_mail = 2
    option_coal = 3
    option_oil = 4
    option_livestock = 5
    option_grain = 6
    option_wood = 7
    option_iron_ore = 8
    option_valuables = 9
    default = 0


class StartingCashBonus(Toggle):
    """Give a starting cash bonus equal to one full starting loan amount.
    Off: No bonus.
    On:  Add one loan worth of cash at session start."""
    display_name = "Starting Cash Bonus"
    default = 0


# ═══════════════════════════════════════════════════════════════
#  INFRASTRUCTURE LOCKS (Gameplay Options)
# ═══════════════════════════════════════════════════════════════

class LockBridges(Toggle):
    """Lock bridge construction behind the 'Bridges' AP item.
    When on, players cannot build any bridge until 'Bridges' is received."""
    display_name = "Lock Bridges"
    default = 0


class LockTunnels(Toggle):
    """Lock tunnel construction behind the 'Tunnels' AP item.
    When on, players cannot build any tunnel until 'Tunnels' is received."""
    display_name = "Lock Tunnels"
    default = 0


class LockCanals(Toggle):
    """Lock canal/aqueduct construction behind the 'Canals' AP item. NOT RECOMMENDED WITH SHIP STARTING VEHICLE TYPE.
    When on, players cannot build canals or water bridges until 'Canals' is received."""
    display_name = "Lock Canals"
    default = 0


class LockTerraforming(Toggle):
    """Lock terraforming (raise/lower land) behind the 'Terraforming' AP item. NOT RECOMMENDED FOR NEW PLAYERS.
    When on, players cannot terraform land until 'Terraforming' is received."""
    display_name = "Lock Terraforming"
    default = 0

class FillerWeights(ItemDict):
    """Controls how often each filler item appears in the item pool.
    Increase a value to make that filler more common, set to 0 to disable it.
    Available fillers: Cash Injection, Choo chooo!"""
    display_name = "Filler Weights"
    valid_keys = ["Cash Injection", "Choo chooo!"]
    default = {"Cash Injection": 10, "Choo chooo!": 10}


# ═══════════════════════════════════════════════════════════════
#  MISSION OPTIONS
# ═══════════════════════════════════════════════════════════════

class CargoVehicleMissionCount(Range):
    """How many 'Transport <Cargo> by <Vehicle>' missions to generate.
    0  = none.  37 = all valid cargo/vehicle combinations.
    Any value in between randomly selects that many from the 37 possibilities.
    Each mission requires the cargo item AND the matching vehicle type to be in logic."""
    display_name = "Cargo-Vehicle Mission Count"
    range_start = 0
    range_end = 37
    default = 0


class GlobalTieredMissionCount(Range):
    """Number of progressive transport tiers generated for each cargo type.
    Tier 1 requires 1 000 units, tier 2 requires 5 000, tier 3 requires 10 000, tier 4 requires 50 000, etc.
    Per-cargo options override this global setting."""
    display_name = "Global Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 1


class PassengersTieredMissionCount(Range):
    """Tiered mission count for Passengers. Overrides global if > 0."""
    display_name = "Passengers Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0  # 0 = use global


class MailTieredMissionCount(Range):
    """Tiered mission count for Mail. Overrides global if > 0."""
    display_name = "Mail Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class ValuablesTieredMissionCount(Range):
    """Tiered mission count for Valuables. Overrides global if > 0."""
    display_name = "Valuables Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class CoalTieredMissionCount(Range):
    """Tiered mission count for Coal. Overrides global if > 0."""
    display_name = "Coal Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class OilTieredMissionCount(Range):
    """Tiered mission count for Oil. Overrides global if > 0."""
    display_name = "Oil Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class LivestockTieredMissionCount(Range):
    """Tiered mission count for Livestock. Overrides global if > 0."""
    display_name = "Livestock Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class GrainTieredMissionCount(Range):
    """Tiered mission count for Grain. Overrides global if > 0."""
    display_name = "Grain Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class WoodTieredMissionCount(Range):
    """Tiered mission count for Wood. Overrides global if > 0."""
    display_name = "Wood Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class IronOreTieredMissionCount(Range):
    """Tiered mission count for Iron Ore. Overrides global if > 0."""
    display_name = "Iron Ore Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class SteelTieredMissionCount(Range):
    """Tiered mission count for Steel. Overrides global if > 0."""
    display_name = "Steel Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class GoodsTieredMissionCount(Range):
    """Tiered mission count for Goods. Overrides global if > 0."""
    display_name = "Goods Tiered Mission Count"
    range_start = 0
    range_end = 10
    default = 0


class UtilitiesRequiredTier(Range):
    """Tiered cargo missions at or above this tier require all locked utilities
    (Bridges, Tunnels, Canals, Terraforming) to be unlocked in logic.
    Only has effect when at least one infrastructure lock option is enabled.
    Default: 2 (i.e. tier-2 missions and above need all utility items)."""
    display_name = "Utilities Required Tier"
    range_start = 1
    range_end = 10
    default = 2


# ═══════════════════════════════════════════════════════════════
#  SHOP OPTIONS
# ═══════════════════════════════════════════════════════════════

class EnableShop(Toggle):
    """Whether the AP shop is enabled for this slot."""
    display_name = "Enable Shop"
    default = 1


class ShopSlots(Range):
    """Total number of AP shop locations to generate."""
    display_name = "Shop Slots"
    range_start = 1
    range_end = 100
    default = 20


class ShotTiers(Range):
    """How many visibility tiers the AP shop is split into.
    Tier 1 is visible from the start; each Progressive Shop Upgrade reveals one more tier."""
    display_name = "Shop Tiers"
    range_start = 1
    range_end = 100
    default = 5


class ShopCostMin(Range):
    """Minimum cost for random shop item pricing."""
    display_name = "Shop Cost Minimum"
    range_start = 1000
    range_end = 2000000
    default = 50000


class ShopCostMax(Range):
    """Maximum cost for random shop item pricing.
    Should be >= Shop Cost Minimum. Default 1,000,000."""
    display_name = "Shop Cost Maximum"
    range_start = 1000
    range_end = 10000000
    default = 1000000


class RevealShopItems(Toggle):
    """Whether the AP shop list reveals actual item rewards.
    Off: shop entries stay blind (Shop 1, Shop 2, ...).
    On:  shop entries show the real item each location unlocks."""
    display_name = "Reveal Shop Items"
    default = 1


# ═══════════════════════════════════════════════════════════════
#  WORLD GENERATION
# ═══════════════════════════════════════════════════════════════

class StartYear(Range):
    """The starting year for the game world. Default is 1960."""
    display_name = "Start Year"
    range_start = 1
    range_end   = 5_000_000
    default     = 1960


class MapSizeX(Choice):
    """Width of the generated map."""
    display_name = "Map Width"
    option_64   = 0
    option_128  = 1
    option_256  = 2
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    option_4096 = 6
    default = 2  # 256 (OpenTTD default)

    @property
    def map_bits(self) -> int:
        return self.value + 6


class MapSizeY(Choice):
    """Height of the generated map."""
    display_name = "Map Height"
    option_64   = 0
    option_128  = 1
    option_256  = 2
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    option_4096 = 6
    default = 2  # 256 (OpenTTD default)

    @property
    def map_bits(self) -> int:
        return self.value + 6


class LandGenerator(Choice):
    """Which terrain generator to use."""
    display_name        = "Land Generator"
    option_original     = 0
    option_terragenesis = 1
    default = 1


class IndustryDensity(Choice):
    """Number of industries generated at game start."""
    display_name     = "Industry Density"
    option_fund_only = 0
    option_minimal   = 1
    option_very_low  = 2
    option_low       = 3
    option_normal    = 4
    option_high      = 5
    default = 4


class TerrainType(Choice):
    """Base terrain shape in map generation."""
    display_name           = "Terrain Type"
    option_very_flat       = 0
    option_flat            = 1
    option_hilly           = 2
    option_mountainous     = 3
    option_alpinist        = 4
    default = 1  # Flat (OpenTTD default)


class VarietyDistribution(Choice):
    """Landscape variety distribution."""
    display_name           = "Variety Distribution"
    option_none            = 0
    option_very_low        = 1
    option_low             = 2
    option_medium          = 3
    option_high            = 4
    option_very_high       = 5
    default = 0  # None (OpenTTD default)


class Smoothness(Choice):
    """Roughness/smoothness preset for terrain generator."""
    display_name           = "Smoothness"
    option_very_smooth     = 0
    option_smooth          = 1
    option_rough           = 2
    option_very_rough      = 3
    default = 1  # Smooth (OpenTTD default)


class Rivers(Choice):
    """Amount of rivers generated."""
    display_name           = "Rivers"
    option_none            = 0
    option_few             = 1
    option_medium          = 2
    option_many            = 3
    default = 2  # Medium (OpenTTD default)


class TownNames(Choice):
    """Town name style."""
    display_name = "Town Names"
    option_english_original   = 0
    option_french             = 1
    option_german             = 2
    option_english_additional = 3
    option_latin_american     = 4
    option_silly              = 5
    option_swedish            = 6
    option_dutch              = 7
    option_finnish            = 8
    option_polish             = 9
    option_slovak             = 10
    option_norwegian          = 11
    option_hungarian          = 12
    option_austrian           = 13
    option_romanian           = 14
    option_czech              = 15
    option_swiss              = 16
    option_danish             = 17
    option_turkish            = 18
    option_italian            = 19
    option_catalan            = 20
    default = 0  # English (Original)

class NumberOfTowns(Choice):
    """Initial number of towns."""
    display_name = "Number of Towns"
    option_very_low = 0
    option_low      = 1
    option_normal   = 2
    option_high     = 3
    default = 2  # Normal (OpenTTD default)


class SeaLevel(Choice):
    """Sea/lake amount preset."""
    display_name = "Sea Level"
    option_very_low = 0
    option_low      = 1
    option_medium   = 2
    option_high     = 3
    default = 0  # Very Low (OpenTTD default)


@dataclass
class OpenTTDOptions(PerGameCommonOptions):
    # Gameplay Options
    starting_vehicle_type:           StartingVehicleType
    starting_cargo_type:             StartingCargoType
    starting_cash_bonus:             StartingCashBonus
    lock_bridges:                    LockBridges
    lock_tunnels:                    LockTunnels
    lock_canals:                     LockCanals
    lock_terraforming:               LockTerraforming
    filler_weights:                  FillerWeights

    # Mission Options
    cargo_vehicle_mission_count:     CargoVehicleMissionCount
    global_tiered_mission_count:     GlobalTieredMissionCount
    passengers_tiered_mission_count: PassengersTieredMissionCount
    mail_tiered_mission_count:       MailTieredMissionCount
    valuables_tiered_mission_count:  ValuablesTieredMissionCount
    coal_tiered_mission_count:       CoalTieredMissionCount
    oil_tiered_mission_count:        OilTieredMissionCount
    livestock_tiered_mission_count:  LivestockTieredMissionCount
    grain_tiered_mission_count:      GrainTieredMissionCount
    wood_tiered_mission_count:       WoodTieredMissionCount
    iron_ore_tiered_mission_count:   IronOreTieredMissionCount
    steel_tiered_mission_count:      SteelTieredMissionCount
    goods_tiered_mission_count:      GoodsTieredMissionCount
    utilities_required_tier:         UtilitiesRequiredTier

    # Shop Options
    enable_shop:                     EnableShop
    shop_slots:                      ShopSlots
    shop_tiers:                      ShotTiers
    shop_cost_min:                   ShopCostMin
    shop_cost_max:                   ShopCostMax
    reveal_shop_items:               RevealShopItems

    # World Generation
    start_year:                      StartYear
    map_size_x:                      MapSizeX
    map_size_y:                      MapSizeY
    land_generator:                  LandGenerator
    terrain_type:                    TerrainType
    variety_distribution:            VarietyDistribution
    smoothness:                      Smoothness
    rivers:                          Rivers
    town_names:                      TownNames
    number_of_towns:                 NumberOfTowns
    sea_level:                       SeaLevel
    industry_density:                IndustryDensity

# ═══════════════════════════════════════════════════════════════
#  OPTION GROUPS — defines the categories in the Options Creator
# ═══════════════════════════════════════════════════════════════

openttd_option_groups = [
    OptionGroup("Gameplay Options", [
        StartingVehicleType,
        StartingCargoType,
        StartingCashBonus,
        LockBridges,
        LockTunnels,
        LockCanals,
        LockTerraforming,
        FillerWeights,
    ]),
    OptionGroup("Mission Options", [
        CargoVehicleMissionCount,
        GlobalTieredMissionCount,
        PassengersTieredMissionCount,
        MailTieredMissionCount,
        ValuablesTieredMissionCount,
        CoalTieredMissionCount,
        OilTieredMissionCount,
        LivestockTieredMissionCount,
        GrainTieredMissionCount,
        WoodTieredMissionCount,
        IronOreTieredMissionCount,
        SteelTieredMissionCount,
        GoodsTieredMissionCount,
        UtilitiesRequiredTier,
    ]),
    OptionGroup("Shop Options", [
        EnableShop,
        ShopSlots,
        ShotTiers,
        ShopCostMin,
        ShopCostMax,
        RevealShopItems,
    ]),
    OptionGroup("World Generation", [
        StartYear,
        MapSizeX,
        MapSizeY,
        LandGenerator,
        TerrainType,
        VarietyDistribution,
        Smoothness,
        Rivers,
        TownNames,
        NumberOfTowns,
        SeaLevel,
        IndustryDensity,
    ]),
]