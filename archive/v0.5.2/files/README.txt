DS3_RawMouseProbe v0.5.2

Diagnostic-only fix for v0.5.1 initialization failure.
Place DS3_RawMouseProbe.asi where your ASI loader loads plugins.

Expected files after successful initialization:
  DS3_RawMouseProbe.status.txt
  DS3_RawMouseProbe.bin

The binary log format is still v5.1, so decode_ds3_rawmouse_probe_v051.py remains compatible.

If initialization still fails, status.txt now prints one or more MISSING_API lines identifying the exact unresolved API.
