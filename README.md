# FalconEye Helios Shield

**Team:** FalconEye (SatForge 2026, National Space Academy)

**Mission name:** Helios Shield

**Mission:** UV and magnetic-disturbance monitoring with ground-control alerts

<p align="center">
  <img src="docs/images/intro.jpg" alt="Helios Shield prototype beside the live dashboard" width="360">
</p>

Small satellites carry little environmental monitoring, so their operators cannot see hazards such as UV and magnetic disturbance building up. Helios Shield is a low-cost 1U CubeSat engineering model. It measures UV and magnetic-field change, logs the readings on board, sends them to the ground over Wi-Fi, and warns the operator within seconds when a limit is crossed.

The repository holds the full chain from sensor to ground dashboard, working on the bench. It is not a flight unit, and it does not claim a calibrated UV index or a space-weather forecast.

## At a glance

| | |
|---|---|
| Form factor | 1U-style CubeSat engineering model, nominal 100 mm 3D-printed frame |
| Payloads | UV (UVM-30A, raw mV) and 3-axis magnetometer (GY-273, HMC5883L-compatible) |
| Support sensors | MPU6050 motion sensor, DHT11 temperature and humidity, INA219 power monitor (not in the load path) |
| Controller | ESP32-WROOM-32 |
| Onboard storage | microSD: JSON lines and raw CSV |
| Downlink | Wi-Fi, HTTPS POST every 2 s. LoRa is not implemented |
| Ground segment | Vercel serverless API and a browser dashboard |
| Power in tests | USB. Battery and solar were not tested |
| Status | Ground-based bench prototype. Not flown |

## How it works

```
Sensors (UV, magnetometer, temperature, motion)
        │
      ESP32 ──────────► microSD log (on board)
        │
   Wi-Fi, HTTPS POST every 2 s
        │
  Vercel API (/api/telemetry) ──► Upstash Redis (latest packet)
        │
  Dashboard (GET every 2 s) ──► gauges, alert levels, timeline
```

## Status in brief

| Verified on the bench | Simulated or modelled | Not done |
|---|---|---|
| Multi-sensor acquisition | Gamma and solar-flare inputs | Calibrated UV index |
| microSD logging | Orbit and globe on the dashboard | LoRa radio link |
| Wi-Fi uplink with HTTP 200 and matching `seq` | Phase 1 Tinkercad and Multisim models | Battery, charging and solar power |
| Live dashboard feed | Demo mode packets (tagged `SIM`) | Mass, envelope and vibration tests |

Details are in [Verification](#5-verification) and [Limitations](#8-limitations-and-next-steps).

## Quick start

1. Wire the ESP32 and sensors as in the [pin map](#pin-map).
2. Deploy the dashboard and set `INGEST_KEY` ([step 2](#2-deploy-the-ground-segment)).
3. Fill in the four values at the top of the sketch and flash it ([step 3](#3-configure-and-flash-the-firmware)).
4. Open the dashboard, set **Source** to **Live feed**, and press **Connect**.

To try the dashboard with no hardware, open `index.html` in a browser. It starts in Simulation mode. The last deployed build is at https://helioshield-iota.vercel.app.

## Contents

- [1. Mission](#1-mission)
- [2. System architecture](#2-system-architecture)
- [3. Subsystems](#3-subsystems)
- [4. Interfaces](#4-interfaces)
- [5. Verification](#5-verification)
- [6. Operations](#6-operations)
- [7. Build guide](#7-build-guide)
- [8. Limitations and next steps](#8-limitations-and-next-steps)
- [9. Glossary](#9-glossary)
- [Repository map](#repository-map)
- [References](#references)
- [Team](#team)

## 1. Mission

This section states the problem, the objectives, the scope, and the evidence for each objective.

### Problem

Low-cost satellites need to detect environmental changes, keep usable records, and alert ground operators when a configured limit is exceeded. Most small platforms give the operator little of this.

### Solution

Helios Shield is an educational demonstration of the measurement-to-warning chain. It is not a space-weather forecasting instrument. It cannot infer solar flares or gamma-ray flux from its sensors.

The payload choice follows from the problem:
- **UV sensing** demonstrates the radiation-related sensing chain.
- **A 3-axis magnetometer** independently shows changes in the local magnetic field.
- **A motion sensor** (MPU6050) supports motion and orientation experiments.
- **A temperature and humidity sensor** (DHT11) adds environmental context.
- **A microSD card** keeps the readings when the wireless link is down.

### Operating concept

On the bench, the ESP32 reads its sensors every 2 seconds, appends each reading to the microSD card, and posts it over Wi-Fi to a cloud API. A browser dashboard reads the latest packet, applies alert rules, and shows the result to the operator.

For a flight version, the concept assumes a nominal 96-minute low Earth orbit and a notional 30-day lifetime. Neither has been checked by orbital analysis or a power-endurance test.

### Objectives and evidence

Status values: **Verified** (observed on hardware), **Partial** (some evidence, with a stated gap), **Simulated** (software only).

| Objective | How it was checked | Criterion | Status |
|---|---|---|---|
| Capture payload data | UV analog voltage, 3-axis magnetic counts, temperature, humidity and motion data read on the bench | Stable, non-null records | Verified (UV in mV only) |
| Detect local disturbances | Magnetic baseline at boot, `field_delta_uT`, and dashboard alert rules | Threshold response is reproducible | Partial. Thresholds are placeholders, and the µT scale is uncalibrated |
| Preserve telemetry | JSON lines and raw CSV appended to microSD, confirmed on the serial monitor | Readable retained records | Verified |
| Deliver data to ground | Authenticated HTTPS POST, server echoes the sequence number, browser GET | HTTP 200 and a matching new `seq` | Verified |
| Demonstrate hazards safely | Synthetic UV and magnetic alert packets, clearly marked `SIM` | Warnings without false sensor claims | Simulated |
| Fit CubeSat constraints | Nominal 10 cm print layout, exterior and interior placement images | Envelope, mass, restraint, cost and power validated | Partial. Layout shown. Mass, envelope and power not measured |

Test-by-test results are in [Verification](#5-verification).

### Scope

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

## 2. System architecture

This section shows the subsystems and how data moves between them.

### Block diagram

```
 PAYLOADS                    ONBOARD COMPUTER                GROUND SEGMENT

 UV (analog) ───────┐
 Magnetometer (I²C)─┤
 Motion (I²C) ──────┼──►  ESP32-WROOM-32  ──► microSD log
 Temp/humidity ─────┤        │
 Power monitor (I²C)┘        │ Wi-Fi 2.4 GHz
                             │ HTTPS POST, every 2 s
                             ▼
                      Vercel API  ──► Upstash Redis (latest packet)
                             ▲
                             │ HTTP GET, every 2 s
                      Browser dashboard
                      (alert rules, gauges, timeline, CSV export)
```

### Subsystem table

| Subsystem | Parts | Role | Status |
|---|---|---|---|
| Payloads | UVM-30A UV module, GY-273 magnetometer | Measure UV and magnetic-field change | Working. UV in mV only |
| Support sensors | MPU6050, DHT11 | Motion and orientation, temperature and humidity | Working. Motion data is logged, not sent |
| Onboard computer and data handling | ESP32-WROOM-32, microSD module | Read sensors, build packets, log, uplink | Working |
| Communications | ESP32 Wi-Fi, HTTPS | Send each packet to the cloud API | Working. LoRa not implemented |
| Ground segment | Vercel function, Upstash Redis, browser dashboard | Store the latest packet, apply alert rules, display | Working |
| Power | USB in tests. INA219 on the I²C bus | Supply and monitor power | USB only. INA219 not in the load path. Battery and solar not tested |
| Structure | Three-part 3D-printed frame | Hold the electronics in a 1U-style envelope | Printed. Not secured or measured |

### Design choices

- **Wi-Fi in place of LoRa.** Wi-Fi was proven on the bench. The wired SoftwareSerial link in the Tinkercad model stood in for the radio in Phase 1.
- **UV as raw voltage.** The UV signal is too weak for the ESP32 ADC alone, so a bias network lifts it into the readable range. The firmware reports millivolts, not a UV index. See [UV subsystem](#uv-payload).
- **Alerts on the ground.** The onboard computer sends measurements. The dashboard decides alert levels, so thresholds change without re-flashing the firmware.
- **Missing data is left out.** A failed sensor is omitted from the packet. The firmware never sends 0 for a missing reading.

### More detail

- Per-subsystem description: [Subsystems](#3-subsystems)
- Pins, addresses and packet fields: [Interfaces](#4-interfaces)

## 3. Subsystems

Each section gives the function, parts, interface, test result and limits.

### UV payload

**Function.** Measure ultraviolet intensity.

**Parts.** UVM-30A UV module, a 10 kΩ series resistor, and a 100 kΩ resistor to 3.3 V.

**Interface.** The module output goes through the 10 kΩ resistor to **GPIO34** (ADC1, input only). The 100 kΩ resistor from GPIO34 to 3.3 V biases the junction. The firmware averages `analogReadMilliVolts` and reports the result as `uv_biased_mv`.

**Why the bias network.** The sensor output is tens to hundreds of millivolts, which was too weak for the ADC on its own. The bias lifts the junction into a readable range.

**Test result.** The biased junction read about 342–350 mV.

**Calibration.** The voltage is not a UV index. The firmware sends `uv_index` only after two reference points are captured. One low point is stored (about 345.6 mV at UV index 0). The high point was rejected because the signal span was under 8 mV, so `uv_index` is omitted. Procedure and rules: [Operations](#uv-calibration).

**Limits.** No validated UV index exists. Calibration needs a trusted reference instrument in sunlight, at two or more levels.

### Magnetometer payload

**Function.** Detect changes in the local magnetic field.

**Parts.** GY-273 module (HMC5883L-compatible).

**Interface.** I²C at address `0x1E`, SDA GPIO21, SCL GPIO22.

**How it reads.** At boot the firmware collects 24 samples as a baseline. It reports `field_delta_uT`, the change from that baseline, in µT. The dashboard averages the last 5 points before it applies thresholds.

**Module identification.** Some GY-273 boards carry a different chip than the label says. The firmware reads the ID bytes and uses the HMC5883L register sequence.

**Test result.** Detected at `0x1E`. Baseline collected, 24 of 24 samples.

**Limits.** The µT scale is an estimate. It is not calibrated against a reference field, and it shifts if the board rotates or its surroundings change. A changing reading shows sensitivity to nearby magnetic objects. It does not prove solar activity.

### Motion and environment sensors

**MPU6050 (accelerometer and gyroscope).** I²C at `0x68`. At start-up the firmware calibrates the gyroscope bias (200 of 200 samples accepted). Motion values are written to the raw CSV log. They are not part of the telemetry packet yet.

**DHT11 (temperature and humidity).** DATA on GPIO27. Readings during testing were about 23.8–24.5 °C and 73–77 % relative humidity. The DHT11 can return repeated values, so the dashboard no longer treats repeated values as a fault.

### Onboard computer and data handling

**Function.** Read sensors, build packets, log them, and send them.

**Parts.** ESP32-WROOM-32 DevKit and a microSD SPI module (CS GPIO5, SCK GPIO18, MISO GPIO19, MOSI GPIO23).

**Behaviour.**
- A packet is built every 2 seconds.
- Each packet is appended to `/falconeye_v41.jsonl`. Raw values go to `/falconeye_raw_v41.csv`.
- The I²C bus runs at 25 kHz with a 150 ms timeout. This was more reliable at start-up than the default speed.
- If Wi-Fi is down, logging continues and the upload is skipped.

**Test result.** Both files were appended on the microSD card, confirmed on the serial monitor.

**Limits.** The microSD module failed at 3.3 V. It runs from the 5 V USB supply. Its logic levels have not been checked against the module's ratings.

### Communications

**Function.** Carry each packet to the ground.

**Implementation.** The ESP32 connects to a 2.4 GHz Wi-Fi network and sends an HTTPS POST with an `x-api-key` header. The server returns `{"ok":true,"seq":N}`.

**Test result.** HTTP 200 with the sequence number echoed.

**Limits.**
- LoRa (SX1278) is not implemented. The Phase 1 [Tinkercad model](#tinkercad-sensor-and-ground-link-model) used a wired serial link as a stand-in.
- The firmware has a bench option that skips certificate checks (`ALLOW_INSECURE_HTTPS_DEMO`). Turn it off for real use.
- Reconnect attempts were slowed after Wi-Fi contention during testing. RSSI logging and offline buffering are not done.

### Ground segment

**Function.** Store the latest packet, apply alert rules, and show the result.

**Parts.**
- `api/telemetry.js`: a Vercel serverless function. POST (key required) stores a packet. GET returns the latest.
- Upstash Redis: stores the latest packet so every function instance sees it. If the store is unreachable the function returns HTTP 503 with a reason.
- Dashboard (`index.html`, `app.js`, `styles.css`, `land.js`): static, no build step.

**Dashboard behaviour.**
- Alert levels per channel (see [thresholds](#alert-thresholds)).
- **STALE** and link-lost handling when packets stop.
- **FAULT** for values outside their valid range.
- `SIM` or `LIVE` labelling. A packet with a missing payload field is labelled `SIM`, and the banner names the missing field.
- CSV export, presentation mode, and a globe with an illustrative orbit.

**Limits.** The server keeps only the latest packet. Charts live in the browser and reset on reload.

### Power

**In tests.** USB powers the ESP32 and sensors.

**INA219.** Present on the I²C bus at `0x40` and initialised, but its VIN+ and VIN− are not in the load path, so it measures no current.

**Multisim model.** A boost stage feeding an LM1117-3.3 regulator was simulated. See [Verification](#multisim-filter-and-power-stage-models).

**Limits.** Battery, charging, solar and protection circuits were not tested. Do not connect a battery or solar input to this system yet, and do not backfeed the USB supply.

### Structure

**Function.** Hold the electronics in a 1U-style envelope.

**Design.** A nominal 10 cm (100 mm) cube in three print groups. Files are in `hardware/3d-print/`:

| File | Contents |
|---|---|
| `CubeSat_1_of_3_Frame_and_BatteryHolder.stl` | Frame and battery holder |
| `CubeSat_2_of_3_Decks_and_Rods.stl` | Decks and rods |
| `CubeSat_3_of_3_Lid_Spacers_Clips.stl` | Lid, spacers and clips |

The layout was prepared for a Bambu Lab A1 mini. Print settings were not recorded with the files.

![Figure 1. Print layout for the three groups](docs/images/print_layout.png)

*Figure 1. Print layout for the three groups.*

![Figure 2. Enclosure views](docs/images/multiview.png)

*Figure 2. Enclosure views: isometric, front, top and back-quarter.*

**Assembly state.** The breadboard and jumper wires sit inside the open chassis. The wires are loose and have no strain relief. The panel is mounted outside, and its electrical connection is not shown.

![Figure 3. Open chassis](docs/images/open_chassis.png)

*Figure 3. Open chassis with the breadboard, sensors and jumper wires.*

**Limits.** Final envelope, mass and fit were not measured. The assembly is not secured or flight-ready.

## 4. Interfaces

This section is the interface reference: pins, bus addresses, the telemetry packet, and alert thresholds.

### Pin map

As-built wiring. This is what was tested.

| Part | Role | Connection (ESP32 pin) |
|---|---|---|
| ESP32-WROOM-32 DevKit | Controller, Wi-Fi (2.4 GHz) | USB power during tests |
| UVM-30A UV module | UV payload (analog) | OUT → 10 kΩ → **GPIO34** (ADC1). 100 kΩ from GPIO34 to 3.3 V |
| GY-273 (HMC5883L-compatible) | Magnetic-field payload | I²C: SDA **GPIO21**, SCL **GPIO22** |
| MPU6050 (GY-521) | Accelerometer and gyroscope | I²C: GPIO21 / GPIO22 |
| DHT11 | Temperature and humidity | DATA **GPIO27**, VCC 3V3, shared GND |
| INA219 | Power monitor | I²C. VIN+ and VIN− are **not** in the load path |
| MicroSD SPI module | Local logging | CS **GPIO5**, SCK **GPIO18**, MISO **GPIO19**, MOSI **GPIO23**. 5 V from USB |

Notes:
- GPIO34 is input-only on the ESP32. Use it only for the UV analog input.
- The original Phase 1 plan used different pins (for example microSD on GPIO13 and a KY-013 thermistor). Those are not used.

### I²C bus

| Device | Address |
|---|---|
| GY-273 magnetometer | `0x1E` |
| MPU6050 | `0x68` |
| INA219 | `0x40` |

Bus speed 25 kHz, timeout 150 ms.

### Telemetry packet

One JSON object per reading, sent every 2 seconds:

```json
{ "source": "LIVE", "seq": 14025, "field_delta_uT": 0.81, "temp_C": 24.1 }
```

| Field | Meaning |
|---|---|
| `source` | `"LIVE"` shows as live. Any other value, or no value, shows as `SIM` |
| `seq` | Sequence number. The dashboard skips repeats and shows **STALE** when packets stop |
| `uv_index` | UV index, 0–15. Omitted until calibrated |
| `field_delta_uT` | Magnetic-field change in µT, relative to a boot-time baseline (uncalibrated) |
| `temp_C` | Temperature in °C (DHT11) |
| `gamma_cpm`, `flare_index` | Optional simulated threat inputs |
| `device`, `rssi`, `uptime_s` | Optional device health: name, Wi-Fi signal (dBm), seconds since boot |

Rules:
- A failed or missing sensor is left out. The firmware never sends 0 for a missing reading.
- A packet with a missing payload field is shown as `SIM`, and the banner names the missing field.
- Accepted aliases: `uv`, `field_uT`, `dB`, `temp`, `cpm`, `flare`.

### HTTP API

| Method | Path | Auth | Result |
|---|---|---|---|
| POST | `/api/telemetry` | `x-api-key` header, must equal `INGEST_KEY` | `{"ok":true,"seq":N}` |
| GET | `/api/telemetry` | none | The latest stored packet |

If the store is unreachable the function returns HTTP 503 with a reason.

### Alert thresholds

A value outside its valid range is not used. The gauge and status strip show **FAULT**, and the timeline logs it.

| Channel | Valid range | Thresholds |
|---|---|---|
| UV index | 0 to 15 | WARNING at 8 |
| Field change | ±100 µT | WARNING above 5 µT, ALERT above 10 µT (5-point average) |
| Temperature | −40 to 85 °C | Band 0–40 °C |
| Gamma (simulated) | 0 to 1000 CPM | WARNING above 26.5, ALERT above 30.7 (baseline 18) |
| Flare index (simulated) | 0 to 100 | WARNING at 50, ALERT at 80 |

These are demonstrator thresholds. They are not validated space-weather alarms.

**Link lost.** If no new packet arrives for 10 seconds (or three poll intervals, if longer), the telemetry dims, a NO LINK note shows the packet age, and the timeline logs **Link lost** and later **Link restored**.

### Files written to the microSD card

| File | Contents |
|---|---|
| `/falconeye_v41.jsonl` | One JSON packet per line, as sent |
| `/falconeye_raw_v41.csv` | Raw values per cycle: UV mV, magnetometer counts, accelerometer and gyroscope values, temperature, humidity, and sensor status flags |

## 5. Verification

This section lists what was tested, what was only simulated, and which problems came up. Results are from the final report (3 November 2026). Each row is what was observed, not a target.

### Bench tests

| Test | Observed | Result |
|---|---|---|
| I²C enumeration | GY-273 at `0x1E`, MPU6050 at `0x68`, INA219 at `0x40` | PASS |
| Gyroscope calibration | 200/200 samples accepted | PASS (start-up bias only) |
| Magnetometer baseline | 24/24 samples | PASS (uncalibrated µT) |
| Environmental sensor | DHT11 about 23.8–24.5 °C, RH about 73–77 % | PASS |
| UV analog path | About 342–350 mV biased junction | PASS (raw mV, not calibrated) |
| UV reference calibration | High point rejected (span under 8 mV) | INCOMPLETE |
| Local storage | JSON and raw CSV appended to microSD | PASS |
| Wi-Fi uplink | HTTP POST returns 200 and the sequence number is echoed | PASS |
| Dashboard live feed | Latest packet retrieved in the browser | PASS (live packet without UV) |
| INA219 load power | Initialised. Current path not connected | NOT TESTED |
| LoRa, solar, battery | No physical RF or energy-chain test | NOT TESTED |

No throughput, packet-loss or power-consumption figures were recorded.

### Dashboard live feed

![Figure 4. Dashboard in live-feed mode](docs/images/dashboard_report.png)

*Figure 4. Live-feed dashboard (screenshot from the final report). The "SIMULATED DATA" label and the UV fallback are software caveats, not proof that the temperature or magnetometer values are synthetic.*

### Simulations

These are Phase 1 study models. They are not measurements.

#### Tinkercad: sensor and ground-link model

Design: https://www.tinkercad.com/things/ccVJsBLquyo-copy-of-falconeye

Tinkercad has no ESP32, so two Arduino Uno boards stand in. The transmitter has potentiometers for the UV and magnetic inputs. The receiver is linked to it by a wired SoftwareSerial line at 2400 baud. The model covers the data path only. It does not model the LoRa radio.

![Figure 5. Tinkercad circuit](docs/images/tinkercad_circuit.png)

*Figure 5. Tinkercad transmitter and wired ground receiver (simulation).*

| Scenario | Ground result | Interpretation |
|---|---|---|
| UV threshold crossed | LED, then buzzer above the selected threshold | Synthetic UV percentage, not UV index |
| Magnetic input moved from baseline | WARNING / CRITICAL | Emulated 20–80 µT range |
| `GAMMA_SIM` / `FLARE_SIM` | `FE\|CRITICAL\|…` packet and ground alarm | Software injection only |
| `LINKOFF` / `LINKON` | Communication lost, then restored | Serial-link recovery test |

#### Multisim: filter and power-stage models

**Analog filter.** A first-order 10 kΩ / 100 nF RC low-pass gives a cutoff of about 159 Hz (f_c = 1/(2πRC)). Its theoretical gain at cutoff is 0.707, with a phase lag of about 45°. The two-channel scope trace follows the expected attenuation and phase shift.

![Figure 6. Multisim oscilloscope](docs/images/multisim_scope.png)

*Figure 6. Multisim oscilloscope, RC low-pass input and output (simulation).*

**EPS model.** A boost stage, driven by a set duty cycle, produces about 5 V, which feeds an LM1117-3.3 regulator. At a 33 Ω load the regulator output is 3.297 V (about 99.9 mA). The model does not include charging, protection or battery safety.

| Input voltage | BOOST duty | Simulated 5 V output |
|---|---|---|
| 3.0 V | 0.40 | 4.997 V |
| 3.7 V | 0.26 | 4.998 V |
| 4.2 V | 0.16 | 4.999 V |

![Figure 7. Multisim meter readings](docs/images/multisim_eps_meters.png)

*Figure 7. Multisim meter readings: 4.997 V boost output and 3.297 V regulated rail (simulation).*

### Problems found and fixes

| Observed issue | Diagnosis and fix | Follow-up |
|---|---|---|
| Intermittent I²C NACKs | Reseated the GY-273, reduced the bus to 25 kHz, added retries and re-initialisation | Sustained soak test |
| Magnetometer chip ambiguity | Read the ID bytes and used the HMC5883L register sequence | Calibrate three axes against a field reference |
| UV signal too weak for the ADC | 10 kΩ / 100 kΩ bias network, averaged `analogReadMilliVolts` | Outdoor reference and sensitivity test |
| High UV calibration rejected | Low point 345.6 mV, high span under 8 mV | Do not force a UV index at night |
| MicroSD failed at 3.3 V | Tested with the 5 V USB supply | Confirm level shifting and ratings |
| Wi-Fi reconnect contention | Longer retry interval, restart of a failed connect | Log RSSI, buffer offline |
| Missing `uv_index` made the dashboard show `SIM` | The dashboard needs all three payload fields. A missing one is carried forward and the packet is labelled `SIM` | Open: add a partial-live label |
| UV warning stayed after a demo | The dashboard carried the old synthetic UV value | Open: show N/A and leave it out of alert logic |
| Repeated DHT11 value flagged as stuck | Rule removed. The DHT11 can repeat values legitimately | None |

## 6. Operations

This section covers running a session: start-up checks, serial commands, UV calibration, dashboard use, and demo mode. Set-up is in the [Build guide](#7-build-guide).

### Start-up checklist

1. Power the ESP32 over USB. Open the serial monitor at **115200** baud.
2. Check the start-up lines (abridged):
   ```
   HELIOS SHIELD / FALCONEYE V4.1
   MAG: READY
   MPU: READY
   GYRO calibration: 200/200 (OK)
   MAG baseline: 24/24 (OK)
   INA219: READY (I2C only; no load path)
   SD: READY
   UV calibration loaded: NOT READY
   Wi-Fi connection requested (2.4 GHz).
   V4.1 READY
   ```
3. Check that each packet prints the sensor values, the `JSON:` line, and `HTTP POST: 200`:
   ```
   JSON: {"source":"LIVE","seq":14025,"field_delta_uT":0.81,"temp_C":24.1}
   HTTP POST: 200
   HTTP response: {"ok":true,"seq":14025}
   ```
   `uv_index` is omitted until UV is calibrated.
4. Open the dashboard, set **Source** to **Live feed**, and press **Connect**. The `seq` value should advance about every 2 seconds.

If Wi-Fi is not connected, the sketch still logs to microSD and prints `HTTP: SKIPPED (Wi-Fi disconnected)`.

### Serial commands

Send these in the serial monitor, one per line.

| Command | Action |
|---|---|
| `HELP` | List the commands |
| `UV READ` | Print the current biased UV voltage (mV) |
| `UV STATUS` | Print the stored calibration points and whether UV index output is enabled |
| `UV CAL LOW <UVI>` | Capture the low reference point |
| `UV CAL HIGH <UVI>` | Capture the high reference point |
| `UV CAL RESET` | Clear both points. `uv_index` is then omitted |
| `DEMO ON` | Send synthetic UV, magnetic and temperature values, tagged `SIMULATED` |
| `DEMO OFF` or `CLEAR` | Return to real sensors only |
| `GAMMA_SIM` | Local message only. The API has no gamma field |

### UV calibration

The UV channel reads a biased voltage in mV. That voltage is not a UV index. The firmware sends `uv_index` only after two reference points are captured, and only inside the range between them.

**Procedure**
1. Take a UV index reading from a reference instrument you trust, at the same place and time as the sensor. Weather forecasts and phone flashlights are not calibration references.
2. Capture the low point: `UV CAL LOW <index>`.
3. Move to a higher, stable UV level and capture the high point: `UV CAL HIGH <index>`. The high point needs at least 8 mV more than the low point, and a higher index.
4. Check the result with `UV STATUS`. Points are stored in the ESP32's flash (Preferences) and survive a restart.

**Rules in the firmware**
- Index values must be between 0 and 15.
- The UV index is output only between the two points, with a 3 mV margin on each side.
- A new low point clears the high point.

**Current state.** One low point is stored (about 345.6 mV at UV index 0). The high point was rejected because the signal span was too small. The UV index is disabled, and packets omit `uv_index`.

### Using the dashboard

- **Source:** Simulation or Live feed. Live feed polls `/api/telemetry` every 2 seconds by default (1–60 s allowed). Paste JSON also works for replaying lines.
- **Presentation mode:** press **P** (or open `?present=1`). Press **P** again to exit.
- **Export CSV:** downloads the session's readings with time (UTC and UAE), source, UV, field change, temperature, gamma, flare and alert level.
- **Ground control timeline:** level changes, mode changes and simulated link events, newest first.
- **Globe:** a dot-matrix Earth with day and night from the real sun position. The orbit is illustrative. The satellite dot takes the alert colour.
- **Stats:** minimum, average and maximum of the readings under each gauge.

### Demo procedure

Use this when real conditions do not trigger an alert (for example, UV at night).

1. Send `DEMO ON` in the serial monitor. Packets carry synthetic UV, magnetic and temperature values tagged `SIMULATED`, and the dashboard labels them `SIM`.
2. Watch the gauges and timeline change level.
3. Send `DEMO OFF` to return to the real sensors.

Do not present demo values as measurements.

### Sending readings from a computer

`tools/send_sim.py` sends simulated readings, replays a file, or forwards live lines from a USB serial port.

```bash
python3 tools/send_sim.py --url https://YOUR-APP.vercel.app/api/telemetry --event magnetic --key YOUR_INGEST_KEY
python3 tools/send_sim.py --url https://YOUR-APP.vercel.app/api/telemetry --serial COM3 --source LIVE --key YOUR_INGEST_KEY   # needs pip install pyserial
```

## 7. Build guide

This section takes you from parts to a working live feed. Follow the steps in order.

### 1. Build the hardware

1. Collect the parts: ESP32-WROOM-32 DevKit, UVM-30A, GY-273, MPU6050 (GY-521), DHT11, INA219, microSD SPI module, 10 kΩ and 100 kΩ resistors, breadboard and jumper wires.
2. Wire them as in the [pin map](#pin-map).
3. Power the ESP32 over USB.

Do not connect a battery or solar input. Battery, charging and solar power were not tested, and the USB supply must not be backfed.

**Enclosure (optional).** Print the three STL files in `hardware/3d-print/` (see [Structure](#structure)). The layout was prepared for a Bambu Lab A1 mini, and print settings were not recorded. Fit has not been measured, so check clearances before you commit to a full print.

### 2. Deploy the ground segment

The dashboard is static HTML, CSS and JavaScript, plus one serverless function (`api/telemetry.js`). It has no build step.

1. Go to vercel.com/new and import this repository. Keep the defaults: framework "Other", no build command, no output directory.
2. Deploy. The last line of the output, or the project page, shows the address.
3. Add storage: in the Vercel project, open **Storage** (or **Marketplace**), add **Upstash Redis**, and connect it to the project.
4. Check that **Settings → Environment Variables** contains `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN`. If the integration used other names (for example `KV_REST_API_URL`), add these two names with the same values.
5. Create `INGEST_KEY` with a long random value. Anyone who can read it can post readings, so keep it private.
6. Redeploy the project. Environment variables apply only after a new deployment.

Without the store, the latest packet is kept in one serverless instance's memory, and another instance can return nothing. With the store configured, the function answers HTTP 503 with a reason if the store is unreachable, so data is not lost silently.

**Check the endpoint**

```bash
# Post a test packet (use your key)
curl -X POST https://YOUR-APP.vercel.app/api/telemetry \
  -H "Content-Type: application/json" \
  -H "x-api-key: YOUR_INGEST_KEY" \
  -d '{"source":"SIM","uv_index":5.4,"field_delta_uT":2.1,"temp_C":26.8}'

# Read the latest packet (no key needed)
curl https://YOUR-APP.vercel.app/api/telemetry
```

The POST returns `{"ok":true,"seq":...}`. The GET returns the stored packet.

**Run the dashboard without hardware.** Open `index.html` in a browser. It starts in Simulation mode. Live feeds need a server that serves `/api/telemetry`.

### 3. Configure and flash the firmware

File: `firmware/falconeye_v41/falconeye_v41.ino` (V4.1).

**Requirements**
- Arduino IDE 2.x, or `arduino-cli`
- ESP32 board package by Espressif. Board: **ESP32 Dev Module**
- Libraries:
  - Adafruit INA219
  - DHT sensor library (Adafruit). It also needs Adafruit Unified Sensor if the IDE asks for it
  - Built in with the ESP32 core: `Wire`, `SPI`, `SD`, `WiFi`, `WiFiClientSecure`, `HTTPClient`, `Preferences`

The library versions used for the report were not recorded. Use the latest stable versions. If the build fails, report the versions you used.

**Configure.** Edit the four values at the top of the sketch. Do not commit them to the repository.

```cpp
const char *WIFI_SSID = "";            // 2.4 GHz network (the ESP32 does not support 5 GHz)
const char *WIFI_PASSWORD = "";
const char *TELEMETRY_URL = "";        // https://YOUR-APP.vercel.app/api/telemetry
const char *INGEST_KEY = "";           // the same value as INGEST_KEY in Vercel
```

**Security.** `ALLOW_INSECURE_HTTPS_DEMO = true` turns off server certificate checks. Use it on the bench only. For any real deployment, paste the endpoint's CA certificate into `ROOT_CA_PEM` and set `ALLOW_INSECURE_HTTPS_DEMO` to `false`.

**Flash**
1. Select the board **ESP32 Dev Module**.
2. Select the USB port and upload.
3. Open the serial monitor at **115200** baud.

### 4. Check it works

Follow the [start-up checklist](#start-up-checklist). You should see `V4.1 READY`, then `HTTP POST: 200` every 2 seconds, and the dashboard `seq` advancing in **Live feed** mode.

### 5. Calibrate UV (optional)

Without calibration the firmware reports raw mV and omits `uv_index`. To enable the index, follow [UV calibration](#uv-calibration). It needs a trusted reference instrument in sunlight.

### 6. Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| `MAG: FAILED` or `MPU: FAILED` | Check the I²C wiring and reseat the module. The bus runs at 25 kHz, and intermittent NACKs came from a loose module |
| `SD: FAILED` | Power the module from 5 V. It failed at 3.3 V |
| `HTTP: SKIPPED (Wi-Fi disconnected)` | Use a 2.4 GHz network and check the SSID and password |
| HTTP 401 from the API | `x-api-key` is missing or does not match `INGEST_KEY`. Redeploy after changing it |
| HTTP 503 from the API | The Upstash store is unreachable. Check the two environment variables |
| Dashboard shows STALE or NO LINK | No new packet for 10 seconds. Check the serial monitor and the Wi-Fi link |
| No `uv_index` in packets | Expected until UV is calibrated |

## 8. Limitations and next steps

This section lists what the prototype does not do, and the work that would close each gap.

### Known limits

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

### Next steps

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

## 9. Glossary

Terms and abbreviations used in this repository.

| Term | Meaning |
|---|---|
| ADC | Analog-to-digital converter. Turns a voltage into a number. The ESP32 ADC reads the UV signal |
| Baseline | The reading taken at boot. Magnetic change is measured relative to it |
| Biased junction | The point where the 10 kΩ and 100 kΩ resistors meet. The 100 kΩ resistor to 3.3 V lifts the weak UV signal into the ADC's readable range |
| BOM | Bill of materials. The parts list |
| C&DH | Command and data handling. The onboard functions that read sensors, build packets and store data |
| CPM | Counts per minute. The unit for the simulated gamma channel |
| CubeSat | A small standard-size satellite built from 10 cm cube units |
| Downlink | The radio or network path from the satellite to the ground |
| EPS | Electrical power system. Generates, stores, regulates and distributes power |
| Engineering model | A functional prototype used to test a design. It is not built for flight |
| Fault | Dashboard state for a value outside its valid range. The value is not used |
| Ground segment | Everything on the ground that receives and displays satellite data: the API and the dashboard |
| I²C | A two-wire bus (SDA and SCL) that connects the magnetometer, motion sensor and power monitor |
| ICD | Interface control document. Defines the pins, addresses and data formats between parts. See [Interfaces](#4-interfaces) |
| IMU | Inertial measurement unit. Here, the MPU6050 accelerometer and gyroscope |
| INA219 | A voltage and current monitor chip. Present on the bus but not in the load path |
| JSONL | JSON lines. One JSON object per line in a text file |
| LEO | Low Earth orbit |
| LIVE | Dashboard label for a packet from the real device with all payload fields |
| LoRa | A long-range, low-power radio. Not implemented here |
| mV | Millivolt. The unit of the raw UV reading |
| µT | Microtesla. The unit of magnetic-field strength |
| Payload | The instruments the mission exists to carry. Here: UV and magnetometer |
| Preferences | ESP32 flash storage for small settings. Holds the UV calibration points |
| RSSI | Received signal strength indicator. The Wi-Fi signal level in dBm |
| `seq` | Sequence number in each packet. A new value means a new reading |
| SIM | Dashboard label for simulated data, or a packet with a missing payload field |
| SPI | A four-wire bus. Connects the microSD module |
| STALE | Dashboard state when no new packet has arrived within the timeout |
| STL | A 3D model file format used for printing |
| Telemetry | Measurements sent from the satellite to the ground |
| TLS | The encryption and certificate check behind HTTPS |
| UV index | A scale of ultraviolet intensity, 0 to 15 here. Needs calibration against a reference |
| 1U | One CubeSat unit, a nominal 10 cm cube |

## Repository map

```
docs/                                     documentation pages and figures (docs/images/)
firmware/falconeye_v41/falconeye_v41.ino  ESP32 firmware, V4.1
hardware/3d-print/                        STL files for the enclosure (3 print groups)
api/telemetry.js                          Vercel function: POST a packet, GET the latest
index.html, app.js, styles.css, land.js   dashboard (no build step)
data/                                     sample packets, tagged SIM
tools/send_sim.py                         sender: simulated readings, replay file, or USB serial
PRD.md                                    dashboard requirements and acceptance criteria
```

## References
- Espressif, ESP32-WROOM-32 datasheet: https://documentation.espressif.com/esp32-wroom-32_datasheet_en.html
- Honeywell, HMC5883L magnetometer: https://strawberry-linux.com/pub/HMC5883L.pdf
- Texas Instruments, INA219: https://www.ti.com/product/INA219
- Texas Instruments, LM1117: https://www.ti.com/product/LM1117
- NOAA Space Weather Prediction Center, GOES X-ray flux: https://www.spaceweather.gov/products/goes-x-ray-flux

## Team
- Inshal Ahmed Syed
- Mohamed Faheem
- Muhammad Rashid Mehmood
- Muhammad Badr
