DS3 DirectInput + D820 Probe v0.6.1
===================================

Diagnostic only. No mouse/camera values are replaced.
No Raw Input device is registered. No WndProc is subclassed.
No COM object vptr is replaced.

Changes from v0.6:
- keeps the working deadspace3.exe IAT hook for DINPUT8!DirectInput8Create;
- records the original IDirectInput8 vtable and all 11 slots;
- patches only slot 3 (CreateDevice) in the original vtable, never the object vptr;
- leaves slot 4 (EnumDevices) untouched;
- mouse devices are tracked by self pointer;
- only selected slots in the original mouse-device vtable are patched;
- shared vtables are safe: non-mouse self pointers immediately chain to the recorded original;
- per-vtable originals and patch/conflict masks are recorded;
- D820 hook is unchanged from v0.6;
- GetRegisteredRawInputDevices remains passive monitoring only.

Output:
  DS3_DirectInputProbe.status.txt
  DS3_DirectInputProbe.bin

Decode:
  python decode_ds3_directinput_probe_v061.py DS3_DirectInputProbe.bin
