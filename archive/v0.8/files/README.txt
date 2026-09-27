DS3 DirectInput + D820 + AB9830 Experimental Bypass v0.8
=========================================================

Purpose
-------
Experimental A/B validation only. This is NOT the final mouse fix.

The module keeps the v0.7 DirectInput, targeted AB9830 and D820 telemetry.
It optionally restores the pre-AB9830 vector only when ALL of these are true:

  1. ABC510 is on the verified camera path whose parent return RVA is 0x0033F3ED.
  2. AB9830 is reached from the verified return RVA 0x006BC599.
  3. A new valid DirectInput mouse GetDeviceState sample was read on the same thread.
  4. That sample is non-zero and at most 5000 microseconds old.
  5. The fresh DirectInput sample has not already been consumed by another targeted AB9830 call.
  6. pre_x^2 + pre_y^2 > 1.
  7. [Mouse] EnableMouseAB9830Bypass=1.

Vanilla AB9830 is ALWAYS called first. When any condition above is false, its output
is left untouched. With the INI at its default 0, v0.8 is observation-only.

Files produced
--------------
  DS3_DirectInputProbe.status.txt
  DS3_DirectInputProbe.bin

Configuration
-------------
  DS3_DirectInputProbe.ini

  [Mouse]
  EnableMouseAB9830Bypass=0

Set to 1 only for the experimental B capture.

Telemetry
---------
The AB9830 records contain:
  pre X/Y
  vanilla post X/Y
  effective post X/Y (what D820 receives)
  fresh DirectInput X/Y
  fresh DirectInput generation and QPC age
  bypass enabled/applied flags

The decoder reports whether effective AB9830 output exactly matches D820 input.
