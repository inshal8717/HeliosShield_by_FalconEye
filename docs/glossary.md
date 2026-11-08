# Glossary

Terms and abbreviations used in this repository. [Back to README](../README.md).

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
| ICD | Interface control document. Defines the pins, addresses and data formats between parts. See [Interfaces](interfaces.md) |
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
