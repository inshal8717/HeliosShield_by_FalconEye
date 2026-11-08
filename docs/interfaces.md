# Interfaces

This page is the interface reference: pins, bus addresses, the telemetry packet, and alert thresholds. [Back to README](../README.md).

## Pin map

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

## I²C bus

| Device | Address |
|---|---|
| GY-273 magnetometer | `0x1E` |
| MPU6050 | `0x68` |
| INA219 | `0x40` |

Bus speed 25 kHz, timeout 150 ms.

## Telemetry packet

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

## HTTP API

| Method | Path | Auth | Result |
|---|---|---|---|
| POST | `/api/telemetry` | `x-api-key` header, must equal `INGEST_KEY` | `{"ok":true,"seq":N}` |
| GET | `/api/telemetry` | none | The latest stored packet |

If the store is unreachable the function returns HTTP 503 with a reason.

## Alert thresholds

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

## Files written to the microSD card

| File | Contents |
|---|---|
| `/falconeye_v41.jsonl` | One JSON packet per line, as sent |
| `/falconeye_raw_v41.csv` | Raw values per cycle: UV mV, magnetometer counts, accelerometer and gyroscope values, temperature, humidity, and sensor status flags |
