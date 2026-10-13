// ============================================================
//  OFFLINE PATIENT MONITORING DEVICE
//  CAREGIVER UNIT — ESP32 #2
//  Output: OLED Display | SD Card Logger | Buzzer Alerts
//  Protocol: ESP-NOW (Receiver) + Optional Bluetooth
//
//  PlatformIO Project — place this file in src/main.cpp
//  See platformio.ini for board config and library dependencies
// ============================================================
//  WIRING — CAREGIVER UNIT (ESP32 #2)
//  OLED SSD1306 (128x64):
//    VCC  → 3.3V      GND  → GND
//    SDA  → GPIO 21   SCL  → GPIO 22
//
//  SD Card Module (SPI):
//    VCC  → 3.3V      GND  → GND
//    MOSI → GPIO 23   MISO → GPIO 19
//    SCK  → GPIO 18   CS   → GPIO 5
//
//  DS3231 RTC:
//    VCC  → 3.3V      GND  → GND
//    SDA  → GPIO 21  (shared I2C)
//    SCL  → GPIO 22  (shared I2C)
//
//  BUZZER (Caregiver alert):
//    +    → GPIO 26   -    → GND
//
//  LED Indicators (optional):
//    GREEN  → GPIO 27  (Normal)
//    YELLOW → GPIO 14  (Warning)
//    RED    → GPIO 12  (Critical)
// ============================================================

#include <Arduino.h>           // Required in PlatformIO (not needed in Arduino IDE)
#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <BluetoothSerial.h>

// ── PIN DEFINITIONS ─────────────────────────────────────────
#define SD_CS_PIN     5
#define BUZZER_PIN    26
#define LED_GREEN     27
#define LED_YELLOW    14
#define LED_RED       12

// ── OLED ────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── RTC ─────────────────────────────────────────────────────
RTC_DS3231 rtc;
//Hello
// test 2

// ── BLUETOOTH ───────────────────────────────────────────────
BluetoothSerial SerialBT;

// ── DATA PACKET STRUCTURE (must match Patient Unit exactly) ──
typedef struct PatientData {
  float    heartRate;
  float    spO2;
  int      ecgValue;
  bool     ecgLeadOff;
  float    accelMag;
  float    gyroMag;
  bool     fallDetected;
  float    restlessness;
  uint8_t  alertLevel;    // 0=Normal 1=Warning 2=Critical
} PatientData;

// ── GLOBAL STATE ─────────────────────────────────────────────
PatientData   latestData;
bool          dataReceived  = false;
bool          sdAvailable   = false;
unsigned long lastReceived  = 0;
unsigned long lastDisplay   = 0;
unsigned long buzzerStart   = 0;
bool          buzzerOn      = false;
int           displayPage   = 0;       // 0=vitals 1=motion 2=status
unsigned long pageTimer     = 0;
bool          flashState    = false;
unsigned long flashTimer    = 0;

// ── ECG SPIKE DETECTION STATE ────────────────────────────────
#define ECG_HISTORY   20
int  ecgHistory[ECG_HISTORY];
int  ecgHistIdx  = 0;
bool ecgHistFull = false;

// ── LOG FILE ─────────────────────────────────────────────────
String logFileName = "";

// ── FORWARD DECLARATIONS ─────────────────────────────────────
void updateDisplay(uint8_t level, String reason, String ts);
void triggerAlert(uint8_t level, String reason);
void logToSD(PatientData &d, String reason, DateTime &dt);
bool detectECGAnomaly(int newVal);
String getTimestamp(DateTime &dt);
void sendBluetooth(PatientData &d, String &ts);
void setLED(uint8_t alertLevel);

// ── ESP-NOW RECEIVE CALLBACK ─────────────────────────────────
// Use the ESP32 Arduino core callback signature supported by this project.
// Older cores pass the sender MAC as const uint8_t *mac_addr.
void onDataReceived(const uint8_t *mac_addr, const uint8_t *data, int len) {
  (void)mac_addr;  // suppress unused-variable warning if MAC not needed

  if (len == sizeof(PatientData)) {
    memcpy(&latestData, data, sizeof(PatientData));
    dataReceived = true;
    lastReceived = millis();
  }
}

// ── SETUP ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED,    LOW);

  // ── OLED Init ─────────────────────────────────────────────
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[ERROR] OLED not found.");
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Patient Monitor");
    display.println("Initializing...");
    display.display();
    Serial.println("[OK] OLED initialized.");
  }

  // ── RTC Init ──────────────────────────────────────────────
  if (!rtc.begin()) {
    Serial.println("[WARN] RTC not found. Using millis() for timestamps.");
  } else {
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    Serial.println("[OK] RTC initialized.");
  }

  // ── SD Card Init ──────────────────────────────────────────
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[WARN] SD card not found. Logging disabled.");
    sdAvailable = false;
  } else {
    sdAvailable = true;
    DateTime now = rtc.now();
    logFileName  = "/" + String(now.year())
                       + String(now.month())
                       + String(now.day())
                       + "_log.csv";
    if (!SD.exists(logFileName)) {
      File f = SD.open(logFileName, FILE_WRITE);
      if (f) {
        f.println("Timestamp,HeartRate,SpO2,ECG,ECGLeadOff,"
                  "AccelMag,GyroMag,FallDetected,Restlessness,"
                  "AlertLevel,Reason");
        f.close();
      }
    }
    Serial.println("[OK] SD card ready. Log: " + logFileName);
  }

  // ── Bluetooth Init ────────────────────────────────────────
  SerialBT.begin("PatientMonitor");
  Serial.println("[OK] Bluetooth started as 'PatientMonitor'.");

  // ── ESP-NOW Init ──────────────────────────────────────────
  WiFi.mode(WIFI_STA);
  Serial.print("[INFO] Caregiver MAC: ");
  Serial.println(WiFi.macAddress());   // <-- Copy this MAC to Patient Unit!

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed.");
    while (1) { delay(100); }
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("[OK] ESP-NOW initialized. Waiting for Patient Unit...");

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Caregiver Unit");
  display.println("MAC:");
  display.println(WiFi.macAddress());
  display.println("Waiting for");
  display.println("Patient Unit...");
  display.display();
}

// ── MAIN LOOP ────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // ── Connection timeout warning ────────────────────────────
  if (dataReceived && (now - lastReceived) > 5000) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("!! NO SIGNAL !!");
    display.println("Check Patient Unit");
    display.display();
    setLED(1);
    return;
  }

  if (!dataReceived) return;

  DateTime dt = rtc.now();
  String   ts = getTimestamp(dt);

  // ── 1. ECG Anomaly Detection ──────────────────────────────
  bool ecgAnomaly = false;
  if (!latestData.ecgLeadOff) {
    ecgAnomaly = detectECGAnomaly(latestData.ecgValue);
  }

  // ── 2. Sleep / Apnea Detection ────────────────────────────
  bool possibleApnea = (latestData.accelMag < 0.1f &&
                        latestData.spO2 > 0 &&
                        latestData.spO2 < 94.0f);
  bool poorSleep     = (latestData.restlessness > 0.15f);

  // ── 3. Build alert reason string ─────────────────────────
  String  alertReason = "Normal";
  uint8_t level       = latestData.alertLevel;

  if (latestData.fallDetected) {
    alertReason = "FALL EMERGENCY";
    level = 2;
  } else if (latestData.spO2 > 0 && latestData.spO2 < 90.0f) {
    alertReason = "Hypoxia - Critical SpO2";
    level = 2;
  } else if (latestData.spO2 > 0 && latestData.spO2 < 95.0f) {
    alertReason = "Low Oxygen";
    level = 1;
  } else if (latestData.heartRate > 120.0f) {
    alertReason = "Tachycardia";
    level = 1;
  } else if (latestData.heartRate > 0 && latestData.heartRate < 50.0f) {
    alertReason = "Bradycardia";
    level = 1;
  } else if (ecgAnomaly) {
    alertReason = "ECG Anomaly";
    level = 1;
  } else if (possibleApnea) {
    alertReason = "Possible Apnea";
    level = 1;
  } else if (poorSleep) {
    alertReason = "Poor Sleep Pattern";
    level = 0;
  }

  // ── 4. Trigger Alert ─────────────────────────────────────
  if (level >= 1) {
    triggerAlert(level, alertReason);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }

  // ── 5. LED Indicators ────────────────────────────────────
  setLED(level);

  // ── 6. Buzzer Auto-off (3 s) ─────────────────────────────
  if (buzzerOn && (now - buzzerStart) > 3000) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
  }

  // ── 7. OLED Display (rotate pages every 4 s) ─────────────
  if (now - pageTimer > 4000) {
    displayPage = (displayPage + 1) % 3;
    pageTimer   = now;
  }
  if (now - lastDisplay > 250) {
    lastDisplay = now;
    updateDisplay(level, alertReason, ts);
  }

  // ── 8. Flash for critical alerts ──────────────────────────
  if (level == 2 && (now - flashTimer > 500)) {
    flashState = !flashState;
    flashTimer = now;
  }

  // ── 9. SD Logging (every 1 s) ────────────────────────────
  static unsigned long lastLog = 0;
  if (now - lastLog > 1000) {
    lastLog = now;
    logToSD(latestData, alertReason, dt);
  }

  // ── 10. Bluetooth Output (every 1 s) ─────────────────────
  if (SerialBT.hasClient()) {
    static unsigned long lastBT = 0;
    if (now - lastBT > 1000) {
      lastBT = now;
      sendBluetooth(latestData, ts);
    }
  }

  // ── 11. Handle Bluetooth incoming commands ────────────────
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();
    if (cmd == "GETLOG" && sdAvailable) {
      File f = SD.open(logFileName, FILE_READ);
      if (f) {
        while (f.available()) { SerialBT.write(f.read()); }
        f.close();
        SerialBT.println("[END OF LOG]");
      }
    } else if (cmd == "STATUS") {
      SerialBT.printf("HR:%.0f SpO2:%.0f Fall:%d Alert:%d\n",
        latestData.heartRate, latestData.spO2,
        latestData.fallDetected, level);
    }
  }

  // ── Serial debug ─────────────────────────────────────────
  Serial.printf("[RX] HR:%.0f SpO2:%.0f ECG:%d Fall:%d Alert:%s\n",
    latestData.heartRate, latestData.spO2,
    latestData.ecgValue, latestData.fallDetected,
    alertReason.c_str());
}

// ── DISPLAY UPDATE ───────────────────────────────────────────
void updateDisplay(uint8_t level, String reason, String ts) {
  display.clearDisplay();

  if (level == 2 && flashState) {
    display.fillRect(0, 0, SCREEN_WIDTH, 16, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(10, 4);
    display.print("!! " + reason + " !!");
    display.setTextColor(SSD1306_WHITE);
  } else if (level == 2) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("!! ");
    display.print(reason);
    display.print(" !!");
  }

  if (displayPage == 0) {
    // ── Page 0: Vitals ──────────────────────────────────────
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print("HR: ");
    display.setTextSize(2);
    display.print((int)latestData.heartRate);
    display.setTextSize(1);
    display.println(" BPM");

    display.setCursor(0, 38);
    display.print("SpO2: ");
    display.setTextSize(2);
    display.print((int)latestData.spO2);
    display.setTextSize(1);
    display.println(" %");

    display.setCursor(0, 56);
    if      (latestData.spO2 >= 95) display.print("Status: Normal");
    else if (latestData.spO2 >= 90) display.print("Status: Warning");
    else if (latestData.spO2 >   0) display.print("Status: CRITICAL");
    else                             display.print("Sensor: No finger");

  } else if (displayPage == 1) {
    // ── Page 1: ECG + Motion ────────────────────────────────
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print("ECG: ");
    if (latestData.ecgLeadOff) display.print("Leads off");
    else                        display.print(latestData.ecgValue);

    display.setCursor(0, 30);
    display.print("Accel: ");
    display.print(latestData.accelMag, 2);
    display.print("g");

    display.setCursor(0, 42);
    display.print("Fall: ");
    display.print(latestData.fallDetected ? "DETECTED!" : "None");

    display.setCursor(0, 54);
    display.print("Sleep: ");
    display.print(latestData.restlessness > 0.15f ? "Restless" : "OK");

  } else {
    // ── Page 2: Status + Time ────────────────────────────────
    display.setTextSize(1);
    display.setCursor(0, 18);
    display.print("Alert: ");
    if      (latestData.alertLevel == 0) display.print("Normal");
    else if (latestData.alertLevel == 1) display.print("WARNING");
    else                                  display.print("CRITICAL");

    display.setCursor(0, 30);
    display.print(reason.substring(0, 20));

    display.setCursor(0, 42);
    display.print("Time: ");
    display.print(ts.substring(0, 8));  // HH:MM:SS

    display.setCursor(0, 54);
    display.print("SD:");
    display.print(sdAvailable ? "OK " : "ERR");
    display.print(" BT:");
    display.print(SerialBT.hasClient() ? "Connected" : "None");
  }

  display.display();
}

// ── TRIGGER ALERT ────────────────────────────────────────────
void triggerAlert(uint8_t level, String reason) {
  unsigned long now = millis();
  if (level == 2) {
    if (!buzzerOn) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerOn    = true;
      buzzerStart = now;
      Serial.println("[CRITICAL] " + reason);
    }
  } else if (level == 1 && !buzzerOn) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("[WARNING] " + reason);
  }
}

// ── SET LED INDICATORS ───────────────────────────────────────
void setLED(uint8_t level) {
  digitalWrite(LED_GREEN,  level == 0 ? HIGH : LOW);
  digitalWrite(LED_YELLOW, level == 1 ? HIGH : LOW);
  digitalWrite(LED_RED,    level == 2 ? HIGH : LOW);
}

// ── ECG ANOMALY DETECTION ────────────────────────────────────
bool detectECGAnomaly(int newVal) {
  ecgHistory[ecgHistIdx] = newVal;
  ecgHistIdx = (ecgHistIdx + 1) % ECG_HISTORY;
  if (ecgHistIdx == 0) ecgHistFull = true;

  int count = ecgHistFull ? ECG_HISTORY : ecgHistIdx;
  if (count < 5) return false;

  float sum = 0, sumSq = 0;
  for (int i = 0; i < count; i++) {
    sum   += ecgHistory[i];
    sumSq += (float)ecgHistory[i] * ecgHistory[i];
  }
  float mean   = sum / count;
  float stddev = sqrt((sumSq / count) - (mean * mean));
  return (abs(newVal - mean) > 2.0f * stddev && stddev > 50.0f);
}

// ── SD CARD LOGGING ──────────────────────────────────────────
void logToSD(PatientData &d, String reason, DateTime &dt) {
  if (!sdAvailable) return;
  File f = SD.open(logFileName, FILE_APPEND);
  if (!f) {
    Serial.println("[WARN] Could not open log file.");
    return;
  }
  String ts = getTimestamp(dt);
  f.printf("%s,%.1f,%.1f,%d,%d,%.3f,%.3f,%d,%.4f,%d,%s\n",
    ts.c_str(),
    d.heartRate, d.spO2, d.ecgValue, d.ecgLeadOff,
    d.accelMag,  d.gyroMag, d.fallDetected,
    d.restlessness, d.alertLevel, reason.c_str());
  f.close();
}

// ── BLUETOOTH OUTPUT ─────────────────────────────────────────
void sendBluetooth(PatientData &d, String &ts) {
  SerialBT.printf(
    "{\"ts\":\"%s\",\"hr\":%.0f,\"spo2\":%.0f,"
    "\"fall\":%d,\"alert\":%d,\"ecg\":%d,\"rest\":%.3f}\n",
    ts.c_str(), d.heartRate, d.spO2,
    d.fallDetected, d.alertLevel, d.ecgValue, d.restlessness);
}

// ── TIMESTAMP HELPER ─────────────────────────────────────────
String getTimestamp(DateTime &dt) {
  char buf[20];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
    dt.year(), dt.month(), dt.day(),
    dt.hour(), dt.minute(), dt.second());
  return String(buf);
}