# Architecture

This page shows the subsystems and how data moves between them. [Back to README](../README.md).

## Block diagram

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

## Subsystem table

| Subsystem | Parts | Role | Status |
|---|---|---|---|
| Payloads | UVM-30A UV module, GY-273 magnetometer | Measure UV and magnetic-field change | Working. UV in mV only |
| Support sensors | MPU6050, DHT11 | Motion and orientation, temperature and humidity | Working. Motion data is logged, not sent |
| Onboard computer and data handling | ESP32-WROOM-32, microSD module | Read sensors, build packets, log, uplink | Working |
| Communications | ESP32 Wi-Fi, HTTPS | Send each packet to the cloud API | Working. LoRa not implemented |
| Ground segment | Vercel function, Upstash Redis, browser dashboard | Store the latest packet, apply alert rules, display | Working |
| Power | USB in tests. INA219 on the I²C bus | Supply and monitor power | USB only. INA219 not in the load path. Battery and solar not tested |
| Structure | Three-part 3D-printed frame | Hold the electronics in a 1U-style envelope | Printed. Not secured or measured |

## Design choices

- **Wi-Fi in place of LoRa.** Wi-Fi was proven on the bench. The wired SoftwareSerial link in the Tinkercad model stood in for the radio in Phase 1.
- **UV as raw voltage.** The UV signal is too weak for the ESP32 ADC alone, so a bias network lifts it into the readable range. The firmware reports millivolts, not a UV index. See [UV subsystem](subsystems.md#uv-payload).
- **Alerts on the ground.** The onboard computer sends measurements. The dashboard decides alert levels, so thresholds change without re-flashing the firmware.
- **Missing data is left out.** A failed sensor is omitted from the packet. The firmware never sends 0 for a missing reading.

## More detail

- Per-subsystem description: [Subsystems](subsystems.md)
- Pins, addresses and packet fields: [Interfaces](interfaces.md)
