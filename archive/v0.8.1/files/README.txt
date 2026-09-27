DS3 Linear Mouse Fix - Experimental A/B v0.8.1
================================================

Purpose
-------
Experimental validation build only. It combines the already validated selective
AB9830 mouse-path bypass with a selective post-vanilla D820 unclamp.

It does NOT patch AB9830 globally and does NOT patch D820 globally.
D820 vanilla always runs first so its history/state updates remain untouched.

Install
-------
1. Disable older DS3_DirectInputProbe / DS3_RawMouseProbe / MousePipelineDebug ASIs.
2. Copy DS3_LinearMouseFix_Experimental.asi and DS3_LinearMouseFix.ini to the
   location your existing ASI loader uses.
3. Default is vanilla:

   [Mouse]
   EnableLinearMouseFix=0

4. Set EnableLinearMouseFix=1 only for the experimental B test.

Files produced
--------------
DS3_LinearMouseFix.status.txt
DS3_LinearMouseFix.bin

Selective AB9830 condition
--------------------------
Same verified v0.8 condition:
- camera parent return RVA 0x0033F3ED
- AB9830 return RVA 0x006BC599
- fresh non-zero DirectInput mouse delta
- same thread
- <= 5 ms freshness
- pre-AB9830 magnitude > 1

Selective D820 condition
------------------------
- D820 caller return RVA 0x0033F407
- EnableLinearMouseFix=1
- a per-thread mouse-tail latch was started by an ACTUAL AB9830 mouse bypass
- post-vanilla D820 model reconstruction is valid
- abs(preclamp_x)>limit OR abs(preclamp_y)>limit

If both reconstructed preclamp components are within the vanilla limit, the hook
does not write X/Y at all. The vanilla return values remain bit-for-bit untouched.

Tail latch
----------
Starts only when the AB9830 mouse bypass is actually applied.
While active, D820 can be selectively corrected for the history tail even after
the instantaneous mouse sample has fallen back below the AB9830 threshold.
It exits after 3 consecutive targeted D820 samples below the D820 limit or after
a 250 ms safety timeout.

Telemetry
---------
The binary log keeps:
- AB9830 bypass count
- D820 unclamp count
- D820 tail-correction count
- vanilla D820 output
- corrected D820 output
- reconstructed preclamp and scale

Use decode_ds3_linear_mouse_fix_v081.py on DS3_LinearMouseFix.bin.
