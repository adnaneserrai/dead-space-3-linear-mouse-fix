Dead Space 3 DirectInput + D820 + AB9830 Probe v0.7
===================================================

DIAGNOSTIC ONLY. This build does not replace mouse/camera values.

What it observes:
- existing DirectInput8 mouse reads (same v0.6.1 implementation)
- D820 input/output (same v0.6.1 hook)
- AB9830 pre/post X/Y only on the verified camera path:
    camera function call return RVA 0x0033F3ED -> ABC510
    ABC510 direct AB9830 return RVA 0x006BC599
    then D820 return RVA 0x0033F407

AB9830 pre/post magnitudes are calculated OFFLINE by the decoder from the recorded
X/Y values so the game-thread hook does not execute an extra sqrt.

Files created next to deadspace3.exe:
- DS3_DirectInputProbe.status.txt
- DS3_DirectInputProbe.bin

Decode:
  python decode_ds3_directinput_probe_v07.py DS3_DirectInputProbe.bin

Useful outputs:
- *_get_state.csv
- *_ab9830.csv
- *_d820.csv
- *_pipeline.csv

No fix is applied. No Raw Input device is registered. No WndProc is subclassed.
The executable on disk is never modified.
