# Limitations and next steps

This page lists what the prototype does not do, and the work that would close each gap. [Back to README](../README.md).

## Known limits

- **Not flown.** This is a ground-based prototype.
- **UV index is not calibrated.** The firmware reports mV only and omits `uv_index` until two reference points are captured.
- **Magnetic change is an estimate.** The µT value is relative to a boot-time baseline and is not calibrated against a reference field. The 50 µT baseline and the thresholds are placeholders.
- **Simulated:** orbit, gamma, flare and the Phase 1 ground link. The dashboard labels them.
- **No radio.** Wi-Fi is used. LoRa is not implemented.
- **Power and mass.** The INA219 is not in the load path. Battery, charging and solar were not tested. Mass, envelope and fit were not measured.
- **Security.** The firmware can turn off certificate checks (`ALLOW_INSECURE_HTTPS_DEMO`). Fix this before any real use.
- **Dashboard storage.** The server keeps only the latest packet. Charts live in the browser and reset on reload.
- **Missing payload fields.** A packet without a payload field is labelled `SIM`, and the last value is carried forward. A stale synthetic UV value can keep a warning on screen.
- **Sensors.** The DHT11 can repeat values. The dashboard's stuck-sensor warning was removed for that reason.
- **Mechanical.** The wiring is loose and not secured. Vibration, thermal and orbit tests have not been done.
- **Firmware build.** The library versions were not recorded.

## Next steps

Ordered by priority, from the final report (section 10).

| Priority | Task | Done when |
|---|---|---|
| P1 | Fix the browser treatment of missing UV and carried-forward simulated values | No UV warning from missing data, and a partial-live label |
| P1 | Rehearse the live chain: sensor → Vercel → display | A sustained log with a new `seq` every ~2 s |
| P1 | Annotate the assembly images and document component locations | Dimensioned chassis, labelled wiring, clearance and fastener check |
| P2 | Validate the UV circuit in daylight against a trusted reference | Enough mV span and a repeatable calibration |
| P2 | Add real motion and health fields to the API and dashboard | Server allowlist, UI and ESP32 JSON updated |
| P2 | Test the INA219 with a safe known load and a protected current path | Voltage and current within expected tolerance |
| P2 | Verify the 1S battery, charger, regulator and solar wiring | Current, thermal and load tests, with no USB backfeed |
| P3 | Integrate LoRa after bench electrical and RF checks | Separate receiver logs, antenna and packet success |
| P3 | Finish securing the enclosure and weigh the complete system | Boards and harness secured, mass and envelope measured, powered enclosure test |
| P3 | Improve TLS and credential handling | CA validation, secret rotation, no plaintext secrets in the repository |
