# WRSR Industrial Tycoon

**Workers optional. Industry essential.**

A mod for **Workers & Resources: Soviet Republic** that lets you play it like Transport Tycoon: build industries, connect them by road, rail, sea and air, and earn money by exporting, without housing or managing a population to staff your buildings.

**Experimental · Windows x64 · WRSR 1.1.1.9 only**

[What it does](#what-it-does) · [Install](#install) · [Recommended game settings](#recommended-game-settings) · [How to play](#how-to-play) · [Plugin settings](#plugin-settings) · [Technical guide](guide.md)

## What it does

Buildings that normally need workers behave as if they were fully staffed, and construction sites get virtual workers. No citizens are created.

Everything else stays as in the base game. You still deliver every input and construction material, buy vehicles, supply power and fuel, and make room for the output. Nothing is built instantly and no resources appear for free.

The mod is two independent plugins for [TesmioLoader](https://github.com/MaxLegend/TesmioLoader), shipped together in one package. Either can be turned off in the launcher:

| Plugin | What it does |
|---|---|
| `industrial_tycoon.dll` | Buildings **work** without staff. |
| `industrial_construction.dll` | Construction sites **build** without workers (realistic mode). |

### Works without staff

- **Every factory and every mine**, including DLC and Workshop ones: food, alcohol and crops, the miscellaneous section, metallurgy, chemistry, electronics, the uranium chain, the oil refinery, woodcutting posts, gravel quarries, and coal, iron, bauxite and uranium mines.
- **Every power plant.**
- **Vehicle, train, ship and aircraft factories, and every scrapyard.**
- **Hand-worked fields** and **container packing and unpacking facilities**.
- **Fire stations**: trucks leave and put out fires without firefighters.
- **Rail construction offices**: their vehicles build track and refuel without staff.
- **Vehicle repair stations.**
- **Airport control towers.**
- **Construction offices** take concrete and asphalt from concrete and asphalt plants without staff.
- With **waste management** on: incinerators, waste separation, recycling and the other waste factories.
- With **water management** on: water wells and water and sewage treatment plants.

### Builds without workers (realistic mode)

Storages and farms, the whole electrical network, construction and demolition offices, roads, bridges, tunnels and road buildings, vehicle repair stations, fire stations, the ship section, the rail section, cargo and storage buildings, factory connections and cableways, conveyors and pipelines, decorations and monuments, every industry, airports (runways, taxiways, stands, heliports, control tower), customs houses and the foreign pipeline connection. With waste or water management on, their buildings and pipes too. With building wear on, construction offices also renovate these buildings without workers.

### Not covered

- **Citizen services and housing:** shops, schools, hospitals, police, hotels, attractions, universities, churches and the like.
- **Heating.**
- **Passenger transport:** bus, trolleybus, tram, metro and airport passenger terminals, tram tracks.

Train passenger stations and passenger harbors can be built; the other passenger stations stay excluded.

### Mods

It should work with most mods. Buildings are recognized by their type, not by their name, so Workshop buildings of a covered type (factories, mines, storages, power plants and so on) work without workers too. The exception is other TesmioLoader plugins that change the same game code, which may conflict.

## Install

### Requirements

- **Workers & Resources: Soviet Republic 1.1.1.9** (Steam, `SOVIET64.exe`). Other versions are refused.
- **Windows x64.**
- The DLCs for any DLC buildings you use.

### Steps

1. Download the `wrsr-industrial-tycoon` ZIP from the latest release on the [releases page](https://github.com/paladin2005/WRSR-Industrial-Tycoon/releases). The "Source code" ZIP is not an installer.
2. Close the game.
3. Open your game folder, the one that contains `SOVIET64.exe`.
4. Extract the package and copy its `tesmioloader` folder into the game folder.
5. Run `tesmioloader\build\tesmiolauncher.exe` with Steam open. If it does not find your game, click **Browse** and select your `SOVIET64.exe`.
6. Check that it detects version **1.1.1.9**, tick both **Industrial Tycoon** plugins, and launch.

You should end up with:

```text
SovietRepublic/
├── SOVIET64.exe
└── tesmioloader/
    └── build/
        ├── tesmiolauncher.exe
        ├── tesmioloader.dll
        ├── tesmioloader.ini
        └── plugins/
            ├── industrial_tycoon.dll
            ├── industrial_tycoon.ini
            ├── industrial_construction.dll
            └── industrial_construction.ini
```

Always start the game through `tesmiolauncher.exe`; starting it from Steam runs it without the mod.

**Updating:** close the game, keep a copy of your two `.ini` files, copy the new `.dll` files over the old ones, and restore your settings.

**Uninstalling:** untick the plugins in the launcher, or start the game from Steam. Nothing is changed in your saves; buildings simply need workers again.

## Recommended game settings

Start a **new game**. The mod does not remove existing citizens.

| Setting | Recommendation | Why |
|---|---|---|
| **Realistic mode** | On | The construction plugin is for it. With it off, buildings are paid with money and only the industry plugin matters. |
| **Starting buildings** | Use the free ones | The free construction office, depot, gas station and storages let you start without building anything with workers. |
| **Electricity** | On | Supported. Power is part of the challenge. |
| **Fires** | On | Fire stations work without firefighters. |
| **Research** | Off | Universities need citizens and are not covered. |
| **Heating** | Off | Not covered. |
| **Waste management** | Optional | Supported: waste buildings build, and waste factories work without staff. |
| **Water and sewage** | Optional | Supported: wells and treatment plants work without staff. |
| **Vehicle wear and repairs** | Optional | Repair stations work without staff. |
| **Building wear** | Optional | Construction offices renovate worn buildings with virtual workers. They still need the materials and machinery of each renovation. |

## How to play

### Start

In realistic mode, place the free starting buildings, buy vehicles, and import construction materials at the border. Then build your network.

### Construction sites

A site only advances when it has all of these:

- an assigned construction office that can deliver its materials;
- the machinery its current phase needs;
- a worker maximum above 0. With 50 or more, it gets the full default virtual workforce.

Rail sites need a rail construction office with a rail construction vehicle.

### Industries

Connect each industry, supply its inputs and power, and move its output to another industry or to customs to sell it. Example: import crude oil → refinery → fuel and bitumen → sell at customs.

### Good to know

- **Worker counters stay at 0, and "No workers!" warnings may remain.** That is normal. Check the production percentage and the goods moving instead.
- **Concrete and asphalt plants as office sources:** assign them to the construction office as sources. The plant still needs its inputs (cement and gravel, or bitumen and gravel).
- **Factory connection crossings:** construction vehicles reach them only through a forklift station connected to a road. If a crossing says "not assigned to a construction office", link its network to the road with a forklift station.
- **Mines with vehicles:** gravel quarries and bauxite mines still need their working vehicles (excavators) assigned, as in the base game.
- **Real workers:** factories, mines, power plants and the other production buildings always run as fully staffed. Construction sites, renovations, fields, container facilities, control towers, water wells and repair stations only get virtual staff when nobody is there, so real workers keep their normal effect.

## Plugin settings

The settings are the two `.ini` files in `tesmioloader\build\plugins\`. Edit them with the game closed. A missing option uses its default.

`industrial_tycoon.ini`:

| Option | Default | Meaning |
|---|---|---|
| `enabled` | `1` | `0` turns the whole plugin off. |
| `fire_service` | `1` | Fire trucks without firefighters. |
| `virtual_firefighters` | `4` | Crew a truck uses when its own value is unusable (1 to 20). |
| `container_facility` | `1` | Container packing and unpacking without staff. |
| `fields` | `1` | Hand-worked fields without staff. |
| `control_tower` | `1` | Airport control towers without engineers. |
| `water_well` | `1` | Water wells without staff. |
| `repair_station` | `1` | Vehicle repair stations without staff. |
| `plant_sources` | `1` | Construction offices use concrete and asphalt plants without staff. |

`industrial_construction.ini`:

| Option | Default | Meaning |
|---|---|---|
| `enabled` | `1` | `0` turns the whole plugin off. |
| `virtual_workers` | `50` | Virtual workers per site, never more than the site's worker maximum. Must be 1 to 500, or the plugin does not load. |
| `renovation` | `1` | With building wear on, renovations get virtual workers too. |
| `rail_construction` | `1` | Rail office vehicles build and refuel without staff. |
| `virtual_track_crew` | `20` | Crew a rail vehicle uses when its own value is unusable (1 to 200). |

Industries, mines and power plants have no switch; they are always on while the plugin is enabled.

## Credits and license

Built on [TesmioLoader](https://github.com/MaxLegend/TesmioLoader) by MaxLegend, with [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw). Implementation notes and build instructions are in the [technical guide](guide.md).

Licensed under [GPL-3.0-or-later](https://github.com/paladin2005/WRSR-Industrial-Tycoon/blob/main/COPYING). The game and its assets are not included. This is an independent community project.
