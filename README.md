# Dead Space 3 Linear Mouse Fix

A lightweight ASI plugin that fixes Dead Space 3's severe negative mouse acceleration and high-speed input compression.

The fix works in both:

- Normal camera movement (HIP)
- Right Mouse Button aiming (ADS/RMB)

Fast mouse movements now produce nearly the same rotation per mouse count as slow movements, instead of being heavily compressed by the game.

## Features

- Fixes negative mouse acceleration
- Fixes high-speed mouse input compression
- Supports normal camera movement
- Supports ADS / RMB aiming
- Preserves low-speed mouse sensitivity
- Leaves unrelated controller input paths untouched
- Does not modify the game executable on disk
- Can be disabled through the INI file
- Lightweight ASI plugin

## Requirements

- Dead Space 3 for Windows
- A compatible ASI loader, such as Ultimate ASI Loader

## Installation

1. Install an ASI loader in the Dead Space 3 game directory.
2. Copy these files into the game directory:

   `DS3_LinearMouseFix.asi`  
   `DS3_LinearMouseFix.ini`

3. Launch the game.

The fix is enabled by default:

```ini
[Mouse]
EnableLinearMouseFix=1
