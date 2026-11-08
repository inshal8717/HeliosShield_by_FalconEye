# Mission

This page states the problem, the objectives, the scope, and the evidence for each objective. [Back to README](../README.md).

## Problem

Low-cost satellites need to detect environmental changes, keep usable records, and alert ground operators when a configured limit is exceeded. Most small platforms give the operator little of this.

## Solution

Helios Shield is an educational demonstration of the measurement-to-warning chain. It is not a space-weather forecasting instrument. It cannot infer solar flares or gamma-ray flux from its sensors.

The payload choice follows from the problem:
- **UV sensing** demonstrates the radiation-related sensing chain.
- **A 3-axis magnetometer** independently shows changes in the local magnetic field.
- **A motion sensor** (MPU6050) supports motion and orientation experiments.
- **A temperature and humidity sensor** (DHT11) adds environmental context.
- **A microSD card** keeps the readings when the wireless link is down.

## Operating concept

On the bench, the ESP32 reads its sensors every 2 seconds, appends each reading to the microSD card, and posts it over Wi-Fi to a cloud API. A browser dashboard reads the latest packet, applies alert rules, and shows the result to the operator.

For a flight version, the concept assumes a nominal 96-minute low Earth orbit and a notional 30-day lifetime. Neither has been checked by orbital analysis or a power-endurance test.

## Objectives and evidence

Status values: **Verified** (observed on hardware), **Partial** (some evidence, with a stated gap), **Simulated** (software only).

| Objective | How it was checked | Criterion | Status |
|---|---|---|---|
| Capture payload data | UV analog voltage, 3-axis magnetic counts, temperature, humidity and motion data read on the bench | Stable, non-null records | Verified (UV in mV only) |
| Detect local disturbances | Magnetic baseline at boot, `field_delta_uT`, and dashboard alert rules | Threshold response is reproducible | Partial. Thresholds are placeholders, and the µT scale is uncalibrated |
| Preserve telemetry | JSON lines and raw CSV appended to microSD, confirmed on the serial monitor | Readable retained records | Verified |
| Deliver data to ground | Authenticated HTTPS POST, server echoes the sequence number, browser GET | HTTP 200 and a matching new `seq` | Verified |
| Demonstrate hazards safely | Synthetic UV and magnetic alert packets, clearly marked `SIM` | Warnings without false sensor claims | Simulated |
| Fit CubeSat constraints | Nominal 10 cm print layout, exterior and interior placement images | Envelope, mass, restraint, cost and power validated | Partial. Layout shown. Mass, envelope and power not measured |

Test-by-test results are in [Verification](verification.md).

## Scope

**In scope**
- Sensor-to-dashboard data path on the bench
- Onboard logging
- Alert rules and operator display
- A printed 1U-style enclosure

**Out of scope for this prototype**
- Flight operation, orbit and radiation exposure
- A calibrated UV index
- Gamma and solar-flare measurement. These are software-injected scenarios only
- LoRa. Wi-Fi replaces it for the cloud demonstration
- Battery, charging and solar power
