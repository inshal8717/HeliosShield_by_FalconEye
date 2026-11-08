# Build guide

This page takes you from parts to a working live feed. Follow the steps in order. [Back to README](../README.md).

## 1. Build the hardware

1. Collect the parts: ESP32-WROOM-32 DevKit, UVM-30A, GY-273, MPU6050 (GY-521), DHT11, INA219, microSD SPI module, 10 kΩ and 100 kΩ resistors, breadboard and jumper wires.
2. Wire them as in the [pin map](interfaces.md#pin-map).
3. Power the ESP32 over USB.

Do not connect a battery or solar input. Battery, charging and solar power were not tested, and the USB supply must not be backfed.

**Enclosure (optional).** Print the three STL files in `hardware/3d-print/` (see [Structure](subsystems.md#structure)). The layout was prepared for a Bambu Lab A1 mini, and print settings were not recorded. Fit has not been measured, so check clearances before you commit to a full print.

## 2. Deploy the ground segment

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

## 3. Configure and flash the firmware

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

## 4. Check it works

Follow the [start-up checklist](operations.md#start-up-checklist). You should see `V4.1 READY`, then `HTTP POST: 200` every 2 seconds, and the dashboard `seq` advancing in **Live feed** mode.

## 5. Calibrate UV (optional)

Without calibration the firmware reports raw mV and omits `uv_index`. To enable the index, follow [UV calibration](operations.md#uv-calibration). It needs a trusted reference instrument in sunlight.

## 6. Troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| `MAG: FAILED` or `MPU: FAILED` | Check the I²C wiring and reseat the module. The bus runs at 25 kHz, and intermittent NACKs came from a loose module |
| `SD: FAILED` | Power the module from 5 V. It failed at 3.3 V |
| `HTTP: SKIPPED (Wi-Fi disconnected)` | Use a 2.4 GHz network and check the SSID and password |
| HTTP 401 from the API | `x-api-key` is missing or does not match `INGEST_KEY`. Redeploy after changing it |
| HTTP 503 from the API | The Upstash store is unreachable. Check the two environment variables |
| Dashboard shows STALE or NO LINK | No new packet for 10 seconds. Check the serial monitor and the Wi-Fi link |
| No `uv_index` in packets | Expected until UV is calibrated |
