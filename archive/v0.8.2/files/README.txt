DS3 Linear Mouse Fix - Experimental A/B v0.8.2
================================================

This is still an experimental validation build with telemetry.
It extends the validated mouse-only selective correction to both:
  HIP     ABC510 return 0x0033F3ED -> D820 return 0x0033F407
  ADS/RMB ABC510 return 0x00149F17 -> D820 return 0x00149F2E

Install the ASI where your existing ASI loader loads it and place
DS3_LinearMouseFix.ini beside deadspace3.exe / the generated status files.

[Mouse]
EnableLinearMouseFix=0   vanilla behavior, telemetry only
EnableLinearMouseFix=1   selective HIP + ADS experimental correction

Do not use old DS3 mouse probes/fixes simultaneously for the validation run.
The build writes:
  DS3_LinearMouseFix.status.txt
  DS3_LinearMouseFix.bin

The final lightweight release is intentionally not this build; the large logger
is retained only for the last ADS/RMB validation.
