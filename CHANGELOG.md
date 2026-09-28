# Changelog

## v1.1

- Added compatibility with the official protected Steam / EA App retail executable layout.
- Added runtime-aware initialization that waits for the protected game code to become available before enabling the correction.
- Retained the validated HIP and ADS/RMB correction mathematics from v1.0.
- Retained the fresh DirectInput mouse gate and profile-specific history/tail handling.
- Kept unrelated controller D820 callers on the original game path.
- Kept safe refusal when the expected retail structure, runtime signatures, or camera call relationships cannot be validated.
- Reduced startup reporting to a small status file.
- Clarified that the retail SHA-256 in the documentation is a reference checksum, not a runtime hash check.
- Kept Ultimate ASI Loader external; no loader or game executable is bundled.

## v1.0

- Initial lightweight HIP + ADS/RMB mouse correction release.
