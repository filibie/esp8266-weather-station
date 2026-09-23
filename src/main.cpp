#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "secrets.h"

const unsigned long REFRESH_INTERVAL      = 60000; // 60 seconds
const unsigned long RENDER_INTERVAL       = 100;   // 100ms UI update
const unsigned long SENSOR_READ_INTERVAL  = 60000;

const int BUTTON_PIN    = D3;     // GPIO0
const int POT_PIN       = A0;     // Analog Input
const int ONE_WIRE_BUS  = D4;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int currentScreen       = 0;
const int TOTAL_SCREENS = 2; // Screen 0: Weather, Screen 1: BTC

// Button Debounce
unsigned long lastDebounceTime      = 0;
const unsigned long DEBOUNCE_DELAY  = 50;

int lastButtonState   = HIGH;
int buttonState       = HIGH;

unsigned long lastApiTime     = 0;
unsigned long lastRenderTime  = 0;
unsigned long lastSensorTime  = 0;

// --- State Variables ---
String temperatureDisplay = "Fetching...";
String weatherDisplay = "Fetching...";

float currentIndoorTemp = -127.0;
float currentOutdoorTemp = -999.0;
float currentDeltaT = 0.0;
float currentWind = 0.0;

// Explicit hardware I2C pins for NodeMCU: SDA = D2 (GPIO4), SCL = D1 (GPIO5)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);

WiFiClient client;
HTTPClient http;

WiFiClientSecure sslClient;

void updateDeltaT() {
  if (currentIndoorTemp != -127.0 && currentOutdoorTemp != -999.0) {
    currentDeltaT = abs(currentIndoorTemp - currentOutdoorTemp);
  }
}

void showStatus(const char* line1, const char* line2 = "") {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(5, 20, line1);
  u8g2.drawStr(5, 40, line2);
  u8g2.sendBuffer();
}

void fetchWeather() {
  digitalWrite(LED_BUILTIN, LOW);
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  } 
  HTTPClient http;

  Serial.println("Fetching weather data...");

  if (http.begin(sslClient, apiURL)) {
    http.setTimeout(5000); // 5 sec timeout to avoid blocking execution

    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        float temperature = doc["current"]["temperature_2m"] | currentOutdoorTemp;
        float wind = doc["current"]["wind_speed_10m"] | 0.0f;
        currentWind = wind;

        if (currentOutdoorTemp == -999.0 || abs(temperature - currentOutdoorTemp) < 10) {
          currentOutdoorTemp = temperature;
          updateDeltaT();
        } else {
          temperature = currentOutdoorTemp; // send last reading
        }

        const char* tempUnit = doc["hourly_units"]["temperature_2m"] | "°C";
        const char* windUnit = doc["hourly_units"]["wind_speed_10m"] | "km/h";


        weatherDisplay = String(temperature, 1) + tempUnit + "  " + String(wind, 0) + windUnit;
        Serial.println("Weather data: " + weatherDisplay);
      } else {
        Serial.print("JSON Parse Error: ");
        Serial.println(error.c_str());
        weatherDisplay = "JSON Error";
      }
    } else {
      Serial.printf("HTTP Error: %s\n", http.errorToString(httpCode).c_str());
      weatherDisplay = "HTTP Error";
    }

    http.end();
  } else {
    Serial.println("Unable to connect to HTTPS endpoint");
    weatherDisplay = "Conn Failed";
  }
  client.stop();
  digitalWrite(LED_BUILTIN, HIGH); // LED OFF
}


void sendTemperatureToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (currentIndoorTemp == -127.0 || currentOutdoorTemp == -999.0) {
    Serial.println("Skipping InfluxDB write: Invalid sensor or API reading");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  if (http.begin(client, serverUrl)) {
    http.setTimeout(5000);
    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    http.addHeader("Authorization", influxToken);
    http.addHeader("Connection", "close");

    String payload = "temperature_stats indoor=" + String(currentIndoorTemp, 2) +
                      ",outdoor=" + String(currentOutdoorTemp, 2) +
                      ",wind=" + String(currentWind, 2) +
                      ",delta_t=" + String(currentDeltaT, 2);

    int httpResponseCode = http.POST(payload);
    Serial.printf("Server reponse: %d\n", httpResponseCode);
    if (httpResponseCode == 204) {
      Serial.printf("Logged to InfluxDB: Indoor=%.1f, Outdoor=%.1f, DeltaT=%.1f, Wind=%.1f\n", 
                    currentIndoorTemp, currentOutdoorTemp, currentDeltaT, currentWind);
    } else {
      Serial.printf("Error sending POST: %d\n", httpResponseCode);
    }
    http.end();
  }
}

void readTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C) {
    currentIndoorTemp = tempC;
    temperatureDisplay = String(tempC, 1) + " °C";
    updateDeltaT();
    Serial.printf("DS18B20 Read: %.2f °C\n", tempC);
    // sendTemperatureToServer();
  } else {
    temperatureDisplay = "Disconnected";
    Serial.println("Error: DS18B20 disconnected!");
  }
}

void renderUI() {
  u8g2.clearBuffer();

  if (currentScreen == 0) {    
    // Pogoda
    u8g2.setFont(u8g2_font_ncenR10_tf);
    u8g2.setCursor(5, 20);
    u8g2.print("ZEWNETRZNA");

    u8g2.setCursor(5, 40);
    u8g2.print(weatherDisplay.c_str());

    // 3. Full-Width Progress Bar
    unsigned long elapsed = millis() - lastApiTime;
    if (elapsed > REFRESH_INTERVAL) elapsed = REFRESH_INTERVAL;

    int barWidth  = 118;
    int barHeight = 5;
    int x = 7;
    int y = 53;

    int fillWidth = map(elapsed, 0, REFRESH_INTERVAL, 0, barWidth);

    u8g2.drawFrame(x, y, barWidth, barHeight);
    if (fillWidth > 2) {
      u8g2.drawBox(x + 1, y + 1, fillWidth - 2, barHeight - 2);
    }

  } else if (currentScreen == 1) {
    // Room Temperature
    u8g2.setCursor(5, 20);
    u8g2.print("POKOJOWA");
    u8g2.setCursor(5, 40);
    u8g2.print(temperatureDisplay);
  }

  u8g2.sendBuffer();
}

void handleButton()
{
  int reading = digitalRead(BUTTON_PIN);

  // Debounce logic
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        currentScreen = (currentScreen + 1) % TOTAL_SCREENS;
      }
    }
  }
  lastButtonState = reading;
}

void handleBrightness() {
  static float smoothedPot = 0;
  int rawPot = analogRead(POT_PIN);
  
  // Apply Exponential Moving Average filter (alpha = 0.1 for stability)
  smoothedPot = (smoothedPot * 0.9) + (rawPot * 0.1);
  
  // Define a deadzone threshold at the bottom end
  int finalPot = (int)smoothedPot;
  if (finalPot <= 10) {
    finalPot = 0; // Clamp noise to absolute zero
  }

  // Constrain upper bound and map to OLED contrast
  finalPot = constrain(finalPot, 0, 1023);
  uint8_t contrast = map(finalPot, 0, 1023, 0, 255);

  static uint8_t lastContrast = 255;
  if (contrast != lastContrast) {
    u8g2.setContrast(contrast);
    lastContrast = contrast;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP8266 Booting ---");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Off
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  sensors.begin();

  sslClient.setInsecure();
  sslClient.setBufferSizes(2048, 512); 

  // Initialize Wire explicitly with NodeMCU pins
  Wire.begin(D2, D1);
  Wire.setClock(400000); // Set 400kHz Fast I2C speed

  u8g2.begin();
  u8g2.enableUTF8Print();
  
  showStatus("Connecting WiFi...", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Prevents Wi-Fi radio from dropping into sleep mode
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  digitalWrite(LED_BUILTIN, LOW); // On while connecting

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_BUILTIN, LOW);  
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH); 
      delay(100);
    }

    showStatus("WiFi Connected!", WiFi.localIP().toString().c_str());
    delay(1000);
    
    lastApiTime = millis();
    readTemperature();
    fetchWeather();
    delay(100);
    sendTemperatureToServer();
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("\nWiFi Failed!");
    showStatus("WiFi Connection", "Failed!");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  handleButton();
  
  if (currentMillis - lastApiTime >= REFRESH_INTERVAL) {
    lastApiTime = currentMillis;
    Serial.println("-------");
    readTemperature();
    fetchWeather();
    sendTemperatureToServer();
  }

  // Task 1: Render Display (Prioritized for UI smoothness)
  if (currentMillis - lastRenderTime >= RENDER_INTERVAL) {
    lastRenderTime = currentMillis;
    renderUI();
    handleBrightness();
  }

  yield();
}
