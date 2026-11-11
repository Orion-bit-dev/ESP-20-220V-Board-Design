// PRJ-26 220V Power Monitor: 2 relays, 2 buttons, sensing of an outside 220 V source,
// OLED, live web page, WiFi saved in flash with a setup mode (hold D32 for 3 s).
// Passwords below are placeholders: choose your own and never share them.
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- WIFI SETUP ---
Preferences prefs;
String savedSsid = "";
String savedPassword = "";
const char* apName   = "ESP32_Setup";
const char* apPass   = "CHANGE_ME_8PLUS";   // setup WiFi password, 8 characters or more
const char* adminPass = "CHANGE_ME";        // needed for the Toggle buttons on the web page

// --- PINS ---
const int relayAPin = 18;
const int relayBPin = 19;
const int buttonAPin = 25;
const int buttonBPin = 26;
const int sensePin   = 27;      // LOW = outside 220 V source (Power 2) is present
const int buttonSetupPin = 32;

Adafruit_SSD1306 display(128, 64, &Wire, -1);
WebServer server(80);

// --- STATE ---
bool relayA_IsOn = false;
bool relayB_IsOn = false;
bool power2_IsOn = false;
bool isAPMode = false;

int lastButtonA = HIGH;
int lastButtonB = HIGH;
int lastButtonSetup = HIGH;

unsigned long lastWifiCheck = 0;
unsigned long buttonSetupHeldTime = 0;

// active low modules: on = drive LOW, off = let go (open drain HIGH)
void setRelayA(bool on) { digitalWrite(relayAPin, on ? LOW : HIGH); relayA_IsOn = on; }
void setRelayB(bool on) { digitalWrite(relayBPin, on ? LOW : HIGH); relayB_IsOn = on; }

bool led1_IsOn() { return relayA_IsOn || power2_IsOn; }
bool led2_IsOn() { return relayB_IsOn; }

void updateOLED()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (isAPMode)
  {
    display.println("WIFI SETUP MODE");
    display.setCursor(0, 16);
    display.println("1. Connect phone to:");
    display.println(apName);
    display.setCursor(0, 36);
    display.println("2. Open browser:");
    display.println("192.168.4.1");
    display.display();
    return;
  }

  display.println("220V POWER MONITOR");

  display.setCursor(0, 16);
  display.print("LED 1: ");
  if (led1_IsOn())
  {
    display.print("ON");
    if (relayA_IsOn && power2_IsOn) display.println(" (A + P2)");
    else if (relayA_IsOn)           display.println(" (Relay A)");
    else                            display.println(" (Power 2)");
  }
  else
  {
    display.println("OFF");
  }

  display.setCursor(0, 32);
  display.print("LED 2: ");
  if (led2_IsOn()) display.println("ON (Relay B)");
  else             display.println("OFF");

  display.setCursor(0, 54);
  display.print("Relay A:"); display.print(relayA_IsOn ? "ON " : "OFF");
  display.print("  Relay B:"); display.print(relayB_IsOn ? "ON" : "OFF");

  display.display();
}

void handleRoot()
{
  if (isAPMode)
  {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'></head>";
    html += "<body style='font-family:sans-serif;text-align:center;margin-top:40px'>";
    html += "<h2>ESP32 WiFi Setup</h2>";
    html += "<form action='/savewifi' method='POST'>";
    html += "<p>WiFi Name:<br><input type='text' name='ssid' style='font-size:18px;padding:5px'></p>";
    html += "<p>WiFi Password:<br><input type='password' name='pass' style='font-size:18px;padding:5px'></p>";
    html += "<button style='font-size:20px;padding:10px 20px'>Save & Reboot</button>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
    return;
  }

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>220V Monitor</title></head>";
  html += "<body style='font-family:sans-serif;text-align:center;margin-top:40px'>";
  html += "<h1>220V Power Monitor</h1>";

  html += "<h2 id='led1'>LED 1: ?</h2>";
  html += "<h2 id='led2'>LED 2: ?</h2>";

  html += "<p><b>Relay A (LED 1):</b> <span id='rA'>?</span><br>";
  html += "<b>Power 2 Sensor:</b> <span id='p2'>?</span></p>";
  html += "<p><b>Relay B (LED 2):</b> <span id='rB'>?</span></p>";

  html += "<form action='/toggleA' method='POST'><p><input type='password' name='pw' placeholder='Admin password' style='font-size:16px;padding:6px'> ";
  html += "<button style='font-size:18px;padding:8px'>Toggle Relay A</button></p></form>";
  html += "<form action='/toggleB' method='POST'><p><input type='password' name='pw' placeholder='Admin password' style='font-size:16px;padding:6px'> ";
  html += "<button style='font-size:18px;padding:8px'>Toggle Relay B</button></p></form>";

  // XMLHttpRequest: worked reliably on this ESP32 web server
  html += "<script>";
  html += "var xhttp = new XMLHttpRequest();";
  html += "xhttp.onreadystatechange = function() {";
  html += "  if (this.readyState == 4) {";
  html += "    if (this.status == 200) {";
  html += "      var d = JSON.parse(this.responseText);";
  html += "      document.getElementById('led1').textContent = 'LED 1: ' + (d.led1 ? 'ON' : 'OFF');";
  html += "      document.getElementById('led2').textContent = 'LED 2: ' + (d.led2 ? 'ON' : 'OFF');";
  html += "      document.getElementById('rA').textContent = d.relayA ? 'ON' : 'OFF';";
  html += "      document.getElementById('p2').textContent = d.power2 ? 'ON' : 'OFF';";
  html += "      document.getElementById('rB').textContent = d.relayB ? 'ON' : 'OFF';";
  html += "    } else {";
  html += "      document.getElementById('led1').textContent = 'ERROR: OFFLINE';";
  html += "    }";
  html += "  }";
  html += "};";
  html += "function update() {";
  html += "  xhttp.open('GET', '/status', true);";
  html += "  xhttp.send();";
  html += "}";
  html += "update();";
  html += "setInterval(update, 1000);";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

// only works in setup mode, so nobody on the home WiFi can overwrite the WiFi settings
void handleSaveWifi()
{
  if (!isAPMode)
  {
    server.send(403, "text/plain", "Only in setup mode");
    return;
  }
  if (server.hasArg("ssid") && server.hasArg("pass") && server.arg("ssid").length() > 0)
  {
    prefs.begin("wifi", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("pass"));
    prefs.end();

    server.send(200, "text/html", "<h2>Saved! Rebooting now...</h2>");
    delay(2000);
    ESP.restart();
  }
  else
  {
    server.send(400, "text/plain", "Missing data");
  }
}

void handleToggleA()
{
  if (isAPMode || server.arg("pw") != adminPass) { server.send(403, "text/plain", "Not allowed"); return; }
  setRelayA(!relayA_IsOn);
  updateOLED();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleToggleB()
{
  if (isAPMode || server.arg("pw") != adminPass) { server.send(403, "text/plain", "Not allowed"); return; }
  setRelayB(!relayB_IsOn);
  updateOLED();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStatus()
{
  String json = "{";
  json += "\"led1\":";    json += led1_IsOn() ? "true" : "false";
  json += ",\"led2\":";   json += led2_IsOn() ? "true" : "false";
  json += ",\"relayA\":"; json += relayA_IsOn ? "true" : "false";
  json += ",\"relayB\":"; json += relayB_IsOn ? "true" : "false";
  json += ",\"power2\":"; json += power2_IsOn ? "true" : "false";
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void startAPMode()
{
  isAPMode = true;
  setRelayA(false);
  setRelayB(false);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP);          // never WIFI_OFF: it crashes the ESP32 (xQueueSemaphoreTake)
  WiFi.softAP(apName, apPass);

  Serial.println("AP Mode Started. Connect to " + String(apName));
  updateOLED();
}

void setup()
{
  Serial.begin(115200);

  pinMode(relayAPin, OUTPUT_OPEN_DRAIN);
  pinMode(relayBPin, OUTPUT_OPEN_DRAIN);
  setRelayA(false);
  setRelayB(false);

  pinMode(buttonAPin, INPUT_PULLUP);
  pinMode(buttonBPin, INPUT_PULLUP);
  pinMode(sensePin, INPUT_PULLUP);
  pinMode(buttonSetupPin, INPUT_PULLUP);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { while (true); }
  display.setTextColor(SSD1306_WHITE);

  prefs.begin("wifi", true);
  savedSsid = prefs.getString("ssid", "");
  savedPassword = prefs.getString("pass", "");
  prefs.end();

  if (savedSsid == "")
  {
    // nothing saved yet: go straight to setup mode, no WiFi name or password in the code
    startAPMode();
  }
  else
  {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("220V POWER MONITOR");
    display.setCursor(0, 24);
    display.println("Connecting to WiFi:");
    display.println(savedSsid);
    display.display();

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
    {
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    }
    else
    {
      Serial.println("\nWiFi Failed. Running without WiFi.");
    }
  }

  server.on("/", handleRoot);
  server.on("/toggleA", HTTP_POST, handleToggleA);
  server.on("/toggleB", HTTP_POST, handleToggleB);
  server.on("/status", handleStatus);
  server.on("/savewifi", HTTP_POST, handleSaveWifi);
  server.begin();

  if (isAPMode) return;        // setup mode already shows its own screen

  delay(1000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);   // start the screen again after the WiFi power spike
  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("220V POWER MONITOR");
  display.setCursor(0, 24);
  if (WiFi.status() == WL_CONNECTED)
  {
    display.println("WiFi Connected!");
    display.setCursor(0, 40);
    display.print("IP: ");
    display.println(WiFi.localIP());
  }
  else
  {
    display.println("WiFi OFFLINE");
  }
  display.display();

  delay(3000);
  updateOLED();
}

void loop()
{
  server.handleClient();

  // hold the setup button for 3 seconds = setup mode
  int btnSetup = digitalRead(buttonSetupPin);
  if (!isAPMode)
  {
    if (btnSetup == LOW && lastButtonSetup == HIGH)
    {
      buttonSetupHeldTime = millis();
    }
    else if (btnSetup == LOW && lastButtonSetup == LOW && millis() - buttonSetupHeldTime >= 3000)
    {
      startAPMode();
      return;
    }
  }
  lastButtonSetup = btnSetup;

  if (isAPMode) return;

  // reconnect watchdog: check every 15 s, never freeze the buttons
  if (millis() - lastWifiCheck >= 15000)
  {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi dropped! Reconnecting...");
      WiFi.reconnect();
    }
  }

  bool newPower2State = (digitalRead(sensePin) == LOW);
  if (newPower2State != power2_IsOn)
  {
    power2_IsOn = newPower2State;
    Serial.print("Power 2 is now: "); Serial.println(power2_IsOn ? "ON" : "OFF");
    updateOLED();
  }

  int btnA = digitalRead(buttonAPin);
  if (btnA == LOW && lastButtonA == HIGH)
  {
    setRelayA(!relayA_IsOn);
    updateOLED();
    delay(50);
  }
  lastButtonA = btnA;

  int btnB = digitalRead(buttonBPin);
  if (btnB == LOW && lastButtonB == HIGH)
  {
    setRelayB(!relayB_IsOn);
    updateOLED();
    delay(50);
  }
  lastButtonB = btnB;
}