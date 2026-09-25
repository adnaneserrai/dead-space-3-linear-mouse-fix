Dead Space 3 Linear Mouse Fix v1.0
=================================

Final lightweight HIP + ADS build derived directly from the validated v0.8.2
experimental logic.

Supported target
----------------
- deadspace3.exe x86
- PE timestamp: 0x511E9327
- image size accepted: 0x012D4000 or 0x012D6000
- runtime signatures and exact HIP/ADS call links are verified before activation

Install
-------
Place these files beside deadspace3.exe (with the existing ASI loader):
  DS3_LinearMouseFix.asi
  DS3_LinearMouseFix.ini

INI
---
[Mouse]
EnableLinearMouseFix=1

1 = enable the validated HIP + ADS correction
0 = disable it; the final build installs no hooks

What the fix changes
--------------------
HIP profile:
  ABC510 parent return 0x0033F3ED
  D820 return          0x0033F407

ADS/RMB profile:
  ABC510 parent return 0x00149F17
  D820 return          0x00149F2E

AB9830:
- vanilla is always called first;
- only on a verified HIP/ADS camera path;
- only with a fresh, unconsumed, non-zero DirectInput mouse delta on the same thread;
- only when pre magnitude > 1;
- then the pre-AB9830 X/Y vector is restored.

D820:
- vanilla is always called first, preserving all history/internal state updates;
- only the matching HIP/ADS D820 caller can be corrected;
- correction requires the same-profile mouse-tail latch started by a real AB9830 mouse bypass;
- the exact v0.8.1/v0.8.2 validated post-state reconstruction is used;
- only when reconstructed preclamp X or Y exceeds the vanilla limit is returned X/Y replaced by preclamp*scale;
- below the limit, D820 output is left untouched bit-for-bit.

Tail latch constants (unchanged from v0.8.2):
- fresh mouse window: 5000 us
- safety timeout: 250000 us
- stable under-limit exit: 3 targeted D820 samples

Removed from the final build
----------------------------
- binary telemetry logger / DS3_LinearMouseFix.bin
- writer thread and double buffering
- Raw Input registration monitoring
- periodic module-monitor thread
- per-frame diagnostic counters
- observation-only DirectInput hooks (Acquire, Unacquire, GetDeviceData,
  SetCooperativeLevel)

Only a small DS3_LinearMouseFix.status.txt is written during startup.

Controller behavior
-------------------
Controller-only camera input does not start the mouse latch and is left vanilla.
As in the validated experiment, simultaneous physical mouse movement plus controller
input is an edge case because a real fresh mouse sample intentionally opens the
mouse-specific gate.
