DS3_RawMouseProbe v0.5.1
========================

Diagnostic-only x86 ASI for the tested Dead Space 3 executable.

What changed from v0.5:
- fixes RAWINPUT mouse validation: x86 minimum is 16-byte RAWINPUTHEADER + 24-byte RAWMOUSE = 40 bytes;
- compile-time ABI assertions verify the relevant sizes and offsets;
- observes GetRawInputBuffer in addition to RegisterRawInputDevices and GetRawInputData;
- GetRawInputBuffer use is logged even when no mouse packet can be parsed;
- on WOW64, buffered parsing uses the documented 8-byte block alignment and 24-byte mouse payload offset;
- preserves caller LastError around all observer-side pre-call inspection.

The probe does NOT:
- register a Raw Input device;
- create/subclass a WndProc;
- replace mouse X/Y or camera values;
- modify deadspace3.exe on disk;
- touch EA/network/DRM code.

Outputs:
  DS3_RawMouseProbe.status.txt
  DS3_RawMouseProbe.bin

Decoder:
  python decode_ds3_rawmouse_probe_v051.py DS3_RawMouseProbe.bin
