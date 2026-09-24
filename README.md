# ESP8266 Indoor & Outdoor Climate Monitor

An IoT climate tracking solution using an ESP8266 microcontroller, DS18B20 temperature sensor, SSD1306 OLED display, Open-Meteo weather API integration, and a Flask/Chart.js web dashboard with SQLite storage.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP8266-orange.svg)
![Python](https://img.shields.io/badge/python-3.8%2B-brightgreen.svg)

---

## 🌟 Key Features

* **Indoor Monitoring:** Precision temperature tracking with a DS18B20 digital sensor.
* **Outdoor Weather Data:** Live HTTPS polling from Open-Meteo API for outdoor temperature and wind speed.
* **Calculated Metrics:** Dynamic calculation of $\Delta T$ ($|T_{\text{indoor}} - T_{\text{outdoor}}|$) in real time.
* **OLED User Interface:** SSD1306 display using `u8g2` with screen-switching via push button and progress bar timer.
* **Analog Brightness Control:** Smooth hardware OLED dimming using a potentiometer connected to A0 with Exponential Moving Average (EMA) filtering.
* **Central Backend & Live Dashboard:** Flask API endpoint saving incoming telemetry to SQLite with a Chart.js web dashboard.

---

## 🛠️ Hardware Requirements

* **ESP8266 Board** (e.g., NodeMCU or Wemos D1 Mini)
* **DS18B20 Temperature Sensor** (with $4.7\text{k}\Omega$ pull-up resistor)
* **SSD1306 128x64 OLED Display** (I2C)
* **Push Button** (Screen toggle)
* **Potentiometer** (Display contrast control)
* **Breadboard & Jumper Wires**

---

## 🔌 Circuit & Pinout Mapping

| Component | ESP8266 Pin (NodeMCU) | Function |
| :--- | :--- | :--- |
| **SSD1306 SDA** | `D2` (`GPIO4`) | Hardware I2C Data |
| **SSD1306 SCL** | `D1` (`GPIO5`) | Hardware I2C Clock |
| **DS18B20 Signal** | `D4` (`GPIO2`) | OneWire Data |
| **Push Button** | `D3` (`GPIO0`) | Input (`INPUT_PULLUP`) |
| **Potentiometer Wiper**| `A0` | Analog Input |

---

## 📑 Software Architecture

```
                 +-------------------+
                 | Open-Meteo API    |
                 +---------+---------+
                           |
                     HTTPS | Outdoor Temp/Wind
                           v
+---------------+   +-------------------+   HTTP POST   +-------------------+
| DS18B20 Temp  |-->|  ESP8266 NodeMCU  |-------------->|  Python Flask API |
| Sensor        |   +---------+---------+   (JSON Data) +---------+---------+
+---------------+             |                                   |
                              v                                   v
                      SSD1306 OLED Display                  SQLite3 Database
                                                                  |
                                                                  v
                                                           Chart.js Web UI
```

---

## 🚀 Quick Start Guide

### 1. Flask Backend Setup

1. Clone the repository:
   ```bash
   git clone https://github.com/your-username/esp8266-climate-monitor.git
   cd esp8266-climate-monitor/server
   ```

2. Create a virtual environment and install dependencies:
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   pip install flask
   ```

3. Run the Flask server:
   ```bash
   python app.py
   ```
   The backend will start listening at `http://0.0.0.0:5000`.

---

### 2. Microcontroller Firmware Setup

1. Open the project in **Arduino IDE** or **PlatformIO**.
2. Install required libraries:
   * `U8g2` by Oliver
   * `DallasTemperature` by Miles Burton
   * `OneWire` by Paul Stoffregen
   * `ArduinoJson` by Benoit Blanchon (v6 or v7)
3. Configure Wi-Fi and Server IP parameters in your sketch:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* serverUrl = "http://<YOUR_SERVER_IP>:5000/api/reading";
   ```
4. Flash your ESP8266 board.

---

## 📡 API Reference

### `POST /api/reading`
Endpoint used by ESP8266 to push sensor data.

* **Content-Type:** `application/json`
* **Request Payload:**
  ```json
  {
    "indoor": 24.62,
    "outdoor": 11.80,
    "wind": 7.00,
    "delta_t": 12.82
  }
  ```
* **Response:** `201 Created`

### `GET /api/data?limit=50`
Endpoint consumed by Chart.js to render telemetry graphs.

* **Response Example:**
  ```json
  {
    "timestamps": ["2026-09-24 12:00:00"],
    "indoor": [24.62],
    "outdoor": [11.80],
    "delta_t": [12.82],
    "wind": [7.00]
  }
  ```

---

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.