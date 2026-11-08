#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <DHT.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <math.h>

// HELIOS SHIELD / FALCONEYE - V4.1 dashboard + UV calibration.
// Keep the existing tested breadboard wiring and USB power.
// Do not publish this sketch after entering network/API secrets.

// ===== EDIT THESE THREE VALUES LOCALLY =====
const char *WIFI_SSID = "";
const char *WIFI_PASSWORD = "";
const char *TELEMETRY_URL = "";
const char *INGEST_KEY = "";

// BENCH DEMO ONLY. Replace with a verified CA certificate for submission.
const bool ALLOW_INSECURE_HTTPS_DEMO = true;
const char *ROOT_CA_PEM = ""; // Your verified endpoint's trusted CA PEM.

// The UV junction is BIASED by the existing 10k/100k resistor network.
// Never apply a generic mV-to-UV-index conversion to this voltage.
// UV index is omitted until two known-reference points are recorded.
// Captured points are retained in ESP32 Preferences (NVS).
const float UV_MIN_CAL_SPAN_MV = 8.0f;
const float UV_RANGE_MARGIN_MV = 3.0f;

// ----- Connections from working V3 -----
const int UV_PIN = 34;       // UV OUT via 10k, with 100k to 3V3
const int DHT_PIN = 27;
const int SD_CS = 5;
const int MAG_ADDR = 0x1E;
const int MPU_ADDR = 0x68;
const uint32_t I2C_HZ = 25000;
const uint32_t PACKET_INTERVAL_MS = 2000;
const char *JSONL_FILE = "/falconeye_v41.jsonl";
const char *RAW_CSV_FILE = "/falconeye_raw_v41.csv";

// HMC5883L compatible gain register 0x20: nominal 1090 counts/gauss,
// or 10.9 counts per microtesla. Approximate until magnet calibrated.
const float MAG_COUNTS_PER_UT = 10.9f;

DHT dht(DHT_PIN, DHT11);
Adafruit_INA219 ina219(0x40);
Preferences preferences;

bool magReady = false;
bool mpuReady = false;
bool inaReady = false;
bool sdReady = false;
bool gyroCalibrated = false;
bool magBaselineReady = false;
bool demoMode = false;
bool preferencesReady = false;
bool uvLowSet = false, uvHighSet = false;
float uvLowMv = NAN, uvHighMv = NAN;
float uvLowIndex = NAN, uvHighIndex = NAN;

float gyroOffsetX = 0, gyroOffsetY = 0, gyroOffsetZ = 0;
float magBaseX = 0, magBaseY = 0, magBaseZ = 0;
uint32_t lastPacketMs = 0;
uint32_t lastWiFiTryMs = 0;
uint32_t lastMagTryMs = 0;
uint8_t magReadFailures = 0;
uint32_t nextSeq = 1;
uint32_t reservedSeqEnd = 1;
String serialCommandBuffer;

struct Reading {
  float uvMv = NAN, uvIndex = NAN, tempC = NAN, humidity = NAN;
  float magneticDeltaUT = NAN;
  float ax = NAN, ay = NAN, az = NAN;
  float gx = NAN, gy = NAN, gz = NAN;
  int16_t mx = 0, my = 0, mz = 0;
  bool magOK = false, mpuOK = false, dhtOK = false;
};

int16_t combine16(uint8_t hi, uint8_t lo) {
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

bool writeReg(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t address, uint8_t reg, uint8_t *data, uint8_t length) {
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) == 0) {
      int received = Wire.requestFrom(address, length);
      if (received == length) {
        for (int i = 0; i < length; i++) data[i] = Wire.read();
        return true;
      }
      while (Wire.available()) Wire.read();
    }
    delay(15);
  }
  return false;
}

bool initializeMag() {
  for (int attempt = 1; attempt <= 5; attempt++) {
    bool ok = writeReg(MAG_ADDR, 0x00, 0x70) &&
              writeReg(MAG_ADDR, 0x01, 0x20) &&
              writeReg(MAG_ADDR, 0x02, 0x00);
    if (ok) { delay(100); return true; }
    delay(250);
  }
  return false;
}

bool getMag(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t data[6];
  if (!readRegs(MAG_ADDR, 0x03, data, 6)) return false;
  x = combine16(data[0], data[1]);
  z = combine16(data[2], data[3]);
  y = combine16(data[4], data[5]);
  // HMC overflow sentinel; do not treat it as a measurement.
  return x != -4096 && y != -4096 && z != -4096;
}

bool initializeMPU() {
  return writeReg(MPU_ADDR, 0x6B, 0x00) &&
         writeReg(MPU_ADDR, 0x1C, 0x00) &&
         writeReg(MPU_ADDR, 0x1B, 0x00);
}

void calibrateGyro() {
  Serial.println("Keep MPU6050 still for gyro calibration...");
  delay(1500);
  double sx = 0, sy = 0, sz = 0;
  int good = 0;
  for (int i = 0; i < 200; i++) {
    uint8_t b[6];
    if (readRegs(MPU_ADDR, 0x43, b, 6)) {
      sx += combine16(b[0], b[1]) / 131.0;
      sy += combine16(b[2], b[3]) / 131.0;
      sz += combine16(b[4], b[5]) / 131.0;
      good++;
    }
    delay(10);
  }
  gyroCalibrated = good >= 180;
  if (gyroCalibrated) {
    gyroOffsetX = sx / good;
    gyroOffsetY = sy / good;
    gyroOffsetZ = sz / good;
  }
  Serial.printf("GYRO calibration: %d/200 (%s)\n",
                good, gyroCalibrated ? "OK" : "FAILED");
}

void calibrateMagBaseline() {
  magBaselineReady = false;
  if (!magReady) return;
  Serial.println("Keep magnetometer still and away from magnets: baseline...");
  double sx = 0, sy = 0, sz = 0;
  int good = 0;
  for (int i = 0; i < 24; i++) {
    int16_t x, y, z;
    if (getMag(x, y, z)) {
      sx += x; sy += y; sz += z; good++;
    }
    delay(100);
  }
  if (good >= 18) {
    magBaseX = sx / good;
    magBaseY = sy / good;
    magBaseZ = sz / good;
    magBaselineReady = true;
  }
  Serial.printf("MAG baseline: %d/24 (%s)\n",
                good, magBaselineReady ? "OK" : "FAILED");
}

void reserveSequenceBlock() {
  nextSeq = preferences.getULong("nextSeq", 1UL);
  reservedSeqEnd = nextSeq + 1000UL;
  preferences.putULong("nextSeq", reservedSeqEnd);
}

uint32_t getNextSequence() {
  if (nextSeq >= reservedSeqEnd) reserveSequenceBlock();
  return nextSeq++;
}

void initializeSD() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(18, 19, 23, SD_CS);
  sdReady = SD.begin(SD_CS, SPI, 1000000);
  Serial.printf("SD: %s\n", sdReady ? "READY" : "FAILED");
}

void initializeWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (String(WIFI_PASSWORD) == "PASTE_WIFI_PASSWORD_HERE") {
    Serial.println("Wi-Fi credentials not configured yet.");
    return;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiTryMs = millis();
  Serial.println("Wi-Fi connection requested (2.4 GHz).");
}


void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  if (String(WIFI_PASSWORD) ==
      "PASTE_WIFI_PASSWORD_HERE") {
    return;
  }

  // Allow 30 seconds for each attempt
  if (millis() - lastWiFiTryMs < 30000UL) {
    return;
  }

  Serial.printf(
    "Wi-Fi status: %d\n",
    WiFi.status()
  );

  Serial.println("Restarting Wi-Fi connection...");

  WiFi.disconnect(false, false);
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWiFiTryMs = millis();
}


// ===== TWO-POINT UV CALIBRATION (REFERENCE REQUIRED) =====
// Measured quantity: ADC at the existing biased resistor junction, in mV.
// A UV Index reference should be measured beside the sensor at the same time.
// Weather forecasts provide only a rough comparison, NOT instrument calibration.
float readUVBiasedMv(int samples = 60) {
  float sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(UV_PIN);
    delay(10);
  }
  return sum / samples;
}

bool uvCalibrationValid() {
  return uvLowSet && uvHighSet &&
         isfinite(uvLowMv) && isfinite(uvHighMv) &&
         isfinite(uvLowIndex) && isfinite(uvHighIndex) &&
         uvHighMv > uvLowMv + UV_MIN_CAL_SPAN_MV &&
         uvHighIndex > uvLowIndex + 0.1f &&
         uvLowIndex >= 0.0f && uvHighIndex <= 15.0f;
}

void loadUVCalibration() {
  if (!preferencesReady) return;
  uvLowSet = preferences.getBool("uvLowOK", false);
  uvHighSet = preferences.getBool("uvHighOK", false);
  uvLowMv = preferences.getFloat("uvLowMv", NAN);
  uvHighMv = preferences.getFloat("uvHighMv", NAN);
  uvLowIndex = preferences.getFloat("uvLowIdx", NAN);
  uvHighIndex = preferences.getFloat("uvHighIdx", NAN);
  Serial.printf("UV calibration loaded: %s\n",
                uvCalibrationValid() ? "TWO POINTS READY (estimated)" : "NOT READY");
}

void printUVStatus() {
  Serial.println("--- UV CALIBRATION STATUS ---");
  Serial.printf("Current biased junction: %.1f mV\n", readUVBiasedMv(40));
  if (uvLowSet) Serial.printf("LOW: %.1f mV at reference UVI %.2f\n", uvLowMv, uvLowIndex);
  else Serial.println("LOW: not captured");
  if (uvHighSet) Serial.printf("HIGH: %.1f mV at reference UVI %.2f\n", uvHighMv, uvHighIndex);
  else Serial.println("HIGH: not captured");
  Serial.printf("Index output: %s\n", uvCalibrationValid() ? "ENABLED (approximate)" : "DISABLED");
  Serial.println("A real UV reference is required, not an invented number.");
}

void clearUVCalibration() {
  uvLowSet = uvHighSet = false;
  uvLowMv = uvHighMv = NAN;
  uvLowIndex = uvHighIndex = NAN;
  if (preferencesReady) {
    preferences.putBool("uvLowOK", false);
    preferences.putBool("uvHighOK", false);
  }
  Serial.println("UV calibration cleared; uv_index will be omitted.");
}

void captureUVPoint(bool high, float referenceIndex) {
  if (!isfinite(referenceIndex) || referenceIndex < 0.0f || referenceIndex > 15.0f) {
    Serial.println("UV CAL ERROR: reference UV Index must be between 0 and 15.");
    return;
  }
  if (high && !uvLowSet) {
    Serial.println("UV CAL ERROR: capture LOW first (e.g. UV CAL LOW 0).");
    return;
  }
  Serial.println("UV CAL: sampling junction; keep sensor and lighting stable...");
  float measured = readUVBiasedMv(100);
  if (!isfinite(measured)) {
    Serial.println("UV CAL ERROR: bad ADC measurement.");
    return;
  }
  if (high) {
    if (measured <= uvLowMv + UV_MIN_CAL_SPAN_MV ||
        referenceIndex <= uvLowIndex + 0.1f) {
      Serial.printf("UV CAL ERROR: need >%.1f mV signal span and a higher reference UVI.\n",
                    UV_MIN_CAL_SPAN_MV);
      return;
    }
    uvHighMv = measured;
    uvHighIndex = referenceIndex;
    uvHighSet = true;
    if (preferencesReady) {
      preferences.putFloat("uvHighMv", uvHighMv);
      preferences.putFloat("uvHighIdx", uvHighIndex);
      preferences.putBool("uvHighOK", true);
    }
    Serial.printf("UV HIGH saved: %.1f mV at reference UVI %.2f\n", measured, referenceIndex);
  } else {
    uvLowMv = measured;
    uvLowIndex = referenceIndex;
    uvLowSet = true;
    // New low point invalidates any previously captured high point.
    uvHighSet = false;
    if (preferencesReady) {
      preferences.putFloat("uvLowMv", uvLowMv);
      preferences.putFloat("uvLowIdx", uvLowIndex);
      preferences.putBool("uvLowOK", true);
      preferences.putBool("uvHighOK", false);
    }
    Serial.printf("UV LOW saved: %.1f mV at reference UVI %.2f\n", measured, referenceIndex);
  }
  if (!preferencesReady) Serial.println("WARNING: Preferences unavailable; values will not survive restart.");
  printUVStatus();
}

bool parseUVIndex(String input, float &value) {
  input.trim();
  if (input.isEmpty()) return false;
  char *end = nullptr;
  value = strtof(input.c_str(), &end);
  return end != input.c_str() && *end == '\0' && isfinite(value);
}

float calibrateUVIndex(float biasedMv) {
  if (!uvCalibrationValid() || !isfinite(biasedMv)) return NAN;
  // Do not extrapolate far outside the two reference points.
  if (biasedMv < uvLowMv - UV_RANGE_MARGIN_MV ||
      biasedMv > uvHighMv + UV_RANGE_MARGIN_MV) return NAN;
  float uv = uvLowIndex + (biasedMv - uvLowMv) *
             (uvHighIndex - uvLowIndex) / (uvHighMv - uvLowMv);
  return constrain(uv, 0.0f, 15.0f);
}

Reading sampleSensors() {
  Reading r;
  long uvSum = 0;
  for (int i = 0; i < 40; i++) {
    uvSum += analogReadMilliVolts(UV_PIN);
    delay(2);
  }
  r.uvMv = uvSum / 40.0f;
  r.uvIndex = calibrateUVIndex(r.uvMv);

  if (!magReady && millis() - lastMagTryMs > 10000UL) {
    lastMagTryMs = millis();
    magReady = initializeMag();
    if (magReady) calibrateMagBaseline();
  }

  if (magReady) {
    r.magOK = getMag(r.mx, r.my, r.mz);
    if (r.magOK && magBaselineReady) {
      float dx = r.mx - magBaseX;
      float dy = r.my - magBaseY;
      float dz = r.mz - magBaseZ;
      r.magneticDeltaUT = sqrtf(dx*dx + dy*dy + dz*dz) /
                           MAG_COUNTS_PER_UT;
    }
    if (r.magOK) {
      magReadFailures = 0;
    } else {
      magReadFailures++;
      Serial.println("MAG read failed; retry next cycle.");
      if (magReadFailures >= 3) {
        magReady = false;
        magBaselineReady = false;
        lastMagTryMs = millis();
        magReadFailures = 0;
        Serial.println("MAG temporarily offline; reinitializing later.");
      }
    }
  }

  uint8_t mpu[14];
  r.mpuOK = mpuReady && readRegs(MPU_ADDR, 0x3B, mpu, 14);
  if (r.mpuOK) {
    r.ax = combine16(mpu[0], mpu[1]) / 16384.0f;
    r.ay = combine16(mpu[2], mpu[3]) / 16384.0f;
    r.az = combine16(mpu[4], mpu[5]) / 16384.0f;
    if (gyroCalibrated) {
      r.gx = combine16(mpu[8], mpu[9]) / 131.0f - gyroOffsetX;
      r.gy = combine16(mpu[10], mpu[11]) / 131.0f - gyroOffsetY;
      r.gz = combine16(mpu[12], mpu[13]) / 131.0f - gyroOffsetZ;
    }
  }

  r.tempC = dht.readTemperature();
  r.humidity = dht.readHumidity();
  r.dhtOK = isfinite(r.tempC) && isfinite(r.humidity);
  return r;
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (command == "UV READ") {
    Serial.printf("UV biased junction: %.1f mV (not UV Index)\n", readUVBiasedMv());
    Serial.println("UV must change with sunlight; LED/phone flashlight is not a reliable UV reference.");
  } else if (command == "UV STATUS") {
    printUVStatus();
  } else if (command == "UV CAL RESET") {
    clearUVCalibration();
  } else if (command.startsWith("UV CAL LOW ") || command.startsWith("UV CAL HIGH ")) {
    bool high = command.startsWith("UV CAL HIGH ");
    const String prefix = high ? "UV CAL HIGH " : "UV CAL LOW ";
    float reference = NAN;
    if (!parseUVIndex(command.substring(prefix.length()), reference)) {
      Serial.println("UV CAL ERROR: enter a numeric reference index, e.g. UV CAL LOW 0");
    } else {
      captureUVPoint(high, reference);
    }
  } else if (command == "DEMO ON") {
    demoMode = true;
    Serial.println("SIMULATION ENABLED: Dashboard figures are synthetic.");
  } else if (command == "DEMO OFF" || command == "CLEAR") {
    demoMode = false;
    Serial.println("SIMULATION DISABLED: Real sensors only.");
  } else if (command == "GAMMA_SIM") {
    Serial.println("GAMMA_SIM: local simulated event only; dashboard API has no gamma field.");
  } else if (command == "HELP") {
    Serial.println("Commands: UV READ, UV STATUS, UV CAL LOW <UVI>, UV CAL HIGH <UVI>, UV CAL RESET");
    Serial.println("Also: DEMO ON, DEMO OFF, CLEAR, GAMMA_SIM, HELP");
  }
}

void pollSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(serialCommandBuffer);
      serialCommandBuffer = "";
    } else if (serialCommandBuffer.length() < 64) {
      serialCommandBuffer += c;
    }
  }
}

String makeDashboardJSON(const Reading &r, uint32_t seq) {
  String out;
  out.reserve(200);
  out = "{\"source\":\"";
  out += demoMode ? "SIMULATED" : "LIVE";
  out += "\",\"seq\":";
  out += String(seq);

  if (demoMode) {
    // Explicitly synthetic values for checking dashboard alert rules.
    // Which values trigger alerts depends on the server's actual thresholds.
    int phase = seq % 4;
    float uv = (phase == 1 || phase == 3) ? 12.0f : 2.0f;
    float mag = (phase == 2 || phase == 3) ? 25.0f : 0.4f;
    out += ",\"uv_index\":";
    out += String(uv, 1);
    out += ",\"field_delta_uT\":";
    out += String(mag, 2);
    out += ",\"temp_C\":";
    out += String(25.0f, 1);
  } else {
    // Only actual available measurements go in the LIVE packet.
    // NO fabricated zero values for missing sensors.
    if (isfinite(r.uvIndex)) {
      out += ",\"uv_index\":";
      out += String(r.uvIndex, 2);
    }
    if (r.magOK && isfinite(r.magneticDeltaUT)) {
      out += ",\"field_delta_uT\":";
      out += String(r.magneticDeltaUT, 2);
    }
    if (r.dhtOK) {
      out += ",\"temp_C\":";
      out += String(r.tempC, 1);
    }
  }

  out += "}";
  return out;
}

int sendJSON(const String &payload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HTTP: SKIPPED (Wi-Fi disconnected)");
    return -1;
  }
  if (String(TELEMETRY_URL).indexOf("YOUR-APP") >= 0 ||
      String(INGEST_KEY) == "PASTE_INGEST_KEY_HERE") {
    Serial.println("HTTP: SKIPPED (configure Vercel URL and INGEST_KEY)");
    return -2;
  }

  WiFiClientSecure client;
  if (ALLOW_INSECURE_HTTPS_DEMO) {
    client.setInsecure();  // BENCH ONLY: no server identity verification.
  } else if (strlen(ROOT_CA_PEM) > 100) {
    client.setCACert(ROOT_CA_PEM);
  } else {
    Serial.println("HTTP: BLOCKED (no CA certificate configured)");
    return -3;
  }

  HTTPClient http;
  http.setTimeout(4000);
  if (!http.begin(client, TELEMETRY_URL)) {
    Serial.println("HTTP: begin failed");
    return -4;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", INGEST_KEY);
  int code = http.POST(payload);
  Serial.printf("HTTP POST: %d\n", code);
  if (code > 0) {
    String response = http.getString();
    if (response.length() > 200) response = response.substring(0, 200);
    Serial.print("HTTP response: ");
    Serial.println(response);
  } else {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(code));
  }
  http.end();
  return code;
}

void csvOptional(File &f, float value, int digits) {
  f.print(",");
  if (isfinite(value)) f.print(value, digits);
}

void logToSD(const String &payload, const Reading &r, uint32_t seq) {
  if (!sdReady) return;
  File f = SD.open(JSONL_FILE, FILE_APPEND);
  if (f) {
    f.println(payload);
    f.close();
  } else Serial.println("SD: JSONL open failed");

  f = SD.open(RAW_CSV_FILE, FILE_APPEND);
  if (!f) {
    Serial.println("SD: raw CSV open failed");
    return;
  }
  if (f.size() == 0) {
    f.println("seq,time_ms,uv_biased_mv,uv_index_est,mag_x,mag_y,mag_z,"
              "field_delta_est_uT,acc_x_g,acc_y_g,acc_z_g,"
              "gyro_x_dps,gyro_y_dps,gyro_z_dps,temp_C,humidity,"
              "mag_ok,mpu_ok,dht_ok,ina_initialized,simulation_on");
  }
  f.print(seq);
  f.print(","); f.print(millis());
  csvOptional(f, r.uvMv, 1);
  csvOptional(f, r.uvIndex, 2);
  csvOptional(f, r.magOK ? (float)r.mx : NAN, 0);
  csvOptional(f, r.magOK ? (float)r.my : NAN, 0);
  csvOptional(f, r.magOK ? (float)r.mz : NAN, 0);
  csvOptional(f, r.magneticDeltaUT, 2);
  csvOptional(f, r.ax, 3); csvOptional(f, r.ay, 3);
  csvOptional(f, r.az, 3);
  csvOptional(f, r.gx, 2); csvOptional(f, r.gy, 2);
  csvOptional(f, r.gz, 2);
  csvOptional(f, r.tempC, 1);
  csvOptional(f, r.humidity, 1);
  f.print(","); f.print(r.magOK ? 1 : 0);
  f.print(","); f.print(r.mpuOK ? 1 : 0);
  f.print(","); f.print(r.dhtOK ? 1 : 0);
  f.print(","); f.print(inaReady ? 1 : 0);
  f.print(","); f.println(demoMode ? 1 : 0);
  f.close();
  Serial.println("SD: JSON and raw CSV saved");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n==============================");
  Serial.println("HELIOS SHIELD / FALCONEYE V4.1");
  Serial.println("JSON + DASHBOARD + UV CALIBRATION");
  Serial.println("==============================");

  analogReadResolution(12);
  analogSetPinAttenuation(UV_PIN, ADC_0db);
  Wire.begin(21, 22);
  Wire.setTimeOut(150);
  Wire.setClock(I2C_HZ);
  dht.begin();

  magReady = initializeMag();
  lastMagTryMs = millis();
  Serial.printf("MAG: %s\n", magReady ? "READY" : "FAILED");
  mpuReady = initializeMPU();
  Serial.printf("MPU: %s\n", mpuReady ? "READY" : "FAILED");
  inaReady = ina219.begin(&Wire);
  if (inaReady) ina219.setCalibration_32V_2A();
  Wire.setClock(I2C_HZ);
  Serial.printf("INA219: %s (I2C only; no load path)\n",
                inaReady ? "READY" : "FAILED");

  if (mpuReady) calibrateGyro();
  if (magReady) calibrateMagBaseline();
  initializeSD();

  preferencesReady = preferences.begin("falconeye", false);
  if (preferencesReady) {
    reserveSequenceBlock();
    loadUVCalibration();
  } else {
    Serial.println("WARNING: sequence persistence unavailable");
    nextSeq = 1;
    reservedSeqEnd = 0xFFFFFFFF;
  }

  initializeWiFi();
  Serial.println("Commands: UV READ / UV STATUS / UV CAL LOW 0 / UV CAL HIGH <UVI> / UV CAL RESET");
  Serial.println("Also: DEMO ON / DEMO OFF / GAMMA_SIM / HELP");
  Serial.println("UV Index omitted until TWO references; estimates are not reference-grade.");
  Serial.println("V4.1 READY");
  lastPacketMs = millis() - PACKET_INTERVAL_MS;
}

void loop() {
  pollSerialCommands();
  maintainWiFi();
  if (millis() - lastPacketMs < PACKET_INTERVAL_MS) {
    delay(5);
    return;
  }
  lastPacketMs = millis();

  Reading r = sampleSensors();
  uint32_t seq = getNextSequence();
  String payload = makeDashboardJSON(r, seq);

  Serial.println("\n--- FALCONEYE ---");
  Serial.printf("UV biased voltage: %.1f mV (junction only)\n", r.uvMv);
  if (isfinite(r.uvIndex)) {
    Serial.printf("UV Index estimate: %.2f (two-point reference)\n", r.uvIndex);
  } else {
    Serial.println("UV Index: unavailable (uncalibrated or outside reference span)");
  }
  if (r.magOK) {
    Serial.printf("MAG raw X:%d Y:%d Z:%d\n", r.mx, r.my, r.mz);
    if (isfinite(r.magneticDeltaUT))
      Serial.printf("Field delta: %.2f uT (uncalibrated estimate)\n",
                    r.magneticDeltaUT);
  } else Serial.println("MAG: unavailable");
  if (r.mpuOK) {
    Serial.printf("ACC: %.2f %.2f %.2f g\n", r.ax, r.ay, r.az);
    if (gyroCalibrated)
      Serial.printf("GYRO: %.2f %.2f %.2f deg/s\n", r.gx, r.gy, r.gz);
  } else Serial.println("MPU: unavailable");
  if (r.dhtOK) Serial.printf("TEMP: %.1f C | RH: %.1f %%\n", r.tempC, r.humidity);
  else Serial.println("DHT11: unavailable");
  Serial.printf("Wi-Fi: %s\n", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "OFFLINE");
  Serial.print("JSON: ");
  Serial.println(payload);

  logToSD(payload, r, seq);
  bool hasNumericReading = demoMode || r.dhtOK ||
      isfinite(r.uvIndex) ||
      (r.magOK && isfinite(r.magneticDeltaUT));
  if (hasNumericReading) {
    sendJSON(payload);
  } else {
    Serial.println("HTTP: SKIPPED (no valid numeric payload readings)");
  }
}