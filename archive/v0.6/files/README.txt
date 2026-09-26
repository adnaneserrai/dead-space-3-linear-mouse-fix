Dead Space 3 DirectInput + D820 Probe v0.6
==========================================

Diagnostic only. It does not alter mouse/camera values.

What it observes:
- deadspace3.exe's static DINPUT8.dll!DirectInput8Create import
- IDirectInput8::CreateDevice
- mouse IDirectInputDevice8 SetDataFormat / SetCooperativeLevel / Acquire / Unacquire
- mouse GetDeviceState and GetDeviceData
- D820 processor (same verified signature/callsites/prologue as prior probes)
- passive GetRegisteredRawInputDevices snapshots

What it does NOT do:
- no RegisterRawInputDevices call
- no Raw Input receiver
- no WndProc subclass
- no mouse/camera value replacement
- no on-disk deadspace3.exe modification

Output:
  DS3_DirectInputProbe.status.txt
  DS3_DirectInputProbe.bin

Decode:
  python decode_ds3_directinput_probe_v06.py DS3_DirectInputProbe.bin

First validation test:
- disable older experimental mouse/input ASIs
- run gameplay and move the mouse normally for 10-15 seconds
- stop moving for ~1 second and quit normally
- return status.txt + bin
