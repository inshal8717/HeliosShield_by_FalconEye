# Operations

This page covers running a session: start-up checks, serial commands, UV calibration, dashboard use, and demo mode. Set-up is in the [Build guide](build-guide.md). [Back to README](../README.md).

## Start-up checklist

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

## Serial commands

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

## UV calibration

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

## Using the dashboard

- **Source:** Simulation or Live feed. Live feed polls `/api/telemetry` every 2 seconds by default (1–60 s allowed). Paste JSON also works for replaying lines.
- **Presentation mode:** press **P** (or open `?present=1`). Press **P** again to exit.
- **Export CSV:** downloads the session's readings with time (UTC and UAE), source, UV, field change, temperature, gamma, flare and alert level.
- **Ground control timeline:** level changes, mode changes and simulated link events, newest first.
- **Globe:** a dot-matrix Earth with day and night from the real sun position. The orbit is illustrative. The satellite dot takes the alert colour.
- **Stats:** minimum, average and maximum of the readings under each gauge.

## Demo procedure

Use this when real conditions do not trigger an alert (for example, UV at night).

1. Send `DEMO ON` in the serial monitor. Packets carry synthetic UV, magnetic and temperature values tagged `SIMULATED`, and the dashboard labels them `SIM`.
2. Watch the gauges and timeline change level.
3. Send `DEMO OFF` to return to the real sensors.

Do not present demo values as measurements.

## Sending readings from a computer

`tools/send_sim.py` sends simulated readings, replays a file, or forwards live lines from a USB serial port.

```bash
python3 tools/send_sim.py --url https://YOUR-APP.vercel.app/api/telemetry --event magnetic --key YOUR_INGEST_KEY
python3 tools/send_sim.py --url https://YOUR-APP.vercel.app/api/telemetry --serial COM3 --source LIVE --key YOUR_INGEST_KEY   # needs pip install pyserial
```
