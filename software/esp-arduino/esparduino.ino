// // // // // ESP32 + MCP2515 loopback + SSD1306 OLED
// // // // // Serial input line example:
// // // // // t=1513550000000,rpm=2200,speed=52.4,thr=33.5
// // // // //
// // // // // Libraries (install via Library Manager):
// // // // // - MCP_CAN_lib by Cory J. Fowler (or equivalent "MCP_CAN_lib")
// // // // // - Adafruit SSD1306
// // // // // - Adafruit GFX
// // // // //
// // // // // Wiring:
// // // // // MCP2515: CS=GPIO5, INT=GPIO27, SCK=18, MOSI=23, MISO=19, VCC=5V, GND
// // // // // OLED 0.96" SSD1306 I2C: SDA=21, SCL=22, VCC=3.3V, GND

// // // // // ESP32 ↔ MCP2515 (SPI):

// // // // // SCK → GPIO18

// // // // // MOSI → GPIO23

// // // // // MISO → GPIO19

// // // // // CS → choose e.g., GPIO5 (define in code)

// // // // // INT → choose e.g., GPIO27 (attach interrupt or poll)

// // // // // VCC → 5V (common for MCP2515+TJA1050 boards)

// // // // // GND → GND (shared with ESP32)

// // // // // Note: MCP2515/TJA1050 are 5V; ESP32 is 3.3V. Many MCP2515 modules accept 3.3V logic on SPI; if yours doesn’t, add a level shifter for CS and INT. Keep SPI wires short.

// // // // // ESP32 ↔ OLED (I2C):

// // // // // SDA → GPIO21, SCL → GPIO22

// // // // // VCC → 3.3V (most 0.96" OLEDs), GND → GND

// // // // #include <SPI.h>
// // // // #include <mcp_can.h>
// // // // #include <Wire.h>
// // // // #include <Adafruit_GFX.h>
// // // // #include <Adafruit_SSD1306.h>

// // // // // OLED config
// // // // #define SCREEN_WIDTH 128
// // // // #define SCREEN_HEIGHT 64
// // // // #define OLED_RESET    -1
// // // // Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// // // // // MCP2515 config
// // // // #define CAN_CS_PIN   5
// // // // #define CAN_INT_PIN 27
// // // // MCP_CAN CAN(CAN_CS_PIN);

// // // // // Data IDs
// // // // const uint16_t ID_RPM     = 0x100;
// // // // const uint16_t ID_SPEED   = 0x101;
// // // // const uint16_t ID_THR     = 0x102;

// // // // // State
// // // // float g_rpm = 0.0f;
// // // // float g_speed_kmh = 0.0f;
// // // // float g_thr_pct = 0.0f;

// // // // float prev_thr_pct = 0.0f;
// // // // unsigned long prev_thr_ms = 0;

// // // // bool can_ok_last = false;
// // // // String alert_flags;

// // // // // Helpers: parse key=value from a CSV-like line
// // // // bool parseField(const String& line, const String& key, float& outVal) {
// // // //   int kpos = line.indexOf(key + "=");
// // // //   if (kpos < 0) return false;
// // // //   int start = kpos + key.length() + 1;
// // // //   int end = line.indexOf(',', start);
// // // //   if (end < 0) end = line.length();
// // // //   String token = line.substring(start, end);
// // // //   token.trim();
// // // //   if (token.length() == 0) return false;
// // // //   outVal = token.toFloat();
// // // //   return true;
// // // // }

// // // // // Pack helpers (little-endian)
// // // // static inline void put_u16_le(uint8_t* b, uint16_t v) {
// // // //   b[0] = (uint8_t)(v & 0xFF);
// // // //   b[1] = (uint8_t)((v >> 8) & 0xFF);
// // // // }

// // // // bool canSendAndEcho(uint16_t id, const uint8_t* data, uint8_t len) {
// // // //   // Transmit
// // // //   byte sndStat = CAN.sendMsgBuf(id, 0, len, (byte*)data);
// // // //   if (sndStat != CAN_OK) {
// // // //     Serial.printf("CAN TX fail id=0x%03X status=%d\n", id, sndStat);
// // // //     return false;
// // // //   }

// // // //   // In loopback, MCP2515 places TX into RX; poll for a short time
// // // //   unsigned long t0 = millis();
// // // //   while (millis() - t0 < 10) { // up to ~10ms
// // // //     if (digitalRead(CAN_INT_PIN) == LOW) {
// // // //       unsigned long rxId = 0;
// // // //       byte dlc = 0;
// // // //       byte buf[8];
// // // //       if (CAN.readMsgBuf(&rxId, &dlc, buf) == CAN_OK) {
// // // //         if ((uint16_t)rxId == id && dlc == len) {
// // // //           // Compare payloads
// // // //           for (uint8_t i = 0; i < len; i++) {
// // // //             if (buf[i] != data[i]) {
// // // //               Serial.printf("CAN echo mismatch id=0x%03lX byte %u\n", rxId, i);
// // // //               return false;
// // // //             }
// // // //           }
// // // //           return true;
// // // //         } else {
// // // //           // Drain any stray frames
// // // //         }
// // // //       }
// // // //     }
// // // //     // small yield
// // // //     delayMicroseconds(200);
// // // //   }
// // // //   Serial.printf("CAN echo timeout id=0x%03X\n", id);
// // // //   return false;
// // // // }

// // // // void updateAlerts() {
// // // //   alert_flags = "";
// // // //   // Overspeed > 30 km/h
// // // //   if (g_speed_kmh > 30.0f) {
// // // //     if (alert_flags.length()) alert_flags += " ";
// // // //     alert_flags += "SPD";
// // // //   }
// // // //   // High RPM > 3500
// // // //   if (g_rpm > 3500.0f) {
// // // //     if (alert_flags.length()) alert_flags += " ";
// // // //     alert_flags += "RPM";
// // // //   }
// // // //   // Harsh throttle: delta > 20% within 500 ms
// // // //   unsigned long now = millis();
// // // //   if (prev_thr_ms != 0 && (now - prev_thr_ms) <= 500) {
// // // //     if (fabs(g_thr_pct - prev_thr_pct) > 20.0f) {
// // // //       if (alert_flags.length()) alert_flags += " ";
// // // //       alert_flags += "THR";
// // // //     }
// // // //   }
// // // //   prev_thr_pct = g_thr_pct;
// // // //   prev_thr_ms = now;

// // // //   if (alert_flags.length() == 0) alert_flags = "--";
// // // // }

// // // // void drawOLED(bool can_ok) {
// // // //   display.clearDisplay();
// // // //   display.setTextSize(1);
// // // //   display.setTextColor(SSD1306_WHITE);

// // // //   display.setCursor(0, 0);
// // // //   display.printf("Spd: %.1f km/h", g_speed_kmh);

// // // //   display.setCursor(0, 12);
// // // //   display.printf("RPM: %.0f", g_rpm);

// // // //   display.setCursor(0, 24);
// // // //   display.printf("Thr: %.1f %%", g_thr_pct);

// // // //   display.setCursor(0, 36);
// // // //   display.print("Alert: ");
// // // //   display.print(alert_flags);

// // // //   display.setCursor(0, 48);
// // // //   display.print("CAN: ");
// // // //   display.print(can_ok ? "OK" : "ERR");

// // // //   display.display();
// // // // }

// // // // void setup() {
// // // //   Serial.begin(115200);
// // // //   delay(100);

// // // //   // OLED init
// // // //   Wire.begin(21, 22);
// // // //   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
// // // //     Serial.println("SSD1306 init failed");
// // // //   } else {
// // // //     display.clearDisplay();
// // // //     display.setTextSize(1);
// // // //     display.setTextColor(SSD1306_WHITE);
// // // //     display.setCursor(0, 0);
// // // //     display.println("OLED ready");
// // // //     display.display();
// // // //   }

// // // //   // MCP2515 init
// // // //   pinMode(CAN_INT_PIN, INPUT_PULLUP);
// // // //   SPI.begin(18, 19, 23); // SCK, MISO, MOSI (CS handled by lib)
// // // //   // 500 kbps, 16MHz crystal typical for MCP2515 boards
// // // //   if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
// // // //     Serial.println("CAN init ok");
// // // //   } else {
// // // //     Serial.println("CAN init failed");
// // // //     while (1) { delay(1000); }
// // // //   }

// // // //   // Enable loopback mode
// // // //   CAN.setMode(MCP_LOOPBACK);
// // // //   delay(10);

// // // //   // Small banner
// // // //   Serial.println("Ready. Send lines like:");
// // // //   Serial.println("t=1513550000000,rpm=2200,speed=52.4,thr=33.5");
// // // // }

// // // // String line;

// // // // void loop() {
// // // //   // Read one complete line from Serial
// // // //   while (Serial.available()) {
// // // //     char c = (char)Serial.read();
// // // //     if (c == '\r') continue;
// // // //     if (c == '\n') {
// // // //       if (line.length() > 0) {
// // // //         processLine(line);
// // // //         line = "";
// // // //       }
// // // //     } else {
// // // //       line += c;
// // // //       if (line.length() > 256) line = ""; // guard
// // // //     }
// // // //   }
// // // // }

// // // // // Process one CSV-like line
// // // // void processLine(const String& s) {
// // // //   float rpm, spd, thr;
// // // //   bool ok_rpm = parseField(s, "rpm", rpm);
// // // //   bool ok_spd = parseField(s, "speed", spd);
// // // //   bool ok_thr = parseField(s, "thr", thr);

// // // //   if (!ok_rpm || !ok_spd || !ok_thr) {
// // // //     Serial.println("Parse error: expected rpm,speed,thr");
// // // //     return;
// // // //   }

// // // //   // Clamp and store
// // // //   if (rpm < 0) rpm = 0;
// // // //   if (rpm > 65535) rpm = 65535;
// // // //   if (spd < 0) spd = 0;
// // // //   if (spd > 6553.5) spd = 6553.5; // 0.1 km/h scaling cap
// // // //   if (thr < 0) thr = 0;
// // // //   if (thr > 100) thr = 100;

// // // //   g_rpm = rpm;
// // // //   g_speed_kmh = spd;
// // // //   g_thr_pct = thr;

// // // //   // Build payloads
// // // //   uint8_t b_rpm[2];
// // // //   uint16_t rpm_u16 = (uint16_t)roundf(rpm);
// // // //   put_u16_le(b_rpm, rpm_u16);

// // // //   uint8_t b_spd[2];
// // // //   uint16_t spd_u16 = (uint16_t)roundf(spd * 10.0f); // 0.1 km/h
// // // //   put_u16_le(b_spd, spd_u16);

// // // //   uint8_t b_thr[1];
// // // //   uint8_t thr_u8 = (uint8_t)roundf(thr * 2.0f); // 0.5% steps
// // // //   b_thr[0] = thr_u8;

// // // //   // Send and echo in loopback
// // // //   bool ok1 = canSendAndEcho(ID_RPM,   b_rpm, 2);
// // // //   bool ok2 = canSendAndEcho(ID_SPEED, b_spd, 2);
// // // //   bool ok3 = canSendAndEcho(ID_THR,   b_thr, 1);
// // // //   bool can_ok = ok1 && ok2 && ok3;
// // // //   can_ok_last = can_ok;

// // // //   // Alerts + OLED
// // // //   updateAlerts();
// // // //   drawOLED(can_ok);

// // // //   // Debug
// // // //   Serial.printf("rpm=%.0f speed=%.1f thr=%.1f | CAN:%s | alerts:%s\n",
// // // //                 g_rpm, g_speed_kmh, g_thr_pct,
// // // //                 can_ok ? "OK" : "ERR",
// // // //                 alert_flags.c_str());
// // // // }
// // // // ESP32 + MCP2515 CAN Receiver (normal mode) + SSD1306 OLED + WebServer
// // // // Receives CAN frames from Nano sender and displays on OLED + web dashboard
// // // // Pins: MCP2515 CS=GPIO5, INT=GPIO27, SCK=18, MOSI=23, MISO=19
// // // // OLED: I2C SDA=21, SCL=22, VCC=3.3V, GND
// // // // Wi‑Fi: fill in your SSID/PASS below

// // // #include <SPI.h>
// // // #include <mcp_can.h>
// // // #include <Wire.h>
// // // #include <Adafruit_GFX.h>
// // // #include <Adafruit_SSD1306.h>
// // // #include <WiFi.h>
// // // #include <WebServer.h>

// // // #define CAN_CS_PIN   5
// // // #define CAN_INT_PIN 27

// // // MCP_CAN CAN(CAN_CS_PIN);

// // // #define SCREEN_WIDTH 128
// // // #define SCREEN_HEIGHT 64
// // // #define OLED_RESET    -1
// // // Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// // // uint8_t oled_addr = 0x3C;

// // // const uint16_t ID_RPM   = 0x100;
// // // const uint16_t ID_SPEED = 0x101;
// // // const uint16_t ID_THR   = 0x102;

// // // // Live state
// // // volatile uint16_t rpm_u16 = 0;
// // // volatile uint16_t spd_u16 = 0; // 0.1 km/h
// // // volatile uint8_t  thr_u8  = 0; // 0.5 %

// // // float g_rpm=0, g_speed=0, g_thr=0;
// // // String alert_flags="--";

// // // // Thresholds (adjust as needed)
// // // float THRESH_SPEED = 80.0f;
// // // float THRESH_RPM   = 3500.0f;
// // // float THRESH_THR_JUMP = 20.0f;
// // // unsigned long THR_WINDOW_MS = 500;
// // // float prev_thr = 0.0f;
// // // unsigned long prev_thr_ms = 0;

// // // unsigned long lastOLED=0;
// // // unsigned long lastCompute=0;
// // // unsigned long lastCANrx=0;

// // // // Wi‑Fi + Web
// // // const char* WIFI_SSID = "YOUR_SSID";      // ← CHANGE THIS
// // // const char* WIFI_PASS = "YOUR_PASSWORD";  // ← CHANGE THIS
// // // WebServer server(80);

// // // String htmlPage() {
// // //   String s;
// // //   s += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
// // //   s += "<meta http-equiv='refresh' content='1'>";
// // //   s += "<style>body{font-family:Arial;margin:16px;background:#f0f0f0} .v{font-size:28px;font-weight:bold} .ok{color:green} .err{color:red} .card{border:1px solid #ccc;padding:16px;border-radius:8px;margin-bottom:12px;background:white;box-shadow:2px 2px 8px rgba(0,0,0,0.1)}</style>";
// // //   s += "<title>CAN Telemetry</title></head><body>";
// // //   s += "<h2>ESP32 CAN Telemetry Dashboard</h2>";
// // //   s += "<div class='card'><div class='v'>Speed: " + String(g_speed,1) + " km/h</div>";
// // //   s += "<div class='v'>RPM: " + String(g_rpm,0) + "</div>";
// // //   s += "<div class='v'>Throttle: " + String(g_thr,1) + " %</div>";
// // //   s += "<div style='margin-top:12px'>Alerts: <b class='" + String(alert_flags=="--"?"ok":"err") + "'>" + alert_flags + "</b></div></div>";
// // //   s += "<div class='card'><small>CAN IDs: 0x100=RPM, 0x101=Speed(0.1km/h), 0x102=Throttle(0.5%)</small></div>";
// // //   s += "<div class='card'><small>Last CAN RX: " + String((millis()-lastCANrx)/1000.0,1) + "s ago</small></div>";
// // //   s += "</body></html>";
// // //   return s;
// // // }

// // // void handleRoot() {
// // //   server.send(200, "text/html", htmlPage());
// // // }

// // // static inline uint16_t get_u16_le(const uint8_t* b) {
// // //   return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
// // // }

// // // void updateAlertsCompute() {
// // //   g_rpm = rpm_u16;
// // //   g_speed = spd_u16 / 10.0f;
// // //   g_thr = thr_u8 / 2.0f;

// // //   String flags = "";
// // //   if (g_speed > THRESH_SPEED) { if (flags.length()) flags += " "; flags += "SPD"; }
// // //   if (g_rpm > THRESH_RPM)     { if (flags.length()) flags += " "; flags += "RPM"; }

// // //   unsigned long now = millis();
// // //   if (prev_thr_ms != 0 && (now - prev_thr_ms) <= THR_WINDOW_MS) {
// // //     if (fabs(g_thr - prev_thr) > THRESH_THR_JUMP) {
// // //       if (flags.length()) flags += " ";
// // //       flags += "THR";
// // //     }
// // //   }
// // //   prev_thr = g_thr;
// // //   prev_thr_ms = now;

// // //   alert_flags = flags.length() ? flags : String("--");
// // // }

// // // void drawOLED() {
// // //   display.clearDisplay();
// // //   display.setTextSize(1);
// // //   display.setTextColor(SSD1306_WHITE);

// // //   display.setCursor(0,0);
// // //   display.printf("Spd: %.1f km/h", g_speed);
// // //   display.setCursor(0,12);
// // //   display.printf("RPM: %.0f", g_rpm);
// // //   display.setCursor(0,24);
// // //   display.printf("Thr: %.1f %%", g_thr);
// // //   display.setCursor(0,36);
// // //   display.print("Alert: ");
// // //   display.print(alert_flags);
  
// // //   // Show last CAN receive time
// // //   display.setCursor(0,50);
// // //   unsigned long ago = (millis() - lastCANrx) / 1000;
// // //   display.printf("CAN: %lus ago", ago);
  
// // //   display.display();
// // // }

// // // void setup() {
// // //   Serial.begin(115200);
// // //   delay(100);
// // //   Serial.println("[ESP32] Starting...");

// // //   // OLED init
// // //   Wire.begin(21,22);
// // //   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
// // //     Serial.println("[ESP32] Trying OLED at 0x3D...");
// // //     if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
// // //       Serial.println("[ESP32] OLED init failed");
// // //     } else {
// // //       oled_addr = 0x3D;
// // //     }
// // //   } else {
// // //     oled_addr = 0x3C;
// // //   }
// // //   display.clearDisplay();
// // //   display.setTextSize(1);
// // //   display.setTextColor(SSD1306_WHITE);
// // //   display.setCursor(0,0);
// // //   display.printf("OLED @0x%02X", oled_addr);
// // //   display.setCursor(0,12);
// // //   display.print("CAN init...");
// // //   display.display();

// // //   // CAN init (normal mode to receive from Nano)
// // //   pinMode(CAN_INT_PIN, INPUT_PULLUP);
// // //   SPI.begin(18,19,23);
  
// // //   // Adjust to MCP_16MHZ if your module uses 16 MHz crystal
// // //   if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
// // //     Serial.println("[ESP32] CAN init OK");
// // //   } else {
// // //     Serial.println("[ESP32] CAN init FAIL - check wiring/crystal");
// // //     display.clearDisplay();
// // //     display.setCursor(0,0);
// // //     display.print("CAN FAIL!");
// // //     display.display();
// // //     while (1) { delay(1000); }
// // //   }
  
// // //   CAN.setMode(MCP_NORMAL); // ← Normal mode to receive from bus
// // //   Serial.println("[ESP32] CAN in NORMAL mode");

// // //   // Wi‑Fi + Web
// // //   WiFi.mode(WIFI_STA);
// // //   WiFi.begin(WIFI_SSID, WIFI_PASS);
// // //   Serial.print("[ESP32] WiFi connecting");
  
// // //   display.clearDisplay();
// // //   display.setCursor(0,0);
// // //   display.print("WiFi...");
// // //   display.display();
  
// // //   int attempts = 0;
// // //   while (WiFi.status() != WL_CONNECTED && attempts < 20) {
// // //     delay(500);
// // //     Serial.print(".");
// // //     attempts++;
// // //   }
  
// // //   if (WiFi.status() == WL_CONNECTED) {
// // //     Serial.println();
// // //     Serial.print("[ESP32] IP: ");
// // //     Serial.println(WiFi.localIP());
    
// // //     display.clearDisplay();
// // //     display.setCursor(0,0);
// // //     display.print("IP:");
// // //     display.setCursor(0,12);
// // //     display.print(WiFi.localIP());
// // //     display.display();
// // //   } else {
// // //     Serial.println("\n[ESP32] WiFi failed, continuing without web");
// // //     display.clearDisplay();
// // //     display.setCursor(0,0);
// // //     display.print("WiFi FAIL");
// // //     display.display();
// // //   }

// // //   server.on("/", handleRoot);
// // //   server.begin();
// // //   Serial.println("[ESP32] Web server started");
  
// // //   delay(2000);
// // //   lastCANrx = millis();
// // // }

// // // void loop() {
// // //   // Receive CAN frames
// // //   if (CAN.checkReceive() == CAN_MSGAVAIL) {
// // //     unsigned long rxId = 0;
// // //     byte dlc = 0;
// // //     byte buf[8];
// // //     if (CAN.readMsgBuf(&rxId, &dlc, buf) == CAN_OK) {
// // //       lastCANrx = millis();
// // //       switch (rxId) {
// // //         case ID_RPM:
// // //           if (dlc >= 2) rpm_u16 = get_u16_le(buf);
// // //           break;
// // //         case ID_SPEED:
// // //           if (dlc >= 2) spd_u16 = get_u16_le(buf);
// // //           break;
// // //         case ID_THR:
// // //           if (dlc >= 1) thr_u8 = buf[0];
// // //           break;
// // //         default:
// // //           break;
// // //       }
// // //       Serial.printf("[ESP32] RX: ID=0x%03lX DLC=%d\n", rxId, dlc);
// // //     }
// // //   }

// // //   // Compute alerts and update OLED at moderate rate
// // //   unsigned long now = millis();
// // //   if (now - lastCompute >= 100) { // 10 Hz compute
// // //     updateAlertsCompute();
// // //     lastCompute = now;
// // //   }
// // //   if (now - lastOLED >= 200) { // 5 Hz display
// // //     drawOLED();
// // //     lastOLED = now;
// // //   }

// // //   server.handleClient();
// // // }



// // // ESP32 + MCP2515 CAN Receiver (normal mode) + SSD1306 OLED + WebServer
// // // Receives CAN frames from Nano sender and displays on OLED + web dashboard
// // // Crash-proof version with better error handling

// // #include <SPI.h>
// // #include <mcp_can.h>
// // #include <Wire.h>
// // #include <Adafruit_GFX.h>
// // #include <Adafruit_SSD1306.h>
// // #include <WiFi.h>
// // #include <WebServer.h>

// // #define CAN_CS_PIN   5
// // #define CAN_INT_PIN 27

// // MCP_CAN CAN(CAN_CS_PIN);

// // #define SCREEN_WIDTH 128
// // #define SCREEN_HEIGHT 64
// // #define OLED_RESET    -1
// // Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// // uint8_t oled_addr = 0x3C;

// // const uint16_t ID_RPM   = 0x100;
// // const uint16_t ID_SPEED = 0x101;
// // const uint16_t ID_THR   = 0x102;

// // // Live state
// // uint16_t rpm_u16 = 0;
// // uint16_t spd_u16 = 0; // 0.1 km/h
// // uint8_t  thr_u8  = 0; // 0.5 %

// // float g_rpm=0, g_speed=0, g_thr=0;
// // String alert_flags="--";

// // // Thresholds
// // float THRESH_SPEED = 80.0f;
// // float THRESH_RPM   = 3500.0f;
// // float THRESH_THR_JUMP = 20.0f;
// // unsigned long THR_WINDOW_MS = 500;
// // float prev_thr = 0.0f;
// // unsigned long prev_thr_ms = 0;

// // unsigned long lastOLED=0;
// // unsigned long lastCompute=0;
// // unsigned long lastCANrx=0;

// // // Wi‑Fi + Web - UPDATE THESE!
// // const char* WIFI_SSID = "Abhijit";     // ← CHANGE THIS
// // const char* WIFI_PASS = "baar9671"; // ← CHANGE THIS
// // WebServer server(80);

// // String htmlPage() {
// //   String s;
// //   s += "<!doctype html><html><head><meta charset='utf-8'>";
// //   s += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
// //   s += "<meta http-equiv='refresh' content='1'>";
// //   s += "<style>body{font-family:Arial;margin:20px;background:#f5f5f5}";
// //   s += ".card{background:white;padding:20px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}";
// //   s += ".value{font-size:32px;font-weight:bold;color:#333}";
// //   s += ".label{color:#666;font-size:14px}";
// //   s += ".ok{color:green}.err{color:red}</style>";
// //   s += "<title>CAN Telemetry</title></head><body>";
// //   s += "<h1>ESP32 CAN Telemetry</h1>";
// //   s += "<div class='card'><div class='label'>Speed</div><div class='value'>" + String(g_speed,1) + " km/h</div></div>";
// //   s += "<div class='card'><div class='label'>RPM</div><div class='value'>" + String(g_rpm,0) + "</div></div>";
// //   s += "<div class='card'><div class='label'>Throttle</div><div class='value'>" + String(g_thr,1) + " %</div></div>";
// //   s += "<div class='card'><div class='label'>Alerts</div><div class='value " + String(alert_flags=="--"?"ok":"err") + "'>" + alert_flags + "</div></div>";
// //   s += "<div class='card'><small>Last CAN: " + String((millis()-lastCANrx)/1000.0,1) + "s ago</small></div>";
// //   s += "</body></html>";
// //   return s;
// // }

// // void handleRoot() {
// //   server.send(200, "text/html", htmlPage());
// // }

// // static inline uint16_t get_u16_le(const uint8_t* b) {
// //   return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
// // }

// // void updateAlertsCompute() {
// //   g_rpm = rpm_u16;
// //   g_speed = spd_u16 / 10.0f;
// //   g_thr = thr_u8 / 2.0f;

// //   String flags = "";
// //   if (g_speed > THRESH_SPEED) { if (flags.length()) flags += " "; flags += "SPD"; }
// //   if (g_rpm > THRESH_RPM)     { if (flags.length()) flags += " "; flags += "RPM"; }

// //   unsigned long now = millis();
// //   if (prev_thr_ms != 0 && (now - prev_thr_ms) <= THR_WINDOW_MS) {
// //     if (fabs(g_thr - prev_thr) > THRESH_THR_JUMP) {
// //       if (flags.length()) flags += " ";
// //       flags += "THR";
// //     }
// //   }
// //   prev_thr = g_thr;
// //   prev_thr_ms = now;

// //   alert_flags = flags.length() ? flags : String("--");
// // }

// // void drawOLED() {
// //   display.clearDisplay();
// //   display.setTextSize(1);
// //   display.setTextColor(SSD1306_WHITE);

// //   display.setCursor(0,0);
// //   display.printf("Spd: %.1f km/h", g_speed);
// //   display.setCursor(0,12);
// //   display.printf("RPM: %.0f", g_rpm);
// //   display.setCursor(0,24);
// //   display.printf("Thr: %.1f %%", g_thr);
// //   display.setCursor(0,36);
// //   display.print("Alert: ");
// //   display.print(alert_flags);
  
// //   display.setCursor(0,50);
// //   unsigned long ago = (millis() - lastCANrx) / 1000;
// //   display.printf("CAN: %lus ago", ago);
  
// //   display.display();
// // }

// // void setup() {
// //   Serial.begin(115200);
// //   delay(100);
// //   Serial.println("[ESP32] Starting...");

// //   // OLED init
// //   Wire.begin(21,22);
// //   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
// //     Serial.println("[ESP32] Trying OLED at 0x3D...");
// //     if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
// //       Serial.println("[ESP32] OLED init failed");
// //     } else {
// //       oled_addr = 0x3D;
// //     }
// //   }
// //   display.clearDisplay();
// //   display.setTextSize(1);
// //   display.setTextColor(SSD1306_WHITE);
// //   display.setCursor(0,0);
// //   display.printf("OLED @0x%02X", oled_addr);
// //   display.display();

// //   // CAN init
// //   pinMode(CAN_INT_PIN, INPUT_PULLUP);
// //   SPI.begin(18,19,23);
  
// //   if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
// //     Serial.println("[ESP32] CAN init OK");
// //   } else {
// //     Serial.println("[ESP32] CAN init FAIL");
// //     display.clearDisplay();
// //     display.setCursor(0,0);
// //     display.print("CAN FAIL!");
// //     display.display();
// //     while (1) { delay(1000); }
// //   }
  
// //   CAN.setMode(MCP_NORMAL);
// //   Serial.println("[ESP32] CAN in NORMAL mode");

// //   // Wi‑Fi
// //   WiFi.mode(WIFI_STA);
// //   WiFi.begin(WIFI_SSID, WIFI_PASS);
// //   Serial.print("[ESP32] WiFi connecting");
  
// //   int attempts = 0;
// //   while (WiFi.status() != WL_CONNECTED && attempts < 20) {
// //     delay(500);
// //     Serial.print(".");
// //     attempts++;
// //   }
  
// //   if (WiFi.status() == WL_CONNECTED) {
// //     Serial.println();
// //     Serial.print("[ESP32] IP: ");
// //     Serial.println(WiFi.localIP());
// //   } else {
// //     Serial.println("\n[ESP32] WiFi failed, continuing without web");
// //   }

// //   server.on("/", handleRoot);
// //   server.begin();
// //   Serial.println("[ESP32] Web server started");
  
// //   delay(1000);
// //   lastCANrx = millis();
// // }

// // void loop() {
// //   // Receive CAN frames - SAFE version
// //   if (CAN.checkReceive() == CAN_MSGAVAIL) {
// //     unsigned long rxId = 0;
// //     byte dlc = 0;
// //     byte buf[8];
    
// //     // Clear buffer first
// //     memset(buf, 0, sizeof(buf));
    
// //     if (CAN.readMsgBuf(&rxId, &dlc, buf) == CAN_OK) {
// //       lastCANrx = millis();
      
// //       // Only process known IDs to avoid crash
// //       if (rxId == ID_RPM && dlc >= 2) {
// //         rpm_u16 = get_u16_le(buf);
// //         Serial.printf("[ESP32] RX RPM: %u\n", rpm_u16);
// //       }
// //       else if (rxId == ID_SPEED && dlc >= 2) {
// //         spd_u16 = get_u16_le(buf);
// //         Serial.printf("[ESP32] RX SPD: %u\n", spd_u16);
// //       }
// //       else if (rxId == ID_THR && dlc >= 1) {
// //         thr_u8 = buf[0];
// //         Serial.printf("[ESP32] RX THR: %u\n", thr_u8);
// //       }
// //       else {
// //         // Ignore unknown/noise frames
// //         Serial.printf("[ESP32] Ignored ID=0x%lX DLC=%d\n", rxId, dlc);
// //       }
// //     }
// //   }

// //   // Compute & display
// //   unsigned long now = millis();
// //   if (now - lastCompute >= 100) {
// //     updateAlertsCompute();
// //     lastCompute = now;
// //   }
// //   if (now - lastOLED >= 200) {
// //     drawOLED();
// //     lastOLED = now;
// //   }

// //   server.handleClient();
// // }



// // ESP32 CAN Receiver - FINAL CRASH-PROOF VERSION
// #include <SPI.h>
// #include <mcp_can.h>
// #include <Wire.h>
// #include <Adafruit_GFX.h>
// #include <Adafruit_SSD1306.h>
// #include <WiFi.h>
// #include <WebServer.h>

// #define CAN_CS_PIN   5
// #define CAN_INT_PIN 27

// MCP_CAN CAN(CAN_CS_PIN);

// #define SCREEN_WIDTH 128
// #define SCREEN_HEIGHT 64
// #define OLED_RESET -1
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// const uint16_t ID_RPM   = 0x100;
// const uint16_t ID_SPEED = 0x101;
// const uint16_t ID_THR   = 0x102;

// volatile uint16_t rpm_u16 = 0;
// volatile uint16_t spd_u16 = 0;
// volatile uint8_t  thr_u8  = 0;

// float g_rpm=0, g_speed=0, g_thr=0;
// String alert_flags="--";
// unsigned long lastCANrx=0;
// unsigned long lastOLED=0;
// unsigned long rxCount=0;

// const char* WIFI_SSID = "Abhijit";     // ← CHANGE
// const char* WIFI_PASS = "baar9671"; // ← CHANGE
// WebServer server(80);

// String htmlPage() {
//   String s = "<!doctype html><html><head><meta charset='utf-8'>";
//   s += "<meta http-equiv='refresh' content='1'>";
//   s += "<style>body{font-family:Arial;margin:20px;background:#f5f5f5}";
//   s += ".card{background:white;padding:20px;margin:10px;border-radius:8px}";
//   s += ".value{font-size:32px;font-weight:bold}</style></head><body>";
//   s += "<h1>ESP32 CAN Telemetry</h1>";
//   s += "<div class='card'><div>Speed</div><div class='value'>" + String(g_speed,1) + " km/h</div></div>";
//   s += "<div class='card'><div>RPM</div><div class='value'>" + String(g_rpm,0) + "</div></div>";
//   s += "<div class='card'><div>Throttle</div><div class='value'>" + String(g_thr,1) + " %</div></div>";
//   s += "<div class='card'><small>Frames: " + String(rxCount) + "</small></div>";
//   s += "</body></html>";
//   return s;
// }

// void handleRoot() {
//   server.send(200, "text/html", htmlPage());
// }

// void drawOLED() {
//   display.clearDisplay();
//   display.setTextSize(1);
//   display.setTextColor(SSD1306_WHITE);
//   display.setCursor(0,0);
//   display.printf("Spd:%.1fkm/h", g_speed);
//   display.setCursor(0,12);
//   display.printf("RPM:%.0f", g_rpm);
//   display.setCursor(0,24);
//   display.printf("Thr:%.1f%%", g_thr);
//   display.setCursor(0,40);
//   display.printf("RX:%lu", rxCount);
//   display.display();
// }

// void setup() {
//   Serial.begin(115200);
//   delay(500);
//   Serial.println("\n[ESP32] Starting...");

//   // SPI setup
//   pinMode(CAN_CS_PIN, OUTPUT);
//   digitalWrite(CAN_CS_PIN, HIGH);
//   pinMode(CAN_INT_PIN, INPUT_PULLUP);
//   SPI.begin(18,19,23);
//   delay(200);

//   // CAN with retry
//   Serial.println("[ESP32] Init CAN...");
//   bool canOK = false;
//   for (int attempt = 0; attempt < 5 && !canOK; attempt++) {
//     // if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
//     //   Serial.println("[ESP32] CAN OK @ 8MHz");
//     //   canOK = true;
//     // } else {
//     //   delay(100);
//     //   if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK) {
//     //     Serial.println("[ESP32] CAN OK @ 16MHz");
//     //     canOK = true;
//     //   } else {
//     //     delay(500);
//     //   }
//     // }
//     if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
//       Serial.println("[ESP32] CAN OK @ 8MHz");
//       canOK = true;
//     }
//   }
  
//   if (!canOK) {
//     Serial.println("[ESP32] CAN FAIL");
//     while(1) delay(1000);
//   }
  
//   CAN.setMode(MCP_NORMAL);
//   delay(100);

//   // OLED
//   Wire.begin(21,22);
//   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
//     display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
//   }
//   display.clearDisplay();
//   display.setTextSize(1);
//   display.setTextColor(SSD1306_WHITE);
//   display.setCursor(0,0);
//   display.println("ESP32 CAN");
//   display.display();

//   // WiFi
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   int attempts = 0;
//   while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
//     delay(500);
//   }
  
//   if (WiFi.status() == WL_CONNECTED) {
//     Serial.println("[ESP32] IP: " + WiFi.localIP().toString());
//   }

//   server.on("/", handleRoot);
//   server.begin();
//   Serial.println("[ESP32] Ready");
//   lastCANrx = millis();
// }

// void loop() {
//   // PROTECTED CAN receive - NO CRASHES
//   if (digitalRead(CAN_INT_PIN) == LOW) {
//     unsigned long id = 0;
//     byte len = 0;
//     byte buf[8];
//     memset(buf, 0, 8); // Clear buffer
    
//     byte stat = CAN.readMsgBuf(&id, &len, buf);
    
//     // CRITICAL: Only process if read succeeded AND length is valid
//     if (stat == CAN_OK && len > 0 && len <= 8) {
//       // ONLY process known IDs - ignore everything else
//       if (id == ID_RPM && len >= 2) {
//         rpm_u16 = buf[0] | (buf[1] << 8);
//         g_rpm = rpm_u16;
//         rxCount++;
//         lastCANrx = millis();
//       }
//       else if (id == ID_SPEED && len >= 2) {
//         spd_u16 = buf[0] | (buf[1] << 8);
//         g_speed = spd_u16 / 10.0f;
//         rxCount++;
//         lastCANrx = millis();
//       }
//       else if (id == ID_THR && len >= 1) {
//         thr_u8 = buf[0];
//         g_thr = thr_u8 / 2.0f;
//         rxCount++;
//         lastCANrx = millis();
//       }
//       // Unknown IDs: do NOTHING - no serial print, no processing
//     }
//   }

//   // Update OLED
//   if (millis() - lastOLED >= 250) {
//     g_rpm = rpm_u16;
//     g_speed = spd_u16 / 10.0f;
//     g_thr = thr_u8 / 2.0f;
//     drawOLED();
//     lastOLED = millis();
//   }

//   server.handleClient();
//   delay(1); // Critical: yield to watchdog
// }
// ESP32 Dual Mode Telemetry Receiver - CAN & WiFi
// With alert system, diagnostics, and formatted displays

#include <SPI.h>
#include <mcp_can.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define CAN_CS_PIN   5
#define CAN_INT_PIN 27

#define MODE_CAN_ONLY 1
#define MODE_WIFI_ONLY 2
#define MODE_BOTH 3

#define EEPROM_MODE_ADDR 0
#define EEPROM_SIZE 10

MCP_CAN CAN(CAN_CS_PIN);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const uint16_t ID_RPM   = 0x100;
const uint16_t ID_SPEED = 0x101;
const uint16_t ID_THR   = 0x102;

// ===== ALERT THRESHOLDS =====
const float THRESHOLD_SPEED_HIGH = 100.0f;      // km/h
const float THRESHOLD_SPEED_WARNING = 80.0f;    // km/h
const float THRESHOLD_RPM_HIGH = 5000.0f;       // RPM
const float THRESHOLD_RPM_WARNING = 4000.0f;    // RPM
const float THRESHOLD_THR_HARSH = 25.0f;        // % delta in 500ms
const unsigned long HARSH_THR_WINDOW = 500;    // ms

// Alert state tracking
struct AlertState {
  bool speed_high = false;
  bool speed_warning = false;
  bool rpm_high = false;
  bool rpm_warning = false;
  bool throttle_harsh = false;
  bool data_timeout = false;
  unsigned long last_alert_time = 0;
};

volatile uint16_t rpm_u16 = 0;
volatile uint16_t spd_u16 = 0;
volatile uint8_t  thr_u8  = 0;

float g_rpm=0, g_speed=0, g_thr=0;
float prev_thr = 0.0f;
unsigned long prev_thr_time = 0;

String rxSource="--";
unsigned long lastCANrx=0;
unsigned long lastWiFiRx=0;
unsigned long lastOLED=0;
unsigned long rxCount=0;
unsigned long dataTimeout = 5000; // 5 seconds

AlertState alerts;
String alertString = "";

int operatingMode = MODE_BOTH;
bool canEnabled = false;
bool wifiEnabled = false;

const char* WIFI_SSID = "Abhijit";
const char* WIFI_PASS = "baar9671";
// const int UDP_PORT = 4210;
// const char* WIFI_SSID = "P4A-5G";
// const char* WIFI_PASS = "iiitbbsr";
const int UDP_PORT = 8888;

WebServer server(80);
WiFiUDP udp;

// ===== HELPER FUNCTIONS =====
String getModeString() {
  if (operatingMode == MODE_CAN_ONLY) return "CAN";
  if (operatingMode == MODE_WIFI_ONLY) return "WiFi";
  return "Both";
}

String getStatusColor() {
  if (alerts.speed_high || alerts.rpm_high || alerts.throttle_harsh) return "#d32f2f";
  if (alerts.speed_warning || alerts.rpm_warning || alerts.data_timeout) return "#f57c00";
  return "#388e3c";
}

String getStatusLabel() {
  if (alerts.speed_high || alerts.rpm_high || alerts.throttle_harsh) return "🔴 CRITICAL";
  if (alerts.speed_warning || alerts.rpm_warning || alerts.data_timeout) return "🟠 WARNING";
  return "🟢 NORMAL";
}

void updateAlerts() {
  // Clear old alerts
  alerts.speed_high = false;
  alerts.speed_warning = false;
  alerts.rpm_high = false;
  alerts.rpm_warning = false;
  alerts.throttle_harsh = false;
  
  unsigned long now = millis();
  
  // Speed alerts
  if (g_speed > THRESHOLD_SPEED_HIGH) {
    alerts.speed_high = true;
  } else if (g_speed > THRESHOLD_SPEED_WARNING) {
    alerts.speed_warning = true;
  }
  
  // RPM alerts
  if (g_rpm > THRESHOLD_RPM_HIGH) {
    alerts.rpm_high = true;
  } else if (g_rpm > THRESHOLD_RPM_WARNING) {
    alerts.rpm_warning = true;
  }
  
  // Throttle harsh acceleration check
  if (prev_thr_time != 0 && (now - prev_thr_time) <= HARSH_THR_WINDOW) {
    float thr_delta = fabs(g_thr - prev_thr);
    if (thr_delta > THRESHOLD_THR_HARSH) {
      alerts.throttle_harsh = true;
    }
  }
  
  // Data timeout check
  unsigned long timeSinceLastData = min(now - lastCANrx, now - lastWiFiRx);
  if (timeSinceLastData > dataTimeout) {
    alerts.data_timeout = true;
  } else {
    alerts.data_timeout = false;
  }
  
  prev_thr = g_thr;
  prev_thr_time = now;
}

void buildAlertString() {
  alertString = "";
  
  if (alerts.speed_high) {
    alertString += "SPD_HIGH ";
  }
  if (alerts.speed_warning) {
    alertString += "SPD_WARN ";
  }
  if (alerts.rpm_high) {
    alertString += "RPM_HIGH ";
  }
  if (alerts.rpm_warning) {
    alertString += "RPM_WARN ";
  }
  if (alerts.throttle_harsh) {
    alertString += "THR_HARSH ";
  }
  if (alerts.data_timeout) {
    alertString += "NO_DATA ";
  }
  
  if (alertString.length() == 0) {
    alertString = "OK";
  }
}

String getAlertAdvice() {
  String advice = "";
  
  if (alerts.speed_high) {
    advice += "<div class='alert-box critical'>";
    advice += "<b>⚠️ CRITICAL: Speed Too High (>" + String(THRESHOLD_SPEED_HIGH) + " km/h)</b><br>";
    advice += "ACTION: Reduce vehicle speed immediately. Check brake pedal engagement.<br>";
    advice += "✓ Issue will auto-resolve when speed drops below threshold.";
    advice += "</div>";
  }
  
  if (alerts.rpm_high) {
    advice += "<div class='alert-box critical'>";
    advice += "<b>⚠️ CRITICAL: RPM Too High (>" + String((int)THRESHOLD_RPM_HIGH) + " RPM)</b><br>";
    advice += "ACTION: Reduce throttle input. Check engine idle. Verify fuel mixture.<br>";
    advice += "✓ Issue will auto-resolve when RPM drops below threshold.";
    advice += "</div>";
  }
  
  if (alerts.throttle_harsh) {
    advice += "<div class='alert-box critical'>";
    advice += "<b>⚠️ CRITICAL: Harsh Throttle Acceleration (>" + String((int)THRESHOLD_THR_HARSH) + "% in 500ms)</b><br>";
    advice += "ACTION: Apply throttle gradually. Check pedal sensitivity and calibration.<br>";
    advice += "✓ Issue will auto-resolve with smooth throttle inputs.";
    advice += "</div>";
  }
  
  if (alerts.speed_warning) {
    advice += "<div class='alert-box warning'>";
    advice += "<b>⚠️ WARNING: Speed Elevated (" + String(THRESHOLD_SPEED_WARNING) + "-" + String(THRESHOLD_SPEED_HIGH) + " km/h)</b><br>";
    advice += "ACTION: Monitor speed. Maintain safe driving conditions.<br>";
    advice += "✓ Will resolve when speed normalizes.";
    advice += "</div>";
  }
  
  if (alerts.rpm_warning) {
    advice += "<div class='alert-box warning'>";
    advice += "<b>⚠️ WARNING: RPM Elevated (" + String((int)THRESHOLD_RPM_WARNING) + "-" + String((int)THRESHOLD_RPM_HIGH) + " RPM)</b><br>";
    advice += "ACTION: Monitor RPM. Reduce engine load if prolonged.<br>";
    advice += "✓ Will resolve when RPM normalizes.";
    advice += "</div>";
  }
  
  if (alerts.data_timeout) {
    advice += "<div class='alert-box critical'>";
    advice += "<b>⚠️ CRITICAL: Data Timeout (No data for >" + String((int)(dataTimeout/1000)) + "s)</b><br>";
    advice += "ACTION (WiFi): Check ESP32 IP address in Serial Monitor. Verify network connection.<br>";
    advice += "ACTION (CAN): Check CAN wiring (CANH/CANL/GND). Verify Nano is running.<br>";
    advice += "ACTION (Both): Restart both Nano and ESP32.<br>";
    advice += "✓ Issue will auto-resolve when data reception resumes.";
    advice += "</div>";
  }
  
  if (alertString == "OK") {
    advice = "<div class='alert-box normal'>";
    advice += "<b>✅ SYSTEM NORMAL</b><br>";
    advice += "All parameters within safe thresholds.<br>";
    advice += "Vehicle operation is nominal.";
    advice += "</div>";
  }
  
  return advice;
}

String htmlPage() {
  updateAlerts();
  buildAlertString();
  
  String s = "<!doctype html><html><head><meta charset='utf-8'>";
  s += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  s += "<meta http-equiv='refresh' content='1'>";
  s += "<style>";
  s += "* { margin:0; padding:0; box-sizing:border-box; }";
  s += "body { font-family:'Segoe UI',Arial; background:#0f0f0f; color:#fff; padding:20px; }";
  s += "h1 { color:" + String(getStatusColor()) + "; margin-bottom:10px; font-size:28px; }";
  s += ".mode-badge { display:inline-block; background:#1e1e1e; padding:5px 10px; border-radius:5px; font-size:12px; margin-left:10px; }";
  s += ".status-banner { background:" + String(getStatusColor()) + "; color:#fff; padding:15px; border-radius:8px; margin:15px 0; font-weight:bold; font-size:18px; }";
  s += ".cards-container { display:grid; grid-template-columns:repeat(3,1fr); gap:15px; margin:20px 0; }";
  s += "@media (max-width:768px) { .cards-container { grid-template-columns:1fr; } }";
  s += ".card { background:#1e1e1e; padding:20px; border-radius:8px; border-left:4px solid #4CAF50; }";
  s += ".card.warning { border-left-color:#ff9800; }";
  s += ".card.critical { border-left-color:#d32f2f; }";
  s += ".card-label { color:#999; font-size:12px; text-transform:uppercase; letter-spacing:1px; }";
  s += ".card-value { font-size:36px; font-weight:bold; margin:10px 0; }";
  s += ".card-unit { color:#999; font-size:14px; }";
  s += ".info-row { display:flex; justify-content:space-between; padding:10px 0; border-bottom:1px solid #333; }";
  s += ".info-row:last-child { border-bottom:none; }";
  s += ".info-label { color:#999; }";
  s += ".info-value { font-weight:bold; }";
  s += ".alert-box { padding:15px; margin:15px 0; border-radius:8px; border-left:4px solid; }";
  s += ".alert-box.normal { background:#1b5e20; border-left-color:#4CAF50; }";
  s += ".alert-box.warning { background:#e65100; border-left-color:#ff9800; }";
  s += ".alert-box.critical { background:#b71c1c; border-left-color:#d32f2f; }";
  s += ".alert-box b { display:block; margin-bottom:8px; font-size:14px; }";
  s += ".alert-box { font-size:13px; line-height:1.6; }";
  s += ".threshold-guide { background:#263238; padding:15px; border-radius:8px; margin:20px 0; }";
  s += ".threshold-guide h3 { color:#4CAF50; margin-bottom:10px; }";
  s += ".threshold-row { display:flex; justify-content:space-between; padding:8px 0; border-bottom:1px solid #444; }";
  s += ".threshold-row:last-child { border-bottom:none; }";
  s += ".threshold-label { color:#999; }";
  s += ".threshold-value { font-weight:bold; color:#4CAF50; }";
  s += ".critical-threshold { color:#d32f2f; }";
  s += ".warning-threshold { color:#ff9800; }";
  s += ".footer { text-align:center; color:#666; font-size:11px; margin-top:30px; padding-top:20px; border-top:1px solid #333; }";
  s += "</style></head><body>";
  
  s += "<h1>" + getStatusLabel() + " <span class='mode-badge'>" + getModeString() + " MODE</span></h1>";
  s += "<div class='status-banner'>" + alertString + "</div>";
  
  s += "<div class='cards-container'>";
  
  // Speed Card
  String speedClass = "card";
  if (alerts.speed_high) speedClass = "card critical";
  else if (alerts.speed_warning) speedClass = "card warning";
  s += "<div class='" + speedClass + "'>";
  s += "<div class='card-label'>Speed</div>";
  s += "<div class='card-value'>" + String(g_speed, 1) + "<span class='card-unit'> km/h</span></div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Warning</span>";
  s += "<span class='info-value warning-threshold'>" + String(THRESHOLD_SPEED_WARNING) + " km/h</span>";
  s += "</div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Critical</span>";
  s += "<span class='info-value critical-threshold'>" + String(THRESHOLD_SPEED_HIGH) + " km/h</span>";
  s += "</div>";
  s += "</div>";
  
  // RPM Card
  String rpmClass = "card";
  if (alerts.rpm_high) rpmClass = "card critical";
  else if (alerts.rpm_warning) rpmClass = "card warning";
  s += "<div class='" + rpmClass + "'>";
  s += "<div class='card-label'>RPM</div>";
  s += "<div class='card-value'>" + String((int)g_rpm) + "<span class='card-unit'> RPM</span></div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Warning</span>";
  s += "<span class='info-value warning-threshold'>" + String((int)THRESHOLD_RPM_WARNING) + " RPM</span>";
  s += "</div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Critical</span>";
  s += "<span class='info-value critical-threshold'>" + String((int)THRESHOLD_RPM_HIGH) + " RPM</span>";
  s += "</div>";
  s += "</div>";
  
  // Throttle Card
  String thrClass = "card";
  if (alerts.throttle_harsh) thrClass = "card critical";
  s += "<div class='" + thrClass + "'>";
  s += "<div class='card-label'>Throttle</div>";
  s += "<div class='card-value'>" + String(g_thr, 1) + "<span class='card-unit'> %</span></div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Max Harsh Delta</span>";
  s += "<span class='info-value critical-threshold'>" + String((int)THRESHOLD_THR_HARSH) + " %/500ms</span>";
  s += "</div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Status</span>";
  s += "<span class='info-value'>" + String(alerts.throttle_harsh ? "⚠️ HARSH" : "✓ SMOOTH") + "</span>";
  s += "</div>";
  s += "</div>";
  
  s += "</div>";
  
  // Alert advice section
  s += "<h2>⚠️ System Status & Recommendations</h2>";
  s += getAlertAdvice();
  
  // Threshold guide
  s += "<div class='threshold-guide'>";
  s += "<h3>📊 Threshold Configuration</h3>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>Speed Warning</span>";
  s += "<span class='threshold-value warning-threshold'>" + String(THRESHOLD_SPEED_WARNING) + " km/h</span>";
  s += "</div>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>Speed Critical</span>";
  s += "<span class='threshold-value critical-threshold'>" + String(THRESHOLD_SPEED_HIGH) + " km/h</span>";
  s += "</div>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>RPM Warning</span>";
  s += "<span class='threshold-value warning-threshold'>" + String((int)THRESHOLD_RPM_WARNING) + " RPM</span>";
  s += "</div>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>RPM Critical</span>";
  s += "<span class='threshold-value critical-threshold'>" + String((int)THRESHOLD_RPM_HIGH) + " RPM</span>";
  s += "</div>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>Harsh Throttle</span>";
  s += "<span class='threshold-value critical-threshold'>" + String((int)THRESHOLD_THR_HARSH) + " %/500ms</span>";
  s += "</div>";
  s += "<div class='threshold-row'>";
  s += "<span class='threshold-label'>Data Timeout</span>";
  s += "<span class='threshold-value'>" + String((int)(dataTimeout/1000)) + " seconds</span>";
  s += "</div>";
  s += "</div>";
  
  // System info
  s += "<div class='threshold-guide'>";
  s += "<h3>📡 System Information</h3>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Operating Mode</span>";
  s += "<span class='info-value'>" + getModeString() + "</span>";
  s += "</div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Data Source</span>";
  s += "<span class='info-value'>" + rxSource + "</span>";
  s += "</div>";
  s += "<div class='info-row'>";
  s += "<span class='info-label'>Total Frames Received</span>";
  s += "<span class='info-value'>" + String(rxCount) + "</span>";
  s += "</div>";
  if (canEnabled) {
    s += "<div class='info-row'>";
    s += "<span class='info-label'>CAN Last RX</span>";
    s += "<span class='info-value'>" + String((millis()-lastCANrx)/1000.0, 1) + "s ago</span>";
    s += "</div>";
  }
  if (wifiEnabled) {
    s += "<div class='info-row'>";
    s += "<span class='info-label'>WiFi Last RX</span>";
    s += "<span class='info-value'>" + String((millis()-lastWiFiRx)/1000.0, 1) + "s ago</span>";
    s += "</div>";
  }
  s += "</div>";
  
  s += "<div class='footer'>";
  s += "ESP32 Telemetry System | Multi-Mode Receiver | Real-time Monitoring";
  s += "</div>";
  
  s += "</body></html>";
  return s;
}

void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void drawOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header with mode
  display.setCursor(0,0);
  display.printf("[%s] %s", getModeString().c_str(), getStatusLabel().c_str());
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  
  // Data readings
  display.setCursor(0,12);
  display.printf("SPD:%.1f km/h RPM:%d", g_speed, (int)g_rpm);
  
  display.setCursor(0,22);
  display.printf("THR:%.1f%% SRC:%s", g_thr, rxSource.c_str());
  
  // Alert status line
  display.drawFastHLine(0, 32, 128, SSD1306_WHITE);
  display.setCursor(0,35);
  
  if (alerts.speed_high) {
    display.printf(">> SPEED CRITICAL <<");
  } else if (alerts.rpm_high) {
    display.printf(">> RPM CRITICAL <<");
  } else if (alerts.throttle_harsh) {
    display.printf(">> HARSH THROTTLE <<");
  } else if (alerts.speed_warning) {
    display.printf(">> SPEED WARNING <<");
  } else if (alerts.rpm_warning) {
    display.printf(">> RPM WARNING <<");
  } else if (alerts.data_timeout) {
    display.printf(">> NO DATA <<");
  } else {
    display.printf(">> SYSTEM NORMAL <<");
  }
  
  // Stats
  display.setCursor(0,47);
  display.printf("RX:%lu|%s|T:%lums", rxCount, (canEnabled ? "CAN" : "WiFi"), (millis()-lastCANrx));
  
  display.display();
}

bool parseWiFiLine(const String& line, uint16_t& out_rpm, uint16_t& out_spd, uint8_t& out_thr) {
  int rpmIdx = line.indexOf("rpm=");
  if (rpmIdx < 0) return false;
  int rpmStart = rpmIdx + 4;
  int rpmEnd = line.indexOf(',', rpmStart);
  if (rpmEnd < 0) rpmEnd = line.length();
  String rpmStr = line.substring(rpmStart, rpmEnd);
  float rpm = rpmStr.toFloat();
  if (rpm < 0 || rpm > 65535) return false;
  out_rpm = (uint16_t)round(rpm);

  int spdIdx = line.indexOf("speed=");
  if (spdIdx < 0) return false;
  int spdStart = spdIdx + 6;
  int spdEnd = line.indexOf(',', spdStart);
  if (spdEnd < 0) spdEnd = line.length();
  String spdStr = line.substring(spdStart, spdEnd);
  float spd = spdStr.toFloat();
  if (spd < 0 || spd > 6553.5) return false;
  out_spd = (uint16_t)round(spd * 10.0f);

  int thrIdx = line.indexOf("thr=");
  if (thrIdx < 0) return false;
  int thrStart = thrIdx + 4;
  int thrEnd = line.indexOf(',', thrStart);
  if (thrEnd < 0) thrEnd = line.length();
  String thrStr = line.substring(thrStart, thrEnd);
  float thr = thrStr.toFloat();
  if (thr < 0 || thr > 100) return false;
  out_thr = (uint8_t)round(thr * 2.0f);

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[ESP32] Starting...");

  EEPROM.begin(EEPROM_SIZE);
  operatingMode = EEPROM.read(EEPROM_MODE_ADDR);
  if (operatingMode < 1 || operatingMode > 3) {
    operatingMode = MODE_BOTH;
    EEPROM.write(EEPROM_MODE_ADDR, operatingMode);
    EEPROM.commit();
  }

  Serial.printf("[ESP32] Operating Mode: %s\n", getModeString().c_str());

  Wire.begin(21,22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3D);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.printf("Telemetry[%s]", getModeString().c_str());
  display.setCursor(0,12);
  display.println("Initializing...");
  display.display();

  // ===== CAN SETUP =====
  if (operatingMode == MODE_CAN_ONLY || operatingMode == MODE_BOTH) {
    Serial.println("[CAN] Init...");
    pinMode(CAN_CS_PIN, OUTPUT);
    digitalWrite(CAN_CS_PIN, HIGH);
    pinMode(CAN_INT_PIN, INPUT_PULLUP);
    SPI.begin(18,19,23);
    delay(200);

    bool canOK = false;
    for (int attempt = 0; attempt < 5 && !canOK; attempt++) {
      if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        Serial.println("[CAN] OK @ 8MHz");
        canOK = true;
        canEnabled = true;
      } else {
        delay(500);
      }
    }

    if (canOK) {
      CAN.setMode(MCP_NORMAL);
      delay(100);
    } else {
      Serial.println("[CAN] FAIL");
      if (operatingMode == MODE_CAN_ONLY) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("CAN FAIL!");
        display.display();
        while(1) delay(1000);
      }
    }
  }

  // ===== WiFi SETUP =====
  if (operatingMode == MODE_WIFI_ONLY || operatingMode == MODE_BOTH) {
    Serial.println("[WiFi] Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts++ < 20) {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println();
      Serial.println("[WiFi] IP: " + WiFi.localIP().toString());
      Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);
      udp.begin(UDP_PORT);
      wifiEnabled = true;
    } else {
      Serial.println("\n[WiFi] FAIL");
      if (operatingMode == MODE_WIFI_ONLY) {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("WiFi FAIL!");
        display.display();
        while(1) delay(1000);
      }
    }
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.printf("[ESP32] Ready - Mode: %s\n", getModeString().c_str());
  lastCANrx = millis();
  lastWiFiRx = millis();
}

void loop() {
  // ====== CAN RECEIVE ======
  if (canEnabled && (operatingMode == MODE_CAN_ONLY || operatingMode == MODE_BOTH)) {
    if (digitalRead(CAN_INT_PIN) == LOW) {
      unsigned long id = 0;
      byte len = 0;
      byte buf[8];
      memset(buf, 0, 8);

      byte stat = CAN.readMsgBuf(&id, &len, buf);

      if (stat == CAN_OK && len > 0 && len <= 8) {
        if (id == ID_RPM && len >= 2) {
          rpm_u16 = buf[0] | (buf[1] << 8);
          g_rpm = rpm_u16;
          rxCount++;
          lastCANrx = millis();
          rxSource = "CAN";
        }
        else if (id == ID_SPEED && len >= 2) {
          spd_u16 = buf[0] | (buf[1] << 8);
          g_speed = spd_u16 / 10.0f;
          rxCount++;
          lastCANrx = millis();
          rxSource = "CAN";
        }
        else if (id == ID_THR && len >= 1) {
          thr_u8 = buf[0];
          g_thr = thr_u8 / 2.0f;
          rxCount++;
          lastCANrx = millis();
          rxSource = "CAN";
        }
      }
    }
  }

  // ====== WiFi UDP RECEIVE ======
  if (wifiEnabled && (operatingMode == MODE_WIFI_ONLY || operatingMode == MODE_BOTH)) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      char packet[256] = {0};
      udp.read(packet, min(packetSize, 255));
      String line = String(packet);
      line.trim();

      uint16_t wifi_rpm = 0;
      uint16_t wifi_spd = 0;
      uint8_t wifi_thr = 0;

      if (parseWiFiLine(line, wifi_rpm, wifi_spd, wifi_thr)) {
        rpm_u16 = wifi_rpm;
        spd_u16 = wifi_spd;
        thr_u8 = wifi_thr;
        g_rpm = rpm_u16;
        g_speed = spd_u16 / 10.0f;
        g_thr = thr_u8 / 2.0f;
        rxCount++;
        lastWiFiRx = millis();
        rxSource = "WiFi";
      }
    }
  }

  // ====== OLED UPDATE ======
  if (millis() - lastOLED >= 250) {
    drawOLED();
    lastOLED = millis();
  }

  // ====== WEB SERVER ======
  server.handleClient();
  delay(1);
}
