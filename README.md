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

Details are in [Verification](docs/verification.md) and [Limitations](docs/limitations.md).

## Documentation

| Page | What it covers |
|---|---|
| [Mission](docs/mission.md) | Problem, objectives, scope, and the objective-to-evidence matrix |
| [Architecture](docs/architecture.md) | Subsystems and how data moves between them |
| [Subsystems](docs/subsystems.md) | Payloads, controller, comms, ground segment, power, structure |
| [Interfaces](docs/interfaces.md) | Pin map, I²C addresses, telemetry packet, alert thresholds |
| [Verification](docs/verification.md) | Bench results, simulations, problems found and fixes |
| [Operations](docs/operations.md) | Start-up checks, serial commands, dashboard use, demo mode |
| [Build guide](docs/build-guide.md) | Reproduce the hardware, firmware, enclosure and dashboard |
| [Limitations](docs/limitations.md) | Known limits and prioritised next steps |
| [Glossary](docs/glossary.md) | Terms and abbreviations |

## Quick start

1. Wire the ESP32 and sensors as in [Interfaces](docs/interfaces.md#pin-map).
2. Deploy the dashboard and set `INGEST_KEY` ([Build guide, step 2](docs/build-guide.md#2-deploy-the-ground-segment)).
3. Fill in the four values at the top of the sketch and flash it ([Build guide, step 3](docs/build-guide.md#3-configure-and-flash-the-firmware)).
4. Open the dashboard, set **Source** to **Live feed**, and press **Connect**.

To try the dashboard with no hardware, open `index.html` in a browser. It starts in Simulation mode. The last deployed build is at https://helioshield-iota.vercel.app.

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
