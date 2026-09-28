# Dead Space 3 Linear Mouse Fix v1.1

Lightweight ASI plugin for Dead Space 3 that removes the game's high-speed mouse input compression in normal camera movement (HIP) and ADS/RMB aiming while preserving low-speed sensitivity.

## Features

- Fixes high-speed mouse input compression in HIP and ADS/RMB.
- Preserves the game's low-speed mouse sensitivity.
- Leaves unrelated controller camera processing on the original game path.
- Does not modify `deadspace3.exe` on disk.
- Waits for and validates the protected retail game code before enabling the correction.
- Fails safely and leaves the correction inactive when the expected retail code cannot be validated.
- Writes one small status file next to `deadspace3.exe` for startup verification.

## Supported game

This release is for the official 32-bit (x86) Dead Space 3 retail executable distributed through EA App and Steam.

### Reference retail build

The retail executable used for compatibility validation has this identification data:

- Reference SHA-256: `BAA971A30B4D5B1F7BA132F29EE4DD8B51FC02371284C629D8D26EB850B62FEC`
- PE timestamp: `0x511E9327`
- `SizeOfImage`: `0x012D6000`
- Entry point RVA: `0x012D4830`

**Important:** the plugin does **not** calculate the executable SHA-256 at runtime. The SHA-256 above is a reference checksum for identifying the validated retail build. At runtime the plugin instead requires the expected x86 PE layout, retail DirectInput import structure, deprotected runtime signatures, and the verified HIP/ADS call relationships to be present before activation.

Do not use or distribute a modified/cracked `deadspace3.exe` with this release. The game executable itself is never included in this package.

## Requirements

- Dead Space 3 for Windows, official retail Steam or EA App build.
- A compatible 32-bit ASI loader.

### Recommended loader: Ultimate ASI Loader

This release **does not include** an ASI loader.

Use the official Win32 `winmm.dll` build from the [Ultimate ASI Loader project](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases).

For the recommended setup, use the loader's `winmm.dll` file directly. You do not need to rename a `dinput8.dll` to `winmm.dll` for this setup.

Ultimate ASI Loader supports loading ASI files from the game root and from `scripts`, `plugins`, or `update`. See the loader's official documentation if you choose another supported proxy-DLL variant.

## Installation

Recommended layout:

```text
deadspace3.exe
winmm.dll
winmm.ini                 (optional loader configuration)
DS3_LinearMouseFix.ini
plugins\
    DS3_LinearMouseFix.asi
```

1. Install the official Win32 `winmm.dll` build of Ultimate ASI Loader next to `deadspace3.exe`.
2. Put `DS3_LinearMouseFix.asi` in `plugins\`.
3. Put `DS3_LinearMouseFix.ini` in the same directory as `deadspace3.exe`.
4. Launch the game normally.

The ASI can also be placed directly beside `deadspace3.exe` when the selected ASI loader supports loading plugins from the game root.

Do not replace, patch, or modify `deadspace3.exe`.

## Configuration

`DS3_LinearMouseFix.ini`:

```ini
[Mouse]
EnableLinearMouseFix=1
```

- `1` = correction enabled.
- `0` = correction disabled; no game hooks are installed.

The release configuration is enabled by default.

## How it works

Dead Space 3 applies two separate limits to mouse camera input.

First, a camera-input stage limits the magnitude of large mouse vectors. The plugin calls the original stage first and, only on the verified HIP or ADS/RMB mouse-look path with a fresh DirectInput mouse sample on the same thread, restores the original mouse vector when it exceeded the game's magnitude limit.

Second, the camera processor applies a per-axis limit after its internal history/interpolation stage. The plugin calls the original processor first so that the game's internal history and state are updated normally. It then reconstructs the validated pre-limit result and replaces only output that the original per-axis limit would have clipped.

Values that remain within the original limits are left unchanged.

## Controller behavior

Controller-only camera processing remains on the game's original path. The mouse correction requires a fresh non-zero DirectInput mouse sample associated with the same thread and verified mouse-look profile before it can affect a camera call.

Simultaneous physical mouse and controller input is an edge case and has not been exhaustively characterized.

## Frame-rate and RTSS notes

- Very high frame rates can cause undesirable mouse behavior in Dead Space 3 independently of this fix.
- 120 FPS has been tested successfully with the fix.
- RivaTuner Statistics Server (RTSS) can interfere with ASI loading during game startup. When using RTSS, launch the game first and enable RTSS afterwards, or use a driver-level frame-rate limiter.

## Status file / troubleshooting

After launch, the plugin creates:

```text
DS3_LinearMouseFix.status.txt
```

A normal successful initialization looks like:

```text
Dead Space 3 Linear Mouse Fix v1.1
STATUS: initializing
CONFIG EnableLinearMouseFix=1
BUILD: official retail EA/Steam x86 layout recognized
STATUS: waiting for runtime code initialization
STATUS: ACTIVE
PROFILE: HIP + ADS/RMB
```

A refusal means the correction was not activated. Leave the game executable untouched and check that:

- the game is the official retail Steam/EA App build;
- its identification data matches the supported retail reference above;
- the selected ASI loader is installed correctly;
- only one copy of the plugin is being loaded;
- RTSS or another startup overlay is not interfering with loader startup.

The reference SHA-256 is an identification check for the validated retail build; it is not computed by the plugin at runtime.

## Release contents

The release archive contains only the plugin, configuration, documentation, source, build command, license, changelog, and portable checksums.

It does not contain:

- `deadspace3.exe`
- a modified/cracked game executable
- an ASI loader binary
- log or capture files
- old build artifacts

## License

This project is released under the MIT License. See `LICENSE`.

## Disclaimer

Dead Space 3 and its related trademarks are property of their respective owners. This project is an unofficial community modification and is not affiliated with or endorsed by Electronic Arts or Visceral Games.
