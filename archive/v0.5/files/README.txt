DS3_RawMouseProbe v0.5 - API Observer
=====================================

Purpose
-------
Diagnostic only. This version does NOT register its own Raw Input device and does
NOT subclass any Dead Space 3 window. It observes the process's existing Raw
Input path by intercepting RegisterRawInputDevices and GetRawInputData while
capturing the already-validated D820 processor in parallel.

API interception
----------------
1. Loaded-module IAT slots are observed/patched conservatively.
2. If the x86 system API exposes the classic safe hotpatch layout (5 bytes of
   padding plus "mov edi,edi"), v0.5 also installs a process-wide hotpatch.
3. If that layout is unavailable, v0.5 does NOT guess at instruction lengths;
   periodic IAT scanning remains the fallback.
4. An IAT slot already owned by another hook is not overwritten; it is counted
   as a conflict.

Raw Input records
-----------------
- RegisterRawInputDevices: caller module/RVA, thread, every RAWINPUTDEVICE entry,
  flags/target, result, and failure error.
- GetRawInputData: sizing passes (pData == NULL) are counted but never emitted as
  movement records. Actual RID_INPUT calls are recorded; mouse counts are marked
  valid only when a returned RAWINPUT buffer is readable and structurally valid.
- GAME_RAW_INPUT_CONFIRMED is emitted only after a valid, non-zero, relative mouse
  packet is returned through GetRawInputData.

D820
----
The runtime signature, PE/callsite fingerprint, prologue verification and
observation-only D820 hook are retained. No X/Y values are replaced.

Files produced by the game run
------------------------------
  DS3_RawMouseProbe.status.txt
  DS3_RawMouseProbe.bin

Decode
------
  python decode_ds3_rawmouse_probe_v05.py DS3_RawMouseProbe.bin

Known limitation
----------------
If both safe process-wide hotpatching is unavailable AND a component calls a
previously cached/dynamically-resolved API pointer that is not reached through a
patched import slot, that call can escape the IAT fallback. status.txt explicitly
reports whether each process-wide hotpatch was available and the decoder reports
IAT conflicts. Do not infer absence of Raw Input from a capture where both API
hotpatches are unavailable and no relevant IAT slot was captured.
