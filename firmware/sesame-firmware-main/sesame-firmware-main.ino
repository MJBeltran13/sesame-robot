#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>
#include "generated-oled-art/headers/sesame-face-default.h"
#include "generated-oled-art/headers/sesame-face-happy.h"
#include "generated-oled-art/headers/sesame-face-sad.h"
#include "generated-oled-art/headers/sesame-face-angry.h"
#include "generated-oled-art/headers/sesame-face-surprised.h"
#include "generated-oled-art/headers/sesame-face-sleepy.h"
#include "generated-oled-art/headers/sesame-face-love.h"
#include "generated-oled-art/headers/sesame-face-confused.h"
#include "generated-oled-art/headers/sesame-face-thinking.h"
#include "generated-oled-art/headers/sesame-face-excited.h"
#include "generated-oled-art/headers/sesame-face-dead.h"
#include "generated-oled-art/headers/sesame-face-wink.h"
#include "movement-sequences.h"
#include "captive-portal.h"

// --- Access Point Configuration ---
// This is the network the Robot will create
#define AP_SSID  "Sesame-Controller"
#define AP_PASS  "12345678" // Must be at least 8 characters

// --- Station Mode Configuration (Optional) ---
// Set these to connect to your home/office WiFi network
// Leave NETWORK_SSID empty to disable station mode
#define NETWORK_SSID "BatStateU-DevOps"  // Your WiFi network name
#define NETWORK_PASS "Dev3l$06"  // Your WiFi password
#define ENABLE_NETWORK_MODE true  // Set to true to enable network connection attempts

// --- Trioe Hub Control Configuration ---
// Enable this after NETWORK_SSID/PASS are filled in and the robot can reach Trioe Hub.
#define ENABLE_TRIOE_HUB_CONTROL true
#define DISABLE_HOTSPOT_WHEN_TRIOE_CONNECTED true
#define TRIOE_HUB_DATA_URL "https://hub.trioe.dev/api/devices/44123/data/microcontroller/"
#define TRIOE_HUB_POST_URL "https://hub.trioe.dev/api/devices/44123/data/"
#define TRIOE_API_KEY "CA17FF35"
#define TRIOE_COMMAND_STREAM "command"
#define TRIOE_JOYSTICK_STREAM "joystick"
#define TRIOE_POLL_INTERVAL_MS 150
#define TRIOE_HTTP_TIMEOUT_MS 650
#define TRIOE_ACK_TIMEOUT_MS 900
#define TRIOE_FAILURE_BACKOFF_MS 2000
#define TRIOE_JOYSTICK_DEADZONE 35
#define ENABLE_TRIOE_DEBUG_PRINTS true
#define ENABLE_TRIOE_POLL_VALUE_PRINTS true
#define ENABLE_SERVO_DEBUG_PRINTS false

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define FACE_BITMAP_WIDTH 128
#define FACE_BITMAP_HEIGHT 128
#define DISPLAY_ROTATION 3
#define FACE_BITMAP_INVERT false
#define DISPLAY_SEG_REMAP 0x53
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C

// I2C Pins for SSD1327 OLED
#define I2C_SDA 47
#define I2C_SCL 48

// I2C Pins for Distro Board V2 / V3
//#define I2C_SDA 8
//#define I2C_SCL 9

// I2C Pins for Distro Board V1
//#define I2C_SDA 21
//#define I2C_SCL 22

// I2C Pins for S2 Mini Board
//#define I2C_SDA 33
//#define I2C_SCL 35


// DNS Server for Captive Portal
DNSServer dnsServer;
const byte DNS_PORT = 53;

Adafruit_SSD1327 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
bool displayReady = false;
uint8_t activeOledAddr = OLED_I2C_ADDR;
int activeI2cSda = I2C_SDA;
int activeI2cScl = I2C_SCL;

// Global state for animations
String currentCommand = "";
String executingCommand = "";
String currentFaceName = "default";
const unsigned char* const* currentFaceFrames = nullptr;
uint8_t currentFaceFrameCount = 0;
uint8_t currentFaceFrameIndex = 0;
unsigned long lastFaceFrameMs = 0;
int faceFps = 8;
FaceAnimMode currentFaceMode = FACE_ANIM_LOOP;
int8_t faceFrameDirection = 1;
bool faceAnimFinished = false;
int currentFaceFps = 0;
bool idleActive = false;
bool idleBlinkActive = false;
unsigned long nextIdleBlinkMs = 0;
uint8_t idleBlinkRepeatsLeft = 0;

// WiFi Info Scrolling
unsigned long lastInputTime = 0;
bool firstInputReceived = false;
bool showingWifiInfo = false;
int wifiScrollPos = 0;
unsigned long lastWifiScrollMs = 0;
String wifiInfoText = "";
unsigned long lastHeartbeatMs = 0;
unsigned long lastTrioePollMs = 0;
unsigned long trioePollBackoffUntilMs = 0;
unsigned long lastTrioePollErrorPrintMs = 0;
int lastTrioePollErrorStatus = 0;
unsigned long lastNetworkRetryMs = 0;
String lastTrioeCommandValue = "";
String lastTrioeCommandUpdateToken = "";
String lastTrioeDuplicateDebugKey = "";
String lastTrioeJoystickCommand = "";
String lastTrioePollDebugSnapshot = "";
String currentCommandUpdateToken = "";
bool trioePollInProgress = false;

// Network Mode
bool networkConnected = false;
bool localRemoteEnabled = false;
IPAddress networkIP;
String deviceHostname = "sesame-robot";

// Servo Pins for Distro Board
// ======================================================================
// Pin numbers are coorisponding to the ESP32 GPIO pins and may differ based on which board you use.
// If you are using a different board, please adjust the servoPins array accordingly.
// ======================================================================
Servo servos[8];
// Sesame Distro Board V3 Pinout [NEW]
const int servoPins[8] = {
  39,   // S0 / R1 servo signal wire
  36,   // S1 / R2 servo signal wire
  5,   // S2 / L1 servo signal wire
  7,   // S3 / L2 servo signal wire
  40,  // S4 / R4 servo signal wire
  38,  // S5 / R3 servo signal wire
  4,  // S6 / L3 servo signal wire
  6   // S7 / L4 servo signal wire
};

// Sesame Distro Board V2 Pinout (Legacy)
//const int servoPins[8] = {4, 5, 6, 7, 15, 16, 17, 18};

// Sesame Distro Board V1 Pinout (Legacy)
//const int servoPins[8] = {15, 2, 23, 19, 4, 16, 17, 18};

// Lolin S2 Mini Pinout
//const int servoPins[8] = {1, 2, 4, 6, 8, 10, 13, 14};

// Subtrim values for each servo (offset in degrees)
int8_t servoSubtrim[8] = {0, 0, 0, 0, 0, 0, 0, 0};


// Animation constants
int frameDelay = 60;
int walkCycles = 10;
int motorCurrentDelay = 8; // ms delay between motor movements to prevent over-current

struct FaceEntry {
  const char* name;
  const unsigned char* const* frames;
  uint8_t maxFrames;
};

static const uint8_t MAX_FACE_FRAMES = 6;

#define MAKE_GENERATED_FACE_FRAMES(name) \
  const unsigned char* const face_gen_##name##_frames[] = { \
    epd_bitmap_gen_##name, nullptr, nullptr, nullptr, nullptr, nullptr \
  };

MAKE_GENERATED_FACE_FRAMES(default)
MAKE_GENERATED_FACE_FRAMES(happy)
MAKE_GENERATED_FACE_FRAMES(sad)
MAKE_GENERATED_FACE_FRAMES(angry)
MAKE_GENERATED_FACE_FRAMES(surprised)
MAKE_GENERATED_FACE_FRAMES(sleepy)
MAKE_GENERATED_FACE_FRAMES(love)
MAKE_GENERATED_FACE_FRAMES(confused)
MAKE_GENERATED_FACE_FRAMES(thinking)
MAKE_GENERATED_FACE_FRAMES(excited)
MAKE_GENERATED_FACE_FRAMES(dead)
MAKE_GENERATED_FACE_FRAMES(wink)
#undef MAKE_GENERATED_FACE_FRAMES

const FaceEntry faceEntries[] = {
  { "default", face_gen_default_frames, MAX_FACE_FRAMES },
  { "happy", face_gen_happy_frames, MAX_FACE_FRAMES },
  { "talk_happy", face_gen_happy_frames, MAX_FACE_FRAMES },
  { "sad", face_gen_sad_frames, MAX_FACE_FRAMES },
  { "talk_sad", face_gen_sad_frames, MAX_FACE_FRAMES },
  { "angry", face_gen_angry_frames, MAX_FACE_FRAMES },
  { "talk_angry", face_gen_angry_frames, MAX_FACE_FRAMES },
  { "surprised", face_gen_surprised_frames, MAX_FACE_FRAMES },
  { "talk_surprised", face_gen_surprised_frames, MAX_FACE_FRAMES },
  { "sleepy", face_gen_sleepy_frames, MAX_FACE_FRAMES },
  { "talk_sleepy", face_gen_sleepy_frames, MAX_FACE_FRAMES },
  { "love", face_gen_love_frames, MAX_FACE_FRAMES },
  { "talk_love", face_gen_love_frames, MAX_FACE_FRAMES },
  { "confused", face_gen_confused_frames, MAX_FACE_FRAMES },
  { "talk_confused", face_gen_confused_frames, MAX_FACE_FRAMES },
  { "thinking", face_gen_thinking_frames, MAX_FACE_FRAMES },
  { "talk_thinking", face_gen_thinking_frames, MAX_FACE_FRAMES },
  { "excited", face_gen_excited_frames, MAX_FACE_FRAMES },
  { "talk_excited", face_gen_excited_frames, MAX_FACE_FRAMES },
  { "dead", face_gen_dead_frames, MAX_FACE_FRAMES },
  { "wink", face_gen_wink_frames, MAX_FACE_FRAMES },
  { "rest", face_gen_default_frames, MAX_FACE_FRAMES },
  { "stand", face_gen_default_frames, MAX_FACE_FRAMES },
  { "idle", face_gen_default_frames, MAX_FACE_FRAMES },
  { "idle_blink", face_gen_wink_frames, MAX_FACE_FRAMES },
  { "walk", face_gen_happy_frames, MAX_FACE_FRAMES },
  { "wave", face_gen_wink_frames, MAX_FACE_FRAMES },
  { "dance", face_gen_excited_frames, MAX_FACE_FRAMES },
  { "swim", face_gen_happy_frames, MAX_FACE_FRAMES },
  { "point", face_gen_confused_frames, MAX_FACE_FRAMES },
  { "pushup", face_gen_excited_frames, MAX_FACE_FRAMES },
  { "bow", face_gen_sleepy_frames, MAX_FACE_FRAMES },
  { "cute", face_gen_love_frames, MAX_FACE_FRAMES },
  { "freaky", face_gen_surprised_frames, MAX_FACE_FRAMES },
  { "worm", face_gen_confused_frames, MAX_FACE_FRAMES },
  { "shake", face_gen_surprised_frames, MAX_FACE_FRAMES },
  { "shrug", face_gen_confused_frames, MAX_FACE_FRAMES },
  { "crab", face_gen_angry_frames, MAX_FACE_FRAMES }
};

struct FaceFpsEntry {
  const char* name;
  uint8_t fps;
};

const FaceFpsEntry faceFpsEntries[] = {
  { "walk", 1 },
  { "rest", 1 },
  { "swim", 1 },
  { "dance", 1 },
  { "wave", 1 },
  { "point", 5 },
  { "stand", 1 },
  { "cute", 1 },
  { "pushup", 1 },
  { "freaky", 1 },
  { "bow", 1 },
  { "worm", 1 },
  { "shake", 1 },
  { "shrug", 1 },
  { "dead", 2 },
  { "crab", 1 },
  { "idle", 1 },
  { "idle_blink", 7 },
  { "default", 1 },
  // Conversational faces (manually controlled by Python - no auto-animation)
  { "happy", 1 },
  { "talk_happy", 1 },
  { "sad", 1 },
  { "talk_sad", 1 },
  { "angry", 1 },
  { "talk_angry", 1 },
  { "surprised", 1 },
  { "talk_surprised", 1 },
  { "sleepy", 1 },
  { "talk_sleepy", 1 },
  { "love", 1 },
  { "talk_love", 1 },
  { "excited", 1 },
  { "talk_excited", 1 },
  { "confused", 1 },
  { "talk_confused", 1 },
  { "thinking", 1 },
  { "talk_thinking", 1 },
};


// Prototypes
void setServoAngle(uint8_t channel, int angle);
void updateFaceBitmap(const unsigned char* bitmap);
void showWifiSetupScreen();
void setFace(const String& faceName);
void setFaceMode(FaceAnimMode mode);
void setFaceWithMode(const String& faceName, FaceAnimMode mode);
void updateAnimatedFace();
void delayWithFace(unsigned long ms);
void enterIdle();
void exitIdle();
void updateIdleBlink();
int getFaceFpsForName(const String& faceName);
bool pressingCheck(String cmd, int ms);
void handleGetSettings();
void handleSetSettings();
void handleGetStatus();
void handleApiCommand();
bool beginSsd1327Display();
void runDisplayPowerOnTest();
void showDisplayBootDiagnostic();
void pollTrioeHub();
int trioeHttpGet(const String& url, String& response, uint16_t timeoutMs);
int trioeHttpPost(const String& url, const String& payload, uint16_t timeoutMs);
void addTrioeApiHeaders(HTTPClient& http, bool includeJsonContentType = false);
void applyTrioeCommand(const String& commandValue, const String& updateToken);
void applyTrioeJoystick(int x, int y);
void acknowledgeTrioeCommandComplete(const String& command, const String& updateToken);
String normalizeTrioeCommand(String value);
bool isSesameCommand(const String& command);
bool isContinuousMovementCommand(const String& command);
bool shouldPollTrioeDuringDelays();
void updateWifiInfoScroll();
void recordInput();
void serviceLocalRemote();
void maintainNetworkConnection();

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleCommandWeb() {
  // We send 200 OK immediately so the web browser doesn't hang waiting for animation to finish
  if (server.hasArg("pose")) {
    currentCommand = server.arg("pose");
    recordInput();
    exitIdle();
    server.send(200, "text/plain", "OK"); 
  } 
  else if (server.hasArg("go")) {
    currentCommand = server.arg("go");
    recordInput();
    exitIdle();
    server.send(200, "text/plain", "OK");
  } 
  else if (server.hasArg("stop")) {
    currentCommand = "";
    recordInput();
    server.send(200, "text/plain", "OK");
  }
  else if (server.hasArg("motor") && server.hasArg("value")) {
    int motorNum = server.arg("motor").toInt();
    int servoIdx = servoNameToIndex(server.arg("motor"));
    int angle = server.arg("value").toInt();
    if (motorNum >= 1 && motorNum <= 8 && angle >= 0 && angle <= 180) {
      setServoAngle(motorNum - 1, angle); // Convert 1-based to 0-based index
      recordInput();
      server.send(200, "text/plain", "OK");
    } else if (servoIdx != -1 && angle >= 0 && angle <= 180) {
      setServoAngle(servoIdx, angle);
      recordInput();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Invalid motor or angle");
    }
  }
  else {
    server.send(400, "text/plain", "Bad Args");
  }
}

void handleGetSettings() {
  String json = "{";
  json += "\"frameDelay\":" + String(frameDelay) + ",";
  json += "\"walkCycles\":" + String(walkCycles) + ",";
  json += "\"motorCurrentDelay\":" + String(motorCurrentDelay) + ",";
  json += "\"faceFps\":" + String(faceFps);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetSettings() {
  if (server.hasArg("frameDelay")) frameDelay = server.arg("frameDelay").toInt();
  if (server.hasArg("walkCycles")) walkCycles = server.arg("walkCycles").toInt();
  if (server.hasArg("motorCurrentDelay")) motorCurrentDelay = server.arg("motorCurrentDelay").toInt();
  if (server.hasArg("faceFps")) faceFps = (int)max(1L, server.arg("faceFps").toInt());
  server.send(200, "text/plain", "OK");
}

// API endpoint for network clients to get robot status
void handleGetStatus() {
  String json = "{";
  json += "\"currentCommand\":\"" + currentCommand + "\",";
  json += "\"currentFace\":\"" + currentFaceName + "\",";
  json += "\"networkConnected\":" + String(networkConnected ? "true" : "false") + ",";
  json += "\"apIP\":\"" + WiFi.softAPIP().toString() + "\"";
  if (networkConnected) {
    json += ",\"networkIP\":\"" + networkIP.toString() + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

// API endpoint for network clients to send commands (JSON-based)
void handleApiCommand() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json", "{\"error\":\"Method not allowed\"}");
    return;
  }
  
  String body = server.arg("plain");
  
  Serial.println("API Command received:");
  Serial.println(body);
  
  // Check for face-only command (no movement)
  int faceOnlyStart = body.indexOf("\"face\":\"");
  if (faceOnlyStart == -1) {
    faceOnlyStart = body.indexOf("\"face\": \"");
  }
  
  // If we have a face but no command field, it's face-only
  bool faceOnly = (faceOnlyStart > 0 && body.indexOf("\"command\":") == -1 && body.indexOf("\"command\": ") == -1);
  
  String command = "";
  String face = "";
  
  // Parse face
  if (faceOnlyStart > 0) {
    faceOnlyStart = body.indexOf("\"", faceOnlyStart + 6) + 1;
    int faceEnd = body.indexOf("\"", faceOnlyStart);
    if (faceEnd > faceOnlyStart) {
      face = body.substring(faceOnlyStart, faceEnd);
      Serial.print("Parsed face: ");
      Serial.println(face);
    }
  }
  
  // Parse command (if not face-only)
  if (!faceOnly) {
    int cmdStart = body.indexOf("\"command\":\"");
    if (cmdStart == -1) {
      cmdStart = body.indexOf("\"command\": \"");
    }
    
    if (cmdStart == -1) {
      Serial.println("Error: command field not found");
      server.send(400, "application/json", "{\"error\":\"Missing command field\"}");
      return;
    }
    
    cmdStart = body.indexOf("\"", cmdStart + 10) + 1;
    int cmdEnd = body.indexOf("\"", cmdStart);
    
    if (cmdEnd <= cmdStart) {
      Serial.println("Error: invalid command format");
      server.send(400, "application/json", "{\"error\":\"Invalid command format\"}");
      return;
    }
    
    command = body.substring(cmdStart, cmdEnd);
    Serial.print("Parsed command: ");
    Serial.println(command);
  }
  
  // Set face if provided
  if (face.length() > 0) {
    setFace(face);
  }
  
  // If face-only, just acknowledge
  if (faceOnly) {
    recordInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Face updated\"}");
    return;
  }
  
  // Execute command
  if (command == "stop") {
    currentCommand = "";
    recordInput();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command stopped\"}");
  } else {
    currentCommand = command;
    recordInput();
    exitIdle();
    server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Command executed\"}");
  }
}

bool beginSsd1327Display() {
  activeI2cSda = I2C_SDA;
  activeI2cScl = I2C_SCL;
  activeOledAddr = OLED_I2C_ADDR;

  pinMode(activeI2cSda, INPUT_PULLUP);
  pinMode(activeI2cScl, INPUT_PULLUP);
  Wire.begin(activeI2cSda, activeI2cScl);
  Wire.setClock(100000);
  Wire.setTimeOut(20);

  Serial.print(F("SSD1327 I2C SDA="));
  Serial.print(activeI2cSda);
  Serial.print(F(" SCL="));
  Serial.print(activeI2cScl);
  Serial.print(F(" ADDR=0x"));
  if (activeOledAddr < 16) Serial.print("0");
  Serial.println(activeOledAddr, HEX);

  if (!display.begin(activeOledAddr)) {
    Serial.println(F("SSD1327 begin failed."));
    return false;
  }

  display.setRotation(DISPLAY_ROTATION);
  display.oled_command(SSD1327_SEGREMAP);
  display.oled_command(DISPLAY_SEG_REMAP);
  display.invertDisplay(false);
  display.clearDisplay();
  display.display();
  return true;
}

void showDisplayBootDiagnostic() {
  if (!displayReady) return;

  display.clearDisplay();
  display.setTextColor(SSD1327_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Hello, World!"));
  display.println(F("128x128 OLED"));
  display.println(F("Grayscale Display"));
  display.println();
  display.println(F("SSD1327 OK"));
  display.print(F("SDA "));
  display.print(activeI2cSda);
  display.print(F(" SCL "));
  display.println(activeI2cScl);
  display.print(F("ADDR 0x"));
  if (activeOledAddr < 16) display.print("0");
  display.println(activeOledAddr, HEX);
  display.display();
  delay(1200);
}

void runDisplayPowerOnTest() {
  if (!displayReady) return;

  display.clearDisplay();
  display.fillScreen(SSD1327_WHITE);
  display.display();
  delay(700);

  display.clearDisplay();
  display.fillScreen(SSD1327_BLACK);
  display.display();
  delay(700);

  display.clearDisplay();
  for (int x = 0; x < SCREEN_WIDTH; x += 16) {
    display.fillRect(x, 0, 8, SCREEN_HEIGHT, SSD1327_WHITE);
  }
  display.display();
  delay(700);

  display.clearDisplay();
  for (int y = 0; y < SCREEN_HEIGHT; y += 16) {
    display.fillRect(0, y, SCREEN_WIDTH, 8, SSD1327_WHITE);
  }
  display.display();
  delay(700);

  display.clearDisplay();
  display.display();
  delay(200);
}

void setup() {
  Serial.begin(115200);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 5000) {
    delay(10);
  }
  delay(250);
  Serial.println(F("Sesame firmware booting."));
  randomSeed(micros());
  
  displayReady = beginSsd1327Display();
  if (!displayReady) {
    Serial.println(F("SSD1327 I2C display not found on the configured SDA/SCL pairs. Continuing without OLED."));
  } else {
    Serial.print(F("SSD1327 started at 0x"));
    if (activeOledAddr < 16) Serial.print("0");
    Serial.print(activeOledAddr, HEX);
    Serial.print(F(" SDA="));
    Serial.print(activeI2cSda);
    Serial.print(F(" SCL="));
    Serial.println(activeI2cScl);

    runDisplayPowerOnTest();
    showDisplayBootDiagnostic();
    showWifiSetupScreen();
  }

  // --- WIFI CONFIGURATION ---
  // Try to connect to network first if configured
  if (ENABLE_NETWORK_MODE && String(NETWORK_SSID).length() > 0) {
    Serial.println("Attempting to connect to network: " + String(NETWORK_SSID));
    WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes
    WiFi.setHostname(deviceHostname.c_str());
    WiFi.begin(NETWORK_SSID, NETWORK_PASS);
    
    // Wait up to 10 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      networkConnected = true;
      networkIP = WiFi.localIP();
      Serial.println();
      Serial.print("Connected to network! IP: ");
      Serial.println(networkIP);
    } else {
      Serial.println();
      Serial.println("Failed to connect to network. Running in AP-only mode.");
      WiFi.mode(WIFI_AP); // Fall back to AP-only
    }
  } else {
    WiFi.mode(WIFI_AP);
    Serial.println("Network mode disabled. Running in AP-only mode.");
  }
  
  const bool trioeHubConnectedMode = ENABLE_TRIOE_HUB_CONTROL &&
    DISABLE_HOTSPOT_WHEN_TRIOE_CONNECTED &&
    networkConnected;

  if (trioeHubConnectedMode) {
    localRemoteEnabled = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    wifiInfoText = "Network: " + String(NETWORK_SSID) + " (" + networkIP.toString() + ")  |  Trioe Hub control active  |  ";
    Serial.println(F("Trioe Hub connected mode: hotspot, captive portal, and local remote disabled."));
  } else {
    localRemoteEnabled = true;

    // --- ACCESS POINT CONFIGURATION ---
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress myIP = WiFi.softAPIP();
    
    Serial.print("AP Created. IP: ");
    Serial.println(myIP);

    // Build WiFi info text for scrolling
    if (networkConnected) {
      wifiInfoText = "AP: " + String(AP_SSID) + " (" + myIP.toString() + ")  |  Network: " + String(NETWORK_SSID) + " (" + networkIP.toString() + ") or " + deviceHostname + ".local  |  ";
    } else {
      wifiInfoText = "Connect to WiFi: " + String(AP_SSID) + "  |  Pass: " + String(AP_PASS) + "  |  IP: " + myIP.toString() + "  |  Captive Portal will auto-open!  |  ";
    }
  }
  
  // Initialize input tracking
  lastInputTime = millis();
  firstInputReceived = false;
  showingWifiInfo = false;

  if (localRemoteEnabled) {
    // Start mDNS responder for local network discovery
    if (MDNS.begin(deviceHostname.c_str())) {
      Serial.println("mDNS responder started");
      Serial.print("Access controller at: http://");
      Serial.print(deviceHostname);
      Serial.println(".local");
      MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("Error setting up mDNS responder!");
    }

    // Start DNS Server for Captive Portal
    // This redirects ALL domain requests to the ESP32's IP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // Web Server Routes
    server.on("/", handleRoot);
    server.on("/cmd", handleCommandWeb);
    server.on("/getSettings", handleGetSettings);
    server.on("/setSettings", handleSetSettings);
    
    // API endpoints for network communication
    server.on("/api/status", handleGetStatus);
    server.on("/api/command", handleApiCommand);
    
    // Catch-all route for captive portal
    // This ensures any URL redirects to the controller page
    server.onNotFound(handleRoot);
    
    server.begin();
  }

  // PWM Init
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  for (int i = 0; i < 8; i++) {
    servos[i].setPeriodHertz(50);
    // Map 0-180 to approx 732-2929us
    servos[i].attach(servoPins[i], 732, 2929);
  }
  delay(10);

  // Center every servo on boot.
  for (int i = 0; i < 8; i++) {
    setServoAngle(i, 90);
  }
  
  // Show the native 128x128 default face on startup.
  setFace("default");
  
  if (localRemoteEnabled) {
    Serial.println(F("HTTP server & Captive Portal started."));
  } else {
    Serial.println(F("Local HTTP remote disabled for Trioe Hub control."));
  }
}

void loop() {
  if (millis() - lastHeartbeatMs >= 5000) {
    lastHeartbeatMs = millis();
    Serial.println(F("loop alive"));
  }

  maintainNetworkConnection();
  serviceLocalRemote();
  updateAnimatedFace();
  updateIdleBlink();
  updateWifiInfoScroll();
  pollTrioeHub();

  if (currentCommand != "") {
    String cmd = currentCommand;
    String cmdUpdateToken = currentCommandUpdateToken;
    executingCommand = cmd;
    if (ENABLE_TRIOE_DEBUG_PRINTS) {
      Serial.print(F("Executing command: "));
      Serial.println(cmd);
    }
    if (cmd == "forward") runWalkPose();
    else if (cmd == "backward") runWalkBackward();
    else if (cmd == "left") runTurnLeft();
    else if (cmd == "right") runTurnRight();
    else if (cmd == "rest") { runRestPose(); if (currentCommand == "rest") currentCommand = ""; }
    else if (cmd == "stand") { runStandPose(1); if (currentCommand == "stand") currentCommand = ""; }
    else if (cmd == "wave") runWavePose();
    else if (cmd == "dance") runDancePose();
    else if (cmd == "swim") runSwimPose();
    else if (cmd == "point") runPointPose();
    else if (cmd == "pushup") runPushupPose();
    else if (cmd == "bow") runBowPose();
    else if (cmd == "cute") runCutePose();
    else if (cmd == "freaky") runFreakyPose();
    else if (cmd == "worm") runWormPose();
    else if (cmd == "shake") runShakePose();
    else if (cmd == "shrug") runShrugPose();
    else if (cmd == "dead") runDeadPose();
    else if (cmd == "crab") runCrabPose();
    if (ENABLE_TRIOE_DEBUG_PRINTS) {
      Serial.print(F("Command finished/loop state: requested="));
      Serial.print(cmd);
      Serial.print(F(" currentCommand="));
      Serial.println(currentCommand.length() ? currentCommand : "(empty)");
    }
    acknowledgeTrioeCommandComplete(cmd, cmdUpdateToken);
    executingCommand = "";
    if (currentCommand.length() == 0) {
      currentCommandUpdateToken = "";
    }
  }
  
  // Serial CLI for debugging (can be used to diagnose servo position issues and wiring)
  if (Serial.available()) {
    static char command_buffer[32];
    static byte buffer_pos = 0;
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (buffer_pos > 0) {
        command_buffer[buffer_pos] = '\0';
        int motorNum, angle;
        recordInput();
        if(strcmp(command_buffer, "run walk") == 0 || strcmp(command_buffer, "rn wf") == 0) { currentCommand = "forward"; runWalkPose(); currentCommand = ""; }
        else if(strcmp(command_buffer, "rn wb") == 0) { currentCommand = "backward"; runWalkBackward(); currentCommand = ""; }
        else if(strcmp(command_buffer, "rn tl") == 0) { currentCommand = "left"; runTurnLeft(); currentCommand = ""; }
        else if(strcmp(command_buffer, "rn tr") == 0) { currentCommand = "right"; runTurnRight(); currentCommand = ""; }
        else if(strcmp(command_buffer, "run rest") == 0 || strcmp(command_buffer, "rn rs") == 0) runRestPose();
        else if(strcmp(command_buffer, "run stand") == 0 || strcmp(command_buffer, "rn st") == 0) runStandPose(1);
        else if(strcmp(command_buffer, "rn wv") == 0) { currentCommand = "wave"; runWavePose(); }
        else if(strcmp(command_buffer, "rn dn") == 0) { currentCommand = "dance"; runDancePose(); }
        else if(strcmp(command_buffer, "rn sw") == 0) { currentCommand = "swim"; runSwimPose(); }
        else if(strcmp(command_buffer, "rn pt") == 0) { currentCommand = "point"; runPointPose(); }
        else if(strcmp(command_buffer, "rn pu") == 0) { currentCommand = "pushup"; runPushupPose(); }
        else if(strcmp(command_buffer, "rn bw") == 0) { currentCommand = "bow"; runBowPose(); }
        else if(strcmp(command_buffer, "rn ct") == 0) { currentCommand = "cute"; runCutePose(); }
        else if(strcmp(command_buffer, "rn fk") == 0) { currentCommand = "freaky"; runFreakyPose(); }
        else if(strcmp(command_buffer, "rn wm") == 0) { currentCommand = "worm"; runWormPose(); }
        else if(strcmp(command_buffer, "rn sk") == 0) { currentCommand = "shake"; runShakePose(); }
        else if(strcmp(command_buffer, "rn sg") == 0) { currentCommand = "shrug"; runShrugPose(); }
        else if(strcmp(command_buffer, "rn dd") == 0) { currentCommand = "dead"; runDeadPose(); }
        else if(strcmp(command_buffer, "rn cb") == 0) { currentCommand = "crab"; runCrabPose(); }
        else if (strcmp(command_buffer, "subtrim") == 0 || strcmp(command_buffer, "st") == 0) {
          Serial.println("Subtrim values:");
          for (int i = 0; i < 8; i++) {
            Serial.print("Motor "); Serial.print(i); Serial.print(": ");
            if (servoSubtrim[i] >= 0) Serial.print("+");
            Serial.println(servoSubtrim[i]);
          }
        }
        else if (strcmp(command_buffer, "subtrim save") == 0 || strcmp(command_buffer, "st save") == 0) {
          Serial.println("Copy and paste this into your code:");
          Serial.print("int8_t servoSubtrim[8] = {");
          for (int i = 0; i < 8; i++) {
            Serial.print(servoSubtrim[i]);
            if (i < 7) Serial.print(", ");
          }
          Serial.println("};");
        }
        else if (strncmp(command_buffer, "subtrim reset", 13) == 0 || strncmp(command_buffer, "st reset", 8) == 0) {
          for (int i = 0; i < 8; i++) servoSubtrim[i] = 0;
          Serial.println("All subtrim values reset to 0");
        }
        else if (strncmp(command_buffer, "subtrim ", 8) == 0 || strncmp(command_buffer, "st ", 3) == 0) {
          const char* params = (command_buffer[1] == 't') ? command_buffer + 3 : command_buffer + 8;
          int trimMotor, trimValue;
          if (sscanf(params, "%d %d", &trimMotor, &trimValue) == 2) {
            if (trimMotor >= 0 && trimMotor < 8) {
              if (trimValue >= -90 && trimValue <= 90) {
                servoSubtrim[trimMotor] = trimValue;
                Serial.print("Motor "); Serial.print(trimMotor); Serial.print(" subtrim set to ");
                if (trimValue >= 0) Serial.print("+");
                Serial.println(trimValue);
              } else {
                Serial.println("Subtrim value must be between -90 and +90");
              }
            } else {
              Serial.println("Invalid motor number (0-7)");
            }
          }
        }
        else if (strncmp(command_buffer, "all ", 4) == 0) {
             if (sscanf(command_buffer + 4, "%d", &angle) == 1) {
                 for (int i = 0; i < 8; i++) setServoAngle(i, angle);
                 Serial.print("All servos set to "); Serial.println(angle);
             }
        }
        else if (sscanf(command_buffer, "%d %d", &motorNum, &angle) == 2) {
             if (motorNum >= 0 && motorNum < 8) {
                 setServoAngle(motorNum, angle);
                 Serial.print("Servo "); Serial.print(motorNum); Serial.print(" set to "); Serial.println(angle);
             } else {
                 Serial.println("Invalid motor number (0-7)");
             }
        }
        buffer_pos = 0;
      }
    } else if (buffer_pos < sizeof(command_buffer) - 1) {
      command_buffer[buffer_pos++] = c;
    }
  }
}

void drawFaceBitmap(const unsigned char* bitmap) {
  if (bitmap == nullptr) return;
  const uint16_t foreground = FACE_BITMAP_INVERT ? SSD1327_BLACK : SSD1327_WHITE;
  const uint16_t background = FACE_BITMAP_INVERT ? SSD1327_WHITE : SSD1327_BLACK;
  display.drawBitmap(0, 0, bitmap, FACE_BITMAP_WIDTH, FACE_BITMAP_HEIGHT, foreground, background);
}

void drawWifiBlockWord(int x, int y) {
  const int t = 4;
  const int h = 30;

  // W
  display.fillRect(x, y, t, h, SSD1327_WHITE);
  display.fillRect(x + 24, y, t, h, SSD1327_WHITE);
  display.fillRect(x + 9, y + 14, t, 16, SSD1327_WHITE);
  display.fillRect(x + 15, y + 14, t, 16, SSD1327_WHITE);
  display.fillRect(x + 4, y + 26, 24, t, SSD1327_WHITE);
  x += 34;

  // I
  display.fillRect(x, y, 18, t, SSD1327_WHITE);
  display.fillRect(x + 7, y, t, h, SSD1327_WHITE);
  display.fillRect(x, y + h - t, 18, t, SSD1327_WHITE);
  x += 26;

  // F
  display.fillRect(x, y, t, h, SSD1327_WHITE);
  display.fillRect(x, y, 22, t, SSD1327_WHITE);
  display.fillRect(x, y + 13, 18, t, SSD1327_WHITE);
  x += 30;

  // I
  display.fillRect(x, y, 18, t, SSD1327_WHITE);
  display.fillRect(x + 7, y, t, h, SSD1327_WHITE);
  display.fillRect(x, y + h - t, 18, t, SSD1327_WHITE);
}

void drawThickArc(int cx, int cy, int radius, int stroke, int startDeg, int endDeg, uint16_t color) {
  int dotRadius = max(1, stroke / 2);
  for (int angle = startDeg; angle <= endDeg; angle += 2) {
    float rad = angle * PI / 180.0f;
    int x = cx + (int)round(cos(rad) * radius);
    int y = cy + (int)round(sin(rad) * radius);
    display.fillCircle(x, y, dotRadius, color);
  }
}

void drawWifiSetupBitmap() {
  const int cx = 64;

  display.drawRoundRect(16, 22, 96, 82, 14, SSD1327_WHITE);
  display.drawRoundRect(17, 23, 94, 80, 13, SSD1327_WHITE);
  display.drawRoundRect(18, 24, 92, 78, 12, SSD1327_WHITE);

  display.fillRoundRect(48, 14, 32, 8, 3, SSD1327_WHITE);
  display.fillRect(61, 5, 6, 12, SSD1327_WHITE);
  display.fillCircle(cx, 5, 7, SSD1327_WHITE);

  display.fillRoundRect(8, 52, 8, 28, 3, SSD1327_WHITE);
  display.fillRoundRect(112, 52, 8, 28, 3, SSD1327_WHITE);

  drawThickArc(cx, 76, 35, 6, 215, 325, SSD1327_WHITE);
  drawThickArc(cx, 76, 24, 6, 220, 320, SSD1327_WHITE);
  drawThickArc(cx, 76, 13, 6, 230, 310, SSD1327_WHITE);
  display.fillCircle(cx, 80, 5, SSD1327_WHITE);

  display.fillRoundRect(32, 112, 18, 8, 3, SSD1327_WHITE);
  display.fillRoundRect(55, 112, 18, 8, 3, SSD1327_WHITE);
  display.fillRoundRect(78, 112, 18, 8, 3, SSD1327_WHITE);
}

void showWifiSetupScreen() {
  if (!displayReady) return;

  display.clearDisplay();
  drawWifiSetupBitmap();
  display.display();
}

// Function to update the robot's face
void updateFaceBitmap(const unsigned char* bitmap) {
  if (!displayReady) return;
  display.clearDisplay();
  drawFaceBitmap(bitmap);
  display.display();
}

uint8_t countFrames(const unsigned char* const* frames, uint8_t maxFrames) {
  if (frames == nullptr || frames[0] == nullptr) return 0;
  uint8_t count = 0;
  for (uint8_t i = 0; i < maxFrames; i++) {
    if (frames[i] == nullptr) break;
    count++;
  }
  return count;
}

void setFace(const String& faceName) {
  if (faceName == currentFaceName && currentFaceFrames != nullptr) return;

  currentFaceName = faceName;
  currentFaceFrameIndex = 0;
  lastFaceFrameMs = 0;
  faceFrameDirection = 1;
  faceAnimFinished = false;
  currentFaceFps = getFaceFpsForName(faceName);

  currentFaceFrames = face_gen_default_frames;
  currentFaceFrameCount = countFrames(face_gen_default_frames, MAX_FACE_FRAMES);

  for (size_t i = 0; i < (sizeof(faceEntries) / sizeof(faceEntries[0])); i++) {
    if (faceName.equalsIgnoreCase(faceEntries[i].name)) {
      currentFaceFrames = faceEntries[i].frames;
      currentFaceFrameCount = countFrames(faceEntries[i].frames, faceEntries[i].maxFrames);
      break;
    }
  }

  if (currentFaceFrameCount == 0) {
    currentFaceFrames = face_gen_default_frames;
    currentFaceFrameCount = countFrames(face_gen_default_frames, MAX_FACE_FRAMES);
    currentFaceName = "default";
    currentFaceFps = getFaceFpsForName(currentFaceName);
  }

  if (currentFaceFrameCount > 0 && currentFaceFrames[0] != nullptr) {
    updateFaceBitmap(currentFaceFrames[0]);
  }
}

void setFaceMode(FaceAnimMode mode) {
  currentFaceMode = mode;
  faceFrameDirection = 1;
  faceAnimFinished = false;
}

void setFaceWithMode(const String& faceName, FaceAnimMode mode) {
  setFaceMode(mode);
  setFace(faceName);
}

int getFaceFpsForName(const String& faceName) {
  for (size_t i = 0; i < (sizeof(faceFpsEntries) / sizeof(faceFpsEntries[0])); i++) {
    if (faceName.equalsIgnoreCase(faceFpsEntries[i].name)) {
      return faceFpsEntries[i].fps;
    }
  }
  return faceFps;
}

void updateAnimatedFace() {
  if (currentFaceFrames == nullptr || currentFaceFrameCount <= 1) return;
  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished) return;

  unsigned long now = millis();
  int fps = max(1, (currentFaceFps > 0 ? currentFaceFps : faceFps));
  unsigned long interval = 1000UL / fps;
  if (now - lastFaceFrameMs >= interval) {
    lastFaceFrameMs = now;
    if (currentFaceMode == FACE_ANIM_LOOP) {
      currentFaceFrameIndex = (currentFaceFrameIndex + 1) % currentFaceFrameCount;
    } else if (currentFaceMode == FACE_ANIM_ONCE) {
      if (currentFaceFrameIndex + 1 >= currentFaceFrameCount) {
        currentFaceFrameIndex = currentFaceFrameCount - 1;
        faceAnimFinished = true;
      } else {
        currentFaceFrameIndex++;
      }
    } else {
      if (faceFrameDirection > 0) {
        if (currentFaceFrameIndex + 1 >= currentFaceFrameCount) {
          faceFrameDirection = -1;
          if (currentFaceFrameIndex > 0) currentFaceFrameIndex--;
        } else {
          currentFaceFrameIndex++;
        }
      } else {
        if (currentFaceFrameIndex == 0) {
          faceFrameDirection = 1;
          if (currentFaceFrameCount > 1) currentFaceFrameIndex++;
        } else {
          currentFaceFrameIndex--;
        }
      }
    }
    updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
  }
}

void delayWithFace(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    updateAnimatedFace();
    if (shouldPollTrioeDuringDelays()) {
      pollTrioeHub();
    }
    serviceLocalRemote();
    delay(5);
  }
}

void scheduleNextIdleBlink(unsigned long minMs, unsigned long maxMs) {
  unsigned long now = millis();
  unsigned long interval = (unsigned long)random(minMs, maxMs);
  nextIdleBlinkMs = now + interval;
}

void enterIdle() {
  idleActive = true;
  idleBlinkActive = false;
  idleBlinkRepeatsLeft = 0;
  setFaceWithMode("idle", FACE_ANIM_BOOMERANG);
  scheduleNextIdleBlink(3000, 7000);
}

void exitIdle() {
  idleActive = false;
  idleBlinkActive = false;
}

void updateIdleBlink() {
  if (!idleActive) return;

  if (!idleBlinkActive) {
    if (millis() >= nextIdleBlinkMs) {
      idleBlinkActive = true;
      if (idleBlinkRepeatsLeft == 0 && random(0, 100) < 30) {
        idleBlinkRepeatsLeft = 1; // double blink
      }
      setFaceWithMode("idle_blink", FACE_ANIM_ONCE);
    }
    return;
  }

  if (currentFaceMode == FACE_ANIM_ONCE && faceAnimFinished) {
    idleBlinkActive = false;
    setFaceWithMode("idle", FACE_ANIM_BOOMERANG);
    if (idleBlinkRepeatsLeft > 0) {
      idleBlinkRepeatsLeft--;
      scheduleNextIdleBlink(120, 220);
    } else {
      scheduleNextIdleBlink(3000, 7000);
    }
  }
}

// ====== HELPERS ======
void setServoAngle(uint8_t channel, int angle) { 
  if (channel < 8) {
    int adjustedAngle = constrain(angle + servoSubtrim[channel], 0, 180);
    if (ENABLE_SERVO_DEBUG_PRINTS) {
      Serial.print(F("Servo write: "));
      Serial.print(ServoNames[channel]);
      Serial.print(F(" target="));
      Serial.print(angle);
      Serial.print(F(" trim="));
      Serial.print(servoSubtrim[channel]);
      Serial.print(F(" adjusted="));
      Serial.print(adjustedAngle);
      Serial.print(F(" pin="));
      Serial.println(servoPins[channel]);
    }
    servos[channel].write(adjustedAngle);
    delayWithFace(motorCurrentDelay);
  }
}

bool pressingCheck(String cmd, int ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    pollTrioeHub();
    serviceLocalRemote();
    updateAnimatedFace();
    if (currentCommand != cmd) {
      runStandPose(1);
      return false;
    }
    yield();
  }
  return true;
}

void addTrioeApiHeaders(HTTPClient& http, bool includeJsonContentType) {
  String apiKey = TRIOE_API_KEY;
  if (apiKey.length() > 0 && apiKey != "YOUR_8_DIGIT_API_KEY") {
    http.addHeader("X-API-Key", apiKey);
    http.addHeader("Authorization", "Bearer " + apiKey);
  }
  if (includeJsonContentType) {
    http.addHeader("Content-Type", "application/json");
  }
}

int trioeHttpGet(const String& url, String& response, uint16_t timeoutMs) {
  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;
  http.setTimeout(timeoutMs);
  http.setConnectTimeout(timeoutMs);
  http.setReuse(false);

  bool httpStarted = false;
  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    secureClient.setTimeout(timeoutMs);
    httpStarted = http.begin(secureClient, url);
  } else {
    client.setTimeout(timeoutMs);
    httpStarted = http.begin(client, url);
  }

  if (!httpStarted) return -1000;
  addTrioeApiHeaders(http);

  int statusCode = http.GET();
  if (statusCode == HTTP_CODE_OK) {
    response = http.getString();
  }
  http.end();
  return statusCode;
}

int trioeHttpPost(const String& url, const String& payload, uint16_t timeoutMs) {
  HTTPClient http;
  WiFiClient client;
  WiFiClientSecure secureClient;
  http.setTimeout(timeoutMs);
  http.setConnectTimeout(timeoutMs);
  http.setReuse(false);

  bool httpStarted = false;
  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    secureClient.setTimeout(timeoutMs);
    httpStarted = http.begin(secureClient, url);
  } else {
    client.setTimeout(timeoutMs);
    httpStarted = http.begin(client, url);
  }

  if (!httpStarted) return -1000;
  addTrioeApiHeaders(http, true);

  int statusCode = http.POST(payload);
  http.end();
  return statusCode;
}

void pollTrioeHub() {
  if (!ENABLE_TRIOE_HUB_CONTROL || !networkConnected || WiFi.status() != WL_CONNECTED) return;
  if (!shouldPollTrioeDuringDelays()) return;
  if (trioePollInProgress) return;

  unsigned long now = millis();
  if (now < trioePollBackoffUntilMs) return;
  if (now - lastTrioePollMs < TRIOE_POLL_INTERVAL_MS) return;
  lastTrioePollMs = now;
  trioePollInProgress = true;
  unsigned long pollStartMs = millis();

  String payload = "";
  int statusCode = trioeHttpGet(TRIOE_HUB_DATA_URL, payload, TRIOE_HTTP_TIMEOUT_MS);

  if (statusCode == HTTP_CODE_OK) {
    trioePollBackoffUntilMs = 0;
    lastTrioePollErrorStatus = 0;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print(F("Trioe JSON parse failed: "));
      Serial.println(error.c_str());
      trioePollInProgress = false;
      return;
    }

    String commandValue = "";
    String commandNonceStream = String(TRIOE_COMMAND_STREAM) + "_nonce";
    String commandAckStream = String(TRIOE_COMMAND_STREAM) + "_ack";
    String commandAckNonceStream = String(TRIOE_COMMAND_STREAM) + "_ack_nonce";
    String joystickBase = String(TRIOE_JOYSTICK_STREAM);
    String joystickXStream = joystickBase + "_x";
    String joystickYStream = joystickBase + "_y";
    String xValue = "";
    String yValue = "";
    String commandUpdateToken = "";
    String commandAckValue = "";
    String commandAckToken = "";

    for (JsonObject stream : doc.as<JsonArray>()) {
      const char* name = stream["name"] | "";
      JsonVariant value = stream["current_value"];
      if (strcmp(name, TRIOE_COMMAND_STREAM) == 0) {
        commandValue = value.as<String>();
        if (!stream["last_updated"].isNull()) {
          commandUpdateToken = stream["last_updated"].as<String>();
        } else if (!stream["stream_id"].isNull()) {
          commandUpdateToken = stream["stream_id"].as<String>();
        }
      } else if (strcmp(name, commandNonceStream.c_str()) == 0) {
        commandUpdateToken = value.as<String>();
      } else if (strcmp(name, commandAckStream.c_str()) == 0) {
        commandAckValue = value.as<String>();
      } else if (strcmp(name, commandAckNonceStream.c_str()) == 0) {
        commandAckToken = value.as<String>();
      } else if (strcmp(name, joystickXStream.c_str()) == 0) {
        xValue = value.as<String>();
      } else if (strcmp(name, joystickYStream.c_str()) == 0) {
        yValue = value.as<String>();
      }
    }

    if (ENABLE_TRIOE_POLL_VALUE_PRINTS) {
      String snapshot = commandValue + "|" + commandUpdateToken + "|" + commandAckValue + "|" + commandAckToken + "|" + xValue + "|" + yValue;
      if (snapshot != lastTrioePollDebugSnapshot) {
        Serial.print(F("Trioe parsed: command='"));
        Serial.print(commandValue.length() ? commandValue : "(empty)");
        Serial.print(F("' token="));
        Serial.print(commandUpdateToken.length() ? commandUpdateToken : "(none)");
        Serial.print(F(" ack='"));
        Serial.print(commandAckValue.length() ? commandAckValue : "(empty)");
        Serial.print(F("' ackToken="));
        Serial.print(commandAckToken.length() ? commandAckToken : "(none)");
        Serial.print(F(" joystick=("));
        Serial.print(xValue.length() ? xValue : "?");
        Serial.print(F(","));
        Serial.print(yValue.length() ? yValue : "?");
        Serial.print(F(") pollMs="));
        Serial.println(millis() - pollStartMs);
        lastTrioePollDebugSnapshot = snapshot;
      }
    }

    if (xValue.length() > 0 && yValue.length() > 0) {
      applyTrioeJoystick(xValue.toInt(), yValue.toInt());
    }

    if (commandValue.length() > 0 &&
        !(commandValue == commandAckValue && commandUpdateToken.length() > 0 && commandUpdateToken == commandAckToken)) {
      applyTrioeCommand(commandValue, commandUpdateToken);
    } else if (ENABLE_TRIOE_DEBUG_PRINTS && commandValue.length() > 0 && commandValue == commandAckValue) {
      String ackedDebugKey = commandValue + "@" + commandUpdateToken;
      if (ackedDebugKey != lastTrioeDuplicateDebugKey) {
        Serial.print(F("Trioe command already acked: "));
        Serial.print(commandValue);
        Serial.print(F(" token="));
        Serial.println(commandUpdateToken.length() ? commandUpdateToken : "(none)");
        lastTrioeDuplicateDebugKey = ackedDebugKey;
      }
    }
  } else {
    if (statusCode < 0 || statusCode >= 500) {
      trioePollBackoffUntilMs = millis() + TRIOE_FAILURE_BACKOFF_MS;
    }
    if (statusCode != lastTrioePollErrorStatus || millis() - lastTrioePollErrorPrintMs >= 5000) {
      Serial.print(F("Trioe poll HTTP "));
      Serial.println(statusCode);
      lastTrioePollErrorStatus = statusCode;
      lastTrioePollErrorPrintMs = millis();
    }
  }

  trioePollInProgress = false;
}

void acknowledgeTrioeCommandComplete(const String& command, const String& updateToken) {
  if (!ENABLE_TRIOE_HUB_CONTROL || !networkConnected || WiFi.status() != WL_CONNECTED) return;
  if (command.length() == 0 || isContinuousMovementCommand(command)) return;

  String ackToken = updateToken.length() > 0 ? updateToken : String(millis());
  String payload = String("{\"updateCurrent\":true,\"streams\":[") +
    "{\"name\":\"" + String(TRIOE_COMMAND_STREAM) + "\",\"type\":\"string\",\"value\":\"done\",\"unit\":\"command\"}," +
    "{\"name\":\"" + String(TRIOE_COMMAND_STREAM) + "_ack\",\"type\":\"string\",\"value\":\"" + command + "\",\"unit\":\"command\"}," +
    "{\"name\":\"" + String(TRIOE_COMMAND_STREAM) + "_ack_nonce\",\"type\":\"number\",\"value\":" + ackToken + ",\"unit\":\"ms\"}" +
    "]}";

  int statusCode = trioeHttpPost(TRIOE_HUB_POST_URL, payload, TRIOE_ACK_TIMEOUT_MS);

  if (ENABLE_TRIOE_DEBUG_PRINTS) {
    Serial.print(F("Trioe command ack/clear: "));
    Serial.print(command);
    Serial.print(F(" HTTP "));
    Serial.println(statusCode);
  }
}

void applyTrioeCommand(const String& commandValue, const String& updateToken) {
  String command = normalizeTrioeCommand(commandValue);
  if (!isSesameCommand(command)) {
    if (ENABLE_TRIOE_DEBUG_PRINTS) {
      Serial.print(F("Trioe command ignored, unknown raw='"));
      Serial.print(commandValue);
      Serial.println(F("'"));
    }
    return;
  }

  if (command == lastTrioeCommandValue && !isContinuousMovementCommand(command) && updateToken == lastTrioeCommandUpdateToken) {
    String duplicateDebugKey = command + "@" + updateToken;
    if (ENABLE_TRIOE_DEBUG_PRINTS) {
      if (duplicateDebugKey != lastTrioeDuplicateDebugKey) {
        Serial.print(F("Trioe command duplicate ignored: "));
        Serial.print(command);
        Serial.print(F(" token="));
        Serial.println(updateToken.length() ? updateToken : "(none)");
        lastTrioeDuplicateDebugKey = duplicateDebugKey;
      }
    }
    return;
  }
  lastTrioeCommandValue = command;
  lastTrioeCommandUpdateToken = updateToken;
  lastTrioeDuplicateDebugKey = "";
  lastTrioeJoystickCommand = "";

  recordInput();

  if (command == "stop") {
    currentCommand = "";
    currentCommandUpdateToken = "";
    return;
  }

  currentCommand = command;
  currentCommandUpdateToken = updateToken;
  exitIdle();
  Serial.print(F("Trioe command queued: raw='"));
  Serial.print(commandValue);
  Serial.print(F("' normalized='"));
  Serial.print(command);
  Serial.print(F("' token="));
  Serial.println(updateToken.length() ? updateToken : "(none)");
  Serial.print(F("Trioe command: "));
  Serial.println(command);
}

void applyTrioeJoystick(int x, int y) {
  String command = "";
  int absX = abs(x);
  int absY = abs(y);

  if (absX < TRIOE_JOYSTICK_DEADZONE && absY < TRIOE_JOYSTICK_DEADZONE) {
    command = "stop";
  } else if (absX > absY) {
    command = x > 0 ? "right" : "left";
  } else {
    command = y > 0 ? "forward" : "backward";
  }

  if (command == lastTrioeJoystickCommand) return;
  lastTrioeJoystickCommand = command;

  recordInput();

  if (command == "stop") {
    if (isContinuousMovementCommand(currentCommand)) {
      if (ENABLE_TRIOE_DEBUG_PRINTS) {
        Serial.print(F("Trioe joystick center stopping movement: "));
        Serial.println(currentCommand);
      }
      currentCommand = "";
      currentCommandUpdateToken = "";
    } else if (ENABLE_TRIOE_DEBUG_PRINTS && currentCommand.length() > 0) {
      Serial.print(F("Trioe joystick center ignored while command pending: "));
      Serial.println(currentCommand);
    }
  } else {
    currentCommand = command;
    currentCommandUpdateToken = "";
    exitIdle();
  }

  Serial.print(F("Trioe joystick: "));
  Serial.print(x);
  Serial.print(F(", "));
  Serial.print(y);
  Serial.print(F(" -> "));
  Serial.println(command);
}

String normalizeTrioeCommand(String value) {
  value.trim();
  value.toLowerCase();
  value.replace("_", "");
  value.replace("-", "");
  value.replace(" ", "");

  if (value == "walk" || value == "go" || value == "up") return "forward";
  if (value == "down" || value == "reverse") return "backward";
  if (value == "turnleft") return "left";
  if (value == "turnright") return "right";
  if (value == "pushups") return "pushup";
  if (value == "stop" || value == "center" || value == "idle") return "stop";
  return value;
}

bool isSesameCommand(const String& command) {
  return command == "forward" ||
         command == "backward" ||
         command == "left" ||
         command == "right" ||
         command == "stop" ||
         command == "rest" ||
         command == "stand" ||
         command == "wave" ||
         command == "dance" ||
         command == "swim" ||
         command == "point" ||
         command == "pushup" ||
         command == "bow" ||
         command == "cute" ||
         command == "freaky" ||
         command == "worm" ||
         command == "shake" ||
         command == "shrug" ||
         command == "dead" ||
         command == "crab";
}

bool isContinuousMovementCommand(const String& command) {
  return command == "forward" ||
         command == "backward" ||
         command == "left" ||
         command == "right";
}

bool shouldPollTrioeDuringDelays() {
  if (executingCommand.length() > 0) {
    return isContinuousMovementCommand(executingCommand);
  }
  return currentCommand.length() == 0 || isContinuousMovementCommand(currentCommand);
}

void recordInput() {
  lastInputTime = millis();
  if (!firstInputReceived) {
    firstInputReceived = true;
    showingWifiInfo = false;
  }
}

void serviceLocalRemote() {
  if (!localRemoteEnabled) return;

  server.handleClient();
  dnsServer.processNextRequest();
}

void maintainNetworkConnection() {
  if (!ENABLE_NETWORK_MODE || String(NETWORK_SSID).length() == 0) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!networkConnected) {
      networkConnected = true;
      networkIP = WiFi.localIP();
      Serial.print(F("Network reconnected! IP: "));
      Serial.println(networkIP);

      if (ENABLE_TRIOE_HUB_CONTROL && DISABLE_HOTSPOT_WHEN_TRIOE_CONNECTED) {
        localRemoteEnabled = false;
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        wifiInfoText = "Network: " + String(NETWORK_SSID) + " (" + networkIP.toString() + ")  |  Trioe Hub control active  |  ";
        Serial.println(F("Trioe Hub connected mode: hotspot, captive portal, and local remote disabled."));
      }
    }
    return;
  }

  if (networkConnected) {
    networkConnected = false;
    Serial.println(F("Network lost. Trioe polling paused until reconnect."));
  }

  unsigned long now = millis();
  if (now - lastNetworkRetryMs < 5000) return;
  lastNetworkRetryMs = now;

  Serial.print(F("Retrying network: "));
  Serial.println(NETWORK_SSID);
  WiFi.mode(localRemoteEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.setHostname(deviceHostname.c_str());
  WiFi.begin(NETWORK_SSID, NETWORK_PASS);
}

void updateWifiInfoScroll() {
  if (!displayReady) return;
  // Don't show WiFi info if first input has been received
  if (firstInputReceived) {
    if (showingWifiInfo) {
      showingWifiInfo = false;
      // Restore the current face
      if (currentFaceFrames != nullptr && currentFaceFrameCount > 0) {
        updateFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
      }
    }
    return;
  }
  
  unsigned long now = millis();
  
  // Check if 30 seconds have passed without input
  if (!showingWifiInfo && (now - lastInputTime >= 30000)) {
    showingWifiInfo = true;
    wifiScrollPos = 0;
    lastWifiScrollMs = now;
  }
  
  if (!showingWifiInfo) return;
  
  // Update scroll every 150ms
  if (now - lastWifiScrollMs >= 150) {
    lastWifiScrollMs = now;
    
    // Clear and redraw with current face in background
    display.clearDisplay();
    
    // Draw the face bitmap in the background
    if (currentFaceFrames != nullptr && currentFaceFrameCount > 0) {
      drawFaceBitmap(currentFaceFrames[currentFaceFrameIndex]);
    }
    
    // Draw black bar for text background on top row
    display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1327_BLACK);
    
    // Draw scrolling text
    display.setTextSize(1);
    display.setTextColor(SSD1327_WHITE);
    display.setTextWrap(false);
    display.setCursor(-wifiScrollPos, 1);
    display.print(wifiInfoText);
    display.setTextWrap(true);
    
    display.display();
    
    // Advance scroll position
    wifiScrollPos += 2;
    if (wifiScrollPos >= (int)(wifiInfoText.length() * 6)) {
      wifiScrollPos = 0;
    }
  }
}
