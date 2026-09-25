# Dead Space 3 Linear Mouse Fix

A lightweight ASI plugin that fixes Dead Space 3's severe negative mouse acceleration and high-speed input compression.

The fix works in both:

- Normal camera movement (HIP)
- Right Mouse Button aiming (ADS/RMB)

Fast mouse movements now produce nearly the same rotation per mouse count as slow movements, instead of being heavily compressed by the game.

## Features

- Fixes severe negative mouse acceleration
- Fixes high-speed mouse input compression
- Supports normal camera movement (HIP)
- Supports ADS / RMB aiming
- Preserves low-speed mouse sensitivity
- Leaves unrelated controller input paths untouched
- Does not modify `deadspace3.exe` on disk
- Validates the expected game code before activating
- Can be disabled through the INI file
- Lightweight final build with diagnostic logging removed

## Requirements

- Dead Space 3 for Windows
- A compatible ASI loader, such as Ultimate ASI Loader

## Installation

1. Install a compatible ASI loader in the Dead Space 3 game directory.
2. Copy these files next to `deadspace3.exe`:

   - `DS3_LinearMouseFix.asi`
   - `DS3_LinearMouseFix.ini`

3. Launch the game.

The fix is enabled by default.

## Configuration

```ini
[Mouse]
EnableLinearMouseFix=1
```

Available values:

- `1` — enable the validated HIP + ADS mouse correction
- `0` — disable the fix completely; no hooks are installed

## Supported Game Version

The plugin targets the 32-bit (`x86`) Dead Space 3 executable.

Validated executable information:

- PE timestamp: `0x511E9327`
- Accepted image size: `0x012D4000` or `0x012D6000`
- Tested executable SHA-256:

```text
dcc26a3d0afc1af232cb8e6d8c9192ff0de9a43ab51990be6756dd2993a63ba2
```

The plugin also verifies runtime signatures and the expected HIP/ADS call relationships before enabling the fix.

If the expected game code cannot be verified, the correction is not activated.

## How It Works

Dead Space 3 applies two separate forms of high-speed input limiting to mouse camera movement.

### 1. Radial input clamp

Before the camera input processor, the game applies a radial magnitude clamp that limits large mouse input vectors.

The plugin preserves the original game function and only restores the unclamped mouse vector when:

- the call belongs to a verified HIP or ADS camera path;
- a fresh, non-zero DirectInput mouse delta was received;
- the mouse sample is on the same thread;
- the sample has not already been consumed;
- the input magnitude exceeds the game's original limit.

Normal low-speed mouse input is left unchanged.

### 2. D820 per-axis clamp

The game's camera input processor applies another clamp after its internal history/interpolation stage.

The original function is always executed first so that all internal game state and history updates remain untouched.

The plugin then reconstructs the validated post-processing result and only replaces the returned camera movement when the original per-axis limit would have clipped the mouse input.

Below the limit, the original game output is preserved.

## Camera Profiles

Only the two verified mouse-look paths are corrected.

### HIP / Normal Camera

```text
ABC510 parent return = 0x0033F3ED
D820 return          = 0x0033F407
```

### ADS / RMB Aiming

```text
ABC510 parent return = 0x00149F17
D820 return          = 0x00149F2E
```

Internal AB9830 return:

```text
0x006BC599
```

Other D820 callers remain untouched.

## Mouse Tail Handling

Fast mouse movement can remain above the game's internal clamp for a few frames because Dead Space 3 keeps a short input history.

The fix therefore keeps a mouse-specific latch active briefly after a validated high-speed mouse input.

Validated constants:

```text
Fresh mouse window:      5000 us
Tail safety timeout:   250000 us
Stable exit threshold:       3 samples
```

The latch is profile-specific:

```text
HIP mouse input -> HIP D820 path only
ADS mouse input -> ADS D820 path only
```

A HIP latch cannot modify ADS processing, and an ADS latch cannot modify HIP processing.

## Controller Behavior

Controller-only camera input does not open the mouse-specific correction latch and remains on the game's original processing path.

Simultaneous physical mouse movement and controller input has not been extensively tested and should be considered an edge case.

## Final Build

The final v1.0 release is derived directly from the validated v0.8.2 experimental implementation without changing the correction algorithm.

The final build removes development-only diagnostics, including:

- binary telemetry logging
- `DS3_LinearMouseFix.bin`
- writer thread and double buffering
- Raw Input registration monitoring
- periodic module monitoring
- per-frame diagnostic counters
- observation-only DirectInput hooks

Only the hooks required for the actual fix remain.

A small `DS3_LinearMouseFix.status.txt` file is written at startup so that activation can be verified.

A successful startup should contain:

```text
CONFIG EnableLinearMouseFix=1
ACTIVE: validated HIP + ADS linear mouse correction enabled.
```

## Release Integrity

### `DS3_LinearMouseFix.asi` v1.0

SHA-256:

```text
348d9781292a7f49a53788a38ef765481e6348767b137efeff3ca5d93a73885e
```

### Source code

SHA-256:

```text
ffd45873e2cc70bfa51e316de13e717ac6c7419b99418166165da60fe096ef43
```

## Building

The exact reproducible build command used for the release is included in:

```text
BUILD_COMMAND.txt
```

The release was built with reproducible build settings and two clean builds produced bit-identical binaries.

## License

This project is licensed under the MIT License.

See `LICENSE` for details.

## Disclaimer

This is an unofficial community modification and is not affiliated with or endorsed by Electronic Arts or Visceral Games.

Use it at your own risk.
