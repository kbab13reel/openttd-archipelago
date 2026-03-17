# OpenTTD Cargolock

An [Archipelago](https://archipelago.gg) multiworld integration for **OpenTTD 15.2** with cargo-gated progression, progressive vehicle tiers, mission-based checks, and multiplayer-aware slot behavior.

This repository began as a fork of solida1987's OpenTTD Archipelago implementation and evolved into a separate gameplay direction.

World identifier: **OpenTTD Cargolock**

> Status: Beta. Core gameplay is stable and actively iterated.

---

## Features

- Cargo lock progression: your company cannot transport cargo that is not unlocked.
- Progressive vehicle tiers: train, road, aircraft, and ship unlocks are progressive instead of one-item-per-vehicle.
- Static mission location set with repeatable gameplay structure.
- Optional shop with configurable slot count, tiers, and costs.
- Multiplayer support:
  - Coop mode: multiple people in one company using one AP slot.
  - Multi-company mode: multiple companies mapped to different AP slots.
- Universal Tracker yaml-less compatible: tracker regeneration can run from slot data without a player YAML.

---

## Install

1. Build or download the patched OpenTTD binary from this project.
2. Place the APWorld package in Archipelago custom worlds as `openttd_cargolock.apworld`.
3. Restart Archipelago after adding or updating the package.
4. Verify only one installed world registers as OpenTTD Cargolock.

If you previously used an older OpenTTD APWorld package, disable or remove it to avoid duplicate game registration.

---

## YAML Setup

Use this game identifier in your player file:

```yaml
name: YourName
game: OpenTTD Cargolock

OpenTTD Cargolock:
  starting_vehicle_type: random
  mission_count: 300
  enable_shop: true
  map_size_x: 256
  map_size_y: 256
```

Notes:
- Use `OpenTTD Cargolock` exactly as shown.
- This world is UT yaml-less compatible, but normal Archipelago generation still supports player YAML files as usual.

---

## Multiplayer Host Order

1. Host connects Archipelago from the OpenTTD main menu.
2. Host starts the AP map (new generation) or loads an existing AP save.
3. After the AP map is live, start OpenTTD multiplayer from that running game.
4. Other players join the OpenTTD server and use the intended company/slot mapping.

Keep slot to company assignments consistent for the life of a save.

---

## Universal Tracker

OpenTTD Cargolock supports Universal Tracker yaml-less regeneration through slot data.

This means tracker-side world reconstruction does not require a local player YAML for this world.

---

## Building from Source

### Requirements

- Windows 10/11
- Visual Studio 2022 with C++ workload
- vcpkg
- CMake 3.21+

### Build

```powershell
git clone https://github.com/kbab13reel/openttd-archipelago
cd openttd-archipelago

vcpkg install

cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

---

## Known Limitations

| Issue | Severity | Notes |
|-------|----------|-------|
| WebSocket compression | Low | Archipelago may warn, connection remains usable. |
| Multi-company edge cases | Medium | Slot/company mapping mistakes can desync expected behavior. |

---

## License

This project is a fork of [OpenTTD](https://github.com/OpenTTD/OpenTTD) and is licensed under GPL v2.

The APWorld package is licensed under MIT.

See [COPYING.md](COPYING.md) for full GPL v2 text.

---

## Credits

- OpenTTD contributors for the base game.
- Archipelago team for the multiworld framework.
- Original OpenTTD Archipelago implementation by [solida1987](https://github.com/solida1987).
- OpenTTD Cargolock fork development by this repository maintainers and contributors.
