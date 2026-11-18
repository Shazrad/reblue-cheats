<h1 align="center">
  <img width="1480" height="662" alt="Untitled-1" src="https://github.com/user-attachments/assets/1779fdfd-bc3a-416d-8b6c-38874d8eae93" />
  <a href="https://discord.gg/92MCyYDpuY">
    <img src="https://img.shields.io/badge/Discord-Join%20Server-5865F2?logo=discord&logoColor=white" alt="Discord">
  </a>
</h1>

> [!IMPORTANT]
> re:Blue is an unofficial project and contains NO ASSETS from the original game.
> This project is useless unless you have a legal backup of the ISO images from the Global release (US/EU, Asia/JP support coming soon).


# re:Blue

re:Blue rebuilds Blue Dragon as a native application through static recompilation, translating the original code into something your machine runs directly rather than emulating a console around it. That opens the door to things an emulator cannot reach: higher frame rates, modern resolutions, and real mod support.

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [How to Install](#how-to-install)
- [Features](#features)
- [FAQ](#faq)
- [Building](#building)
- [Credits](#credits)
- [License](#license)

## Hardware Requirements

Requires all three retail Blue Dragon discs or their disc images. Steam Deck is supported. 64-bit ARM processors are supported on Linux and macOS. Windows is x86-64 only. 

### Minimum

- OS: Windows 10 version 1909 or later, Ubuntu 24.04 / Fedora 40 / SteamOS 3.6 or later, or macOS 13.3 Ventura or later
- Processor: Intel Core i5-4460 3.2 GHz 4 Core or AMD Ryzen 3 1200 or Apple M1, or equivalent
- Memory: 8 GB RAM
- GPU: Nvidia GTX 1050 Ti or AMD RX 570, or equivalent performance & VRAM. DirectX 12 with Shader Model 6.0, or Vulkan 1.2, or Metal
- Storage: 15 GB available space

### Recommended

- OS: Windows 11, SteamOS 3.6, or macOS 14 Sonoma or later
- Processor: AMD Ryzen 5 5600X or Intel Core i5-12400 or Apple M2, or equivalent performance, 6 physical cores minimum
- Memory: 16 GB RAM
- GPU: Nvidia RTX 2060 or AMD RX 5700, or equivalent performance & VRAM. 8 GB VRAM for 4K with MSAA
- Storage: 15 GB available space

## How to Install

[Download latest release for your platform](https://github.com/zolaware/reblue/releases/latest) or [build yourself](#building)

1. Blue Dragon shipped on three DVDs, and you will need a disc image of each one from your own copy of the game.

2. Run the executable. A setup wizard will guide you through the rest. You will be asked to point it at each of the three disc images in turn, and it will check each one before letting you continue. Once you pick where to install, the program copies itself there and restarts from that location, so you can delete the folder you extracted the zip into.

3. Pick a graphics quality preset. The wizard copies the game files out of the discs, and you are done. You may also install DLC from this installer or from the main menu under the config menu

The wizard only needs to run once. If something later goes missing from your install, launching with `--repair` reopens it on your existing install and copies back only what it needs.

## Features

Everything below is new to re:Blue. All of it is configurable in game, from the title screen or the camp menu.

### Graphics

- Resolutions up to 4K, windowed or fullscreen, on whichever monitor you pick
- Aspect ratios 16:9, 4:3, 16:10, 21:9, 32:9, plus auto and stretch
- Four quality presets, Low through Ultra
- MSAA up to 8x or SSAA up to 4x
- Anisotropic filtering
- Shadow quality and draw distance
- Depth of field adjustment
- Unlocked FPS with optional caps and VSync

### Quality of Life

- Unlocked frame rate, with optional caps at 30, 60, 90, or 120
- Save from the camp menu anywhere instead of only at save points
- Field of view adjustment, 45 through 120 degrees
- Skip the in-game tutorial pages
- Full area map on the world map screen, with zoom, floor switching, and a legend
- Optional map markers for the hidden items, chests, and barriers a floor still has, plus per-floor counts, carried onto the field compass
- The field HUD can fade out once you stop pressing anything, or stay off entirely
- Achievement list viewable in game, with eight new re:Blue achievements alongside the original ones
- Master volume control
- Separate center, rear, and subwoofer levels for 5.1/7.1 tuning
- Fully native keyboard and mouse support with cursor and look modes supported by mouse
- Every controller button rebindable to a key, with mouse sensitivity and cursor opacity of your own
- Menus take the mouse directly: hover a row to move the cursor, click to confirm, wheel to scroll
- Custom input based icons/glyphs for hud elements, following the device you last used or pinned to Xbox, PlayStation, Switch, or Steam Deck
- UI language and voice language chosen separately


### Cheats

A full Cheats page in the config menu, reachable from the title screen or the camp menu. Every setting is live — no restart, no save editing — and each one persists per profile.

**In Battle**

- Battle Speed: Off / 2x / 3x / 4x. Everything in a battle runs faster (animations, timers, enemy turns) and drops back to normal as soon as the battle ends. One game tick runs per rendered frame at most, so Nx needs at least 30×N fps
- Invincibility: the party is topped back up continuously, so ordinary fights stop being a threat. A single hit big enough to kill from full HP will still land, and anyone already knocked out stays down
- Infinite MP: cast freely; MP refills as fast as you spend it
- Status Immunity: poison, sleep, petrify and the rest simply stop landing, and anything already on your party is cleared
- One-Hit Kill: enemies are held at 1 HP, so the next hit that connects finishes them. Battles still resolve normally, so you keep the EXP, gold and drops
- Perfect Charge: every charge gauge counts as released on its red zone, wherever you let go, so even a quick tap gets the full red-zone bonus. The game applies its own bonus; the gauge simply sits on the red zone

**Power** — Off / 2x / 5x / 10x

- Attack, Magic Attack, Defence, Magic Defence and Agility: each multiplied independently, so you can turn up exactly what you want
- Stat Bonus: Off / +999 / +9999 bonus to max HP, max MP and every combat stat, for when a multiplier isn't blunt enough. It goes through the game's own stat formula and caps and is never written to your save, so switching it off takes it away again

Multipliers and the stat bonus take effect from the next battle or equipment change.

**Progression**

- EXP per Fight: Off / 10x / 50x / 100x. Applied inside the game's own battle result, so the results screen shows the boosted EXP and levels arrive through the normal level-up
- SP per Fight: Off / 10x / 50x / 100x, applied the same way: shown on the results screen, ranks earned through the normal rank-up
- Gold per Fight: Off / 2x / 3x / 5x, shown on the results screen. Only the battle payout is scaled; shops, chests and events pay what they always did
- Medals per Fight: Off / 2x / 3x / 5x, for the ruins-only currency
- Infinite Gold: held at 99,999,999
- Infinite Medals: held at 9,999
- Unlock All Classes: all nine jobs open on every party member, reserves included
- Raise Party Level: choose +1 / +10 / Max, then confirm. Every party member levels up through the game's own level-up, stat gains included
- Raise Class Ranks: choose +1 / +10 / Max, then confirm. Every unlocked class of every party member ranks up through the game's own rank-up

**Items**

- Item Spawner: a window with a category dropdown (Heal, Usable, Spellbooks, Arm, Finger, Ear, Neck, Chest, Valuables), name search and item list showing how many you hold. Pick any item, choose a quantity (1-99) and Give, or double-click it. Give All Listed adds everything the list currently shows, so "All categories" with no search gives one of every item. Use mouse and keyboard there; Esc closes it
- Infinite Items: every stack you carry stays full, so nothing is spent by using or selling it. Key items (Valuables) are left alone so story checks still see the count they expect

Gives add to what you already hold rather than replacing it, stop at 99 per stack, and skip the unused entries that show up in game as Worthless Junk.

**Encyclopedia**

- Achievements 100%: awards the entire catalogue, all 51
- Reset Achievements: locks them all again, if you'd rather earn them

**Debug**

- Console: press `` ` `` to open it, then `game_cheat_diag on` to log a line whenever a cheat acts. Off by default. Every cheat also has a console command
  — `game_cheats` lists the current state of all of them

### Mods and DLC

- Built-in mod manager
- Official DLC is supported

### Platforms and Languages

- Windows on DX12 (Vulkan support is available if built from source)
- Linux AMD64 and ARM64, including the Steam Deck and other handhelds
- macOS ARM64 (AMD64 support is available if built from source)
- Custom menus in English, French, German, Italian, and Spanish

## FAQ

### Where is my save data and configuration stored?

Everything lives under the folder you installed to:

- Saves and settings: `profiles\default\`
- Your configuration file: `profiles\default\reblue.toml`
- Game files copied from your discs: `game\`
- Mods: `mods\`

### I want to update the game. Will I lose my save data?

No. Copy a newer build over your existing installation and your saves, settings, and mods are left alone. You do not need to reinstall or point the wizard at your discs again.

### How do I install mods?

Use the mod manager in the config menu. It accepts a mod folder or a zip file and puts everything in the right place for you

### Can I keep more than one set of saves?

Yes. Each profile is its own folder under `profiles\`, holding that profile's saves, settings, achievements, and DLC toggles. Launch with `--profile <name>` to pick one, and anything but `default` starts out fresh.

## Building

re:Blue builds with CMake and vcpkg against the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk).

```sh
cmake --preset win-amd64-release       # or linux-amd64-release
cmake --build --preset win-amd64-release
```

Presets cover `win-amd64`, `win-vk`, `linux-amd64`, `linux-arm64`, `mac-amd64`, and `mac-arm64`, each in Debug, Release, and RelWithDebInfo. A `win-amd64` preset builds both the DX12 executable (`reblue.exe`) and the Vulkan one (`reblue_vk.exe`), and a `win-vk` one builds the Vulkan executable alone. As with running the game, building requires the files from your own copy of Blue Dragon.

### Preparing `assets/`

The build needs two things from your installed game in `assets/`: `default.xex`, and the game's 152 Xenos shaders (`.vso`/`.pso`). The shaders are stored zlib-compressed in `pack/!necessity.ipk`, and the build compiles them into the executable. If they are missing, the build still succeeds but the game renders a black screen, and the log fills with `Shader cache miss`.

```sh
cp <install_root>/game/default.xex assets/
python3 tools/extract_shaders.py "<install_root>/game/pack/!necessity.ipk" assets
```

On Linux, `<install_root>` is usually `~/.config/reblue`. Then configure with the SDK on the prefix path, run codegen once, then reconfigure and build:

```sh
cmake --preset linux-amd64-release -DCMAKE_PREFIX_PATH=/path/to/rexglue-sdk/linux-amd64
cmake --build --preset linux-amd64-release --target reblue_codegen
cmake --preset linux-amd64-release -DCMAKE_PREFIX_PATH=/path/to/rexglue-sdk/linux-amd64
cmake --build --preset linux-amd64-release
```

## Credits

### Cheats

This is an unofficial build of re:Blue with an in-game Cheats page. Please report problems with it here, not to the re:Blue team.

- **[oshata](https://github.com/oshata/reblue-cheats)** wrote the original Cheats page this build is based on.
- **HimoShifu** made the Blue Dragon Cheat Engine table that documented the money, medal and inventory layout.
- Everything else is the work of the re:Blue team and contributors listed below.

### re:Blue

Huge thanks to everyone who has put time into this. re:Blue would not be where it is without you.

### re:Blue Development Team

- **[crack](https://github.com/tomcl7)** project lead and developer

- **[rcold](https://github.com/RC0ld)** developer and has done an absurd amount for this project. A lot of re:Blue looks the way it does because of him.

### Playtesting and Support

- **[infernozotza](https://github.com/Zotza)** - Playtester 
- **baus.98** - Playtester
- **[wolfaeterni](https://github.com/Zolawolf)** - Playtester and French Translations 
- **[griever666.](https://github.com/grv666)** - Playtester
- **[fungus](https://github.com/fungoid-creature)** - Playtester
- **[graine25](https://github.com/Graine25)** - macOS and Linux Development Support
- **[zhyxeryz](https://github.com/Zhyxeryz)** - Playtester and German Translations
- **[Azar42](https://github.com/Azar42)** - Playtesting
- **[ZolaKluke](https://github.com/ZolaKluke)** - Playtester
- **[emersed](https://github.com/RaphyEmersed)** - Playtester
- **[mrcmunir](https://github.com/mrcmunir)** - Spanish Translations
- **[mystixor](https://github.com/mystixor)** - German Translations
- **[toby](https://github.com/TbyDtch)** - Graphic Design

### Special Thanks

- The **[ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)** team, for the toolchain this project is built on.

- The **[hedge-dev](https://github.com/hedge-dev)** team, for [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) and for blazing the trail for Xbox 360 recompilations with [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp).

- The wider **Xbox 360 emulation scene**, and the [Xenia](https://github.com/xenia-project/xenia) project in particular. A lot of the hardest problems were solved long before this project started.

## License

See [LICENSE](LICENSE).
