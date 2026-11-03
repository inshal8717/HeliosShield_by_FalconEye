# FalconEye Helios Shield: live dashboard

1U CubeSat engineering model, ground-based prototype, not flown. Static web app plus one small serverless endpoint. No build step, no dependencies.

```
index.html        page (open it as is; it runs in Simulation mode with no setup)
styles.css        theme
app.js            dashboard logic: simulation, live feed, alert rules, drawing
api/telemetry.js  Vercel function: POST a reading, GET the latest one
data/             sample packet and a 40-line storm replay (tagged SIM)
land.js           1-degree world land mask for the dot globe
tools/send_sim.py sender: simulated readings, a replay file, or an Arduino on USB
PRD.md            requirements and acceptance criteria
```

## 1. Deploy to Vercel

**Option A, command line (needs Node.js):**
```
cd falconeye-dashboard
npx vercel login
npx vercel --prod
```
Answer the prompts with the defaults (framework "Other", no build command, no output directory). The last line prints your public address.

**Option B, GitHub:** create a repository, upload these files (GitHub's "Add file → Upload files"), then import the repository at vercel.com/new. Keep the defaults.

Then open the address in a private window. If Vercel asks you to log in, turn off Deployment Protection under Project → Settings so judges can open it.

Any static host also works (Netlify Drop, GitHub Pages), but `/api/telemetry` only runs on Vercel. On other hosts use a feed URL instead (section 2, way A).

## 2. Feed it live JSON

Open the page, set **Source** to **Live feed**. Three ways in:

**A. Feed URL.** Paste any address that returns the JSON packet below, set the poll interval, press **Connect**. Cross-origin addresses must allow CORS. Skip the box entirely by opening `https://YOUR-APP.vercel.app/?feed=https://example.com/latest.json&interval=2`.

**B. Post to your own endpoint.** The default feed URL is `/api/telemetry`. Send readings to it from anything that can make an HTTPS request:
```
curl -X POST https://YOUR-APP.vercel.app/api/telemetry \
  -H "Content-Type: application/json" \
  -d '{"source":"SIM","uv_index":5.4,"field_delta_uT":2.1,"temp_C":26.8}'
```
Or use the helper: `python3 tools/send_sim.py --url https://YOUR-APP.vercel.app/api/telemetry --event magnetic`.

**C. Paste JSON.** **Paste JSON** accepts one packet, an array, or one packet per line. Good for replaying lines copied from Tinkercad's serial monitor.

### Packet format
```json
{ "source": "LIVE", "t": "2026-10-30T07:15:00Z", "seq": 42,
  "uv_index": 5.4, "field_delta_uT": 2.1, "temp_C": 26.8,
  "gamma_cpm": 19, "flare_index": 14 }
```
- `uv_index`, `field_delta_uT`, `temp_C` are the real payload channels. `gamma_cpm` and `flare_index` are optional simulated threat inputs; if missing, a SIM baseline is used.
- `source` must be `"LIVE"` for LIVE badges. Missing, or anything else, shows SIM. A packet that is missing a payload channel is always shown as SIM and the banner says which field is missing.
- `seq` or `t` lets the page skip repeated packets and show STALE when the feed stops (after about 10 s).
- Also accepted: `uv`, `field_uT`, `dB`, `temp`, `cpm`, `flare`.
- Optional device health, shown under Channels: `device` (text, e.g. `"falconeye-esp32"`), `rssi` (Wi-Fi signal, dBm), `uptime_s` (seconds since boot).
- A failed sensor: leave its field out. Do not send 0. The packet is then shown as SIM and the banner names the missing field.

### Fault rules
- **Out of range:** UV outside 0 to 15, field change outside ±100 µT, temperature outside −40 to 85 °C, gamma outside 0 to 1000 CPM, flare outside 0 to 100. The value is not used: the gauge reads FAULT, the status strip shows FAULT, and the timeline logs it.
- **Stuck sensor:** the same non-zero value 15 readings in a row. Flagged the same way. Zero is exempt (UV is 0 in the dark).
- **Link lost:** no new packet for 10 s (or 3 poll intervals, if longer). The telemetry dims, a NO LINK note shows the packet age, and the timeline logs "Link lost" and later "Link restored".

### Tinkercad
Tinkercad cannot post to the internet. Have the Arduino print one JSON line per reading, tag it `"source":"SIM"` (it is a simulation), copy the serial monitor output, and use **Paste JSON**:
```cpp
Serial.print("{\"source\":\"SIM\",\"uv_index\":");   Serial.print(uv, 1);
Serial.print(",\"field_delta_uT\":");                Serial.print(dB, 1);
Serial.print(",\"temp_C\":");                        Serial.print(tempC, 1);
Serial.println("}");
```
Use `"LIVE"` only for real sensors on the bench. For those, run `python3 tools/send_sim.py --url ... --serial COM3 --source LIVE` (needs `pip install pyserial`).

## 3. Using the dashboard
- **Presentation mode:** press **P** (or open `?present=1`). Controls disappear and the numbers get bigger, which suits the demo video. Press P again to go back.
- **Export CSV:** downloads every reading of this session (time, source SIM or LIVE, UV, field change, temperature, gamma, flare, level). Changing the event or the source starts a new session.
- **Ground control timeline:** level changes, mode changes and the simulated LoRa packets in one list, newest first.
- **Globe:** dot-matrix Earth with day and night from the real sun position. The orbit ring is decorative and the satellite dot takes the alert colour. Orbit, gamma and flare inputs and the LoRa link are simulated.
- Under each gauge: minimum, average and maximum of the readings on the charts.

## 4. Storage on Vercel (do this before the demo)
Without a store, the latest reading lives in one serverless instance's memory, and the dashboard can read from a different instance and see nothing. To fix it:
1. In Vercel: Project → Storage (or Marketplace) → add **Upstash Redis** (free tier) and connect it to this project.
2. Check that Project → Settings → Environment Variables now has `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN`. If the integration named them differently (for example `KV_REST_API_URL`), add these two names with the same values.
3. Redeploy. Post a reading, then open `/api/telemetry` in a private window: it should return that reading.
If the store is unreachable, the endpoint answers HTTP 503 with the reason instead of silently losing data.

## 5. Lock the endpoint (recommended)
Without a key, anyone who knows the address can post readings. In Vercel: Project → Settings → Environment Variables, add `INGEST_KEY` with a long random value, then redeploy. Senders must then include the header `x-api-key: <value>` (`--key` in `send_sim.py`). Reading (GET) stays open so the dashboard can poll.

## 6. Known limits
- `/api/telemetry` keeps only the latest reading. The dashboard polls it, so if the ESP32 sends faster than the dashboard polls, some readings are not shown. Keep both at 2 s.
- History on the charts lives in the browser tab and resets on reload.
- Orbit globe, gamma and flare inputs, and the LoRa link are simulated and labelled as such. Nothing is transmitted over radio.
- Magnetic baseline (50 µT) and thresholds are placeholders; confirm them before Phase 2.
- Do not state mass, power or link performance until measured.
