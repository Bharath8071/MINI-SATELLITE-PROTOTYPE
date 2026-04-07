# 🛰️ SAT — Environmental Monitoring Node with GPS & GSM Geolocation

![Platform](https://img.shields.io/badge/Platform-ESP32-blue)
![Language](https://img.shields.io/badge/Language-Arduino%20C%2B%2B-orange)
![Cloud](https://img.shields.io/badge/Cloud-ThingSpeak-green)
![License](https://img.shields.io/badge/license-All%20Rights%20Reserved-red)
![Status](https://img.shields.io/badge/status-under--development-orange)

A real-time IoT environmental monitoring system built on the **ESP32** microcontroller. It collects multi-sensor environmental data, uploads it to **ThingSpeak** over WiFi, and buffers data on an **SD card** during offline periods. The **SIM868** module serves dual roles — GSM data communication and built-in GPS for location tracking. Location is resolved through a **3-tier fallback system**: onboard GPS first or GSM cell tower triangulation via BeaconDB API second as a final fallback.

Ideal for CanSat, weather balloon payloads, remote weather stations, or any edge IoT deployment.

---

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Wiring Diagram](#-wiring-diagram)
- [Sensors Overview](#-sensors-overview)
- [Software Dependencies](#-software-dependencies)
- [Configuration](#-configuration)
- [How It Works](#-how-it-works)
- [Data Pipeline](#-data-pipeline)
- [ThingSpeak Channel Fields](#-thingspeak-channel-fields)
- [Offline Data Buffering](#-offline-data-buffering)
- [Location & Geolocation System](#-location--geolocation-system)
- [Getting Started](#-getting-started)
- [Project Advantages](#-project-advantages)
- [Folder Structure](#-folder-structure)
- [License](#-license)

---

## ✨ Features

- ⏰ **NTP time synchronization** — Accurate IST timestamps synced to SIM868
- 🌡️ **Triple temperature sensing** — LM35 (analog), DHT22, and BMP280
- 💧 **Humidity monitoring** — DHT22 sensor
- 📈 **Atmospheric pressure & altitude** — BMP280 via I2C
- 📶 **Dual-role SIM868** — Handles both GSM data communication and onboard GPS location
- 📡 **3-tier geolocation** — SIM868 built-in GPS → GSM cell tower triangulation (BeaconDB)
- ☁️ **Cloud IoT upload** — Real-time data streaming to ThingSpeak (7 fields)
- 💾 **Offline SD card buffering** — No data loss during WiFi outages
- 🔁 **Auto-recovery** — Buffered SD data re-uploaded automatically when WiFi reconnects

---

## 🔧 Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 Dev Board | Main microcontroller |
| LM35 | Analog temperature sensor |
| DHT22 | Digital temperature & humidity sensor |
| BMP280 | I2C pressure & altitude sensor |
| SIM868 Module | GSM communication + built-in GPS for location tracking |
| MicroSD Card Module | SPI-based SD card for offline buffering |
| MicroSD Card | Any FAT32 formatted card |
| Jumper Wires | Standard dupont wires |
| Power Supply | 3.3V / 5V depending on module |

---

## 🔌 Wiring Diagram

| Component | ESP32 Pin |
|-----------|-----------|
| LM35 (OUT) | GPIO 12 (ADC) |
| DHT22 (DATA) | GPIO 13 |
| BMP280 (SDA) | GPIO 21 (I2C SDA) |
| BMP280 (SCL) | GPIO 22 (I2C SCL) |
| SIM868 (TX → ESP RX) | GPIO 16 |
| SIM868 (RX → ESP TX) | GPIO 17 |
| SIM868 (PWR SELECT) | GPIO 4 |
| SD Card (CS) | GPIO 5 |
| SD Card (MOSI) | GPIO 23 |
| SD Card (MISO) | GPIO 19 |
| SD Card (SCK) | GPIO 18 |

<!-- ⚠️ BMP280 I2C address is set to `0x76`. Change in code if yours is `0x77`. -->

---

## 📡 Sensors Overview

### 1. LM35 — Analog Temperature Sensor
- Connected to GPIO 12 via ADC
- Voltage output calibrated using `esp_adc_cal` library for accuracy
- Output: Temperature in **°F**

### 2. DHT22 — Temperature & Humidity Sensor
- Connected to GPIO 13
- Output: Temperature in **°C** and relative **Humidity in %**

### 3. BMP280 — Barometric Pressure & Altitude Sensor
- Connected via I2C at address `0x76`
- Output: **Atmospheric Pressure (Pa)** and estimated **Altitude (m)**

### 4. SIM868 — Multi-Role GSM + GPS Module
The SIM868 serves multiple functions in this project:

**GSM Communication**
- Communicates via UART2 (RX=16, TX=17) using AT commands
- Handles clock synchronization via `AT+CCLK`
- Scans nearby cell towers using `AT+CENG?` for geolocation fallback

**Built-in GPS (Primary Location Source)**
- The SIM868 has an integrated GPS receiver
- Used as the first and preferred source for latitude and longitude

**GSM Cell Tower Triangulation (Secondary Fallback)**
- If GPS fails or has no fix, cell tower data (MCC, MNC, LAC, Cell ID) is sent to the **BeaconDB API**
- Returns an estimated lat/lng based on tower positions

---

## 📦 Software Dependencies

Install these libraries via Arduino Library Manager or PlatformIO:

```
WiFi.h           (built-in ESP32)
HTTPClient.h     (built-in ESP32)
HardwareSerial.h (built-in ESP32)
Wire.h           (built-in ESP32)
SPI.h            (built-in ESP32)
SD.h             (built-in ESP32)
esp_adc_cal.h    (built-in ESP32)
NTPClient        by Fabrice Weinberg
ThingSpeak       by MathWorks
DHT sensor lib   by Adafruit
Adafruit_BMP280  by Adafruit
Adafruit_Sensor  by Adafruit
ArduinoJson      by Benoit Blanchon
```

---

## ⚙️ Configuration

Open `SAT_wificode_FINAL.ino` and update the following:

```cpp
// WiFi Credentials
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ThingSpeak Channel
unsigned long myChannelNO = "YOUR_CHANNEL_NO";
const char* myWriteAPIKey     = "YOUR_WRITE_API_KEY";
```

---

## 🔄 How It Works

### Startup Sequence (`setup()`)
1. Serial and UART2 (SIM868) initialized at 9600 baud
2. WiFi connection established
3. ThingSpeak client started
4. BMP280 sensor initialized over I2C
5. SD card initialized on SPI
6. DHT22 started
7. SIM868 module powered on and verified with `AT` command
8. NTP time fetched and converted to IST (UTC+5:30)
9. SIM868 clock synced using `AT+CCLK`
10. Initial GSM cell tower scan and BeaconDB geolocation

### Main Loop (`loop()`)
- Every **~11 seconds**, all sensor readings are collected
- If **WiFi is connected**: data is sent directly to ThingSpeak
- If **WiFi is disconnected**: data is buffered as JSON to SD card
- Every **10 sensor cycles** (~110 seconds), GSM geolocation is refreshed

---

## 🗺️ Data Pipeline

```
┌─────────────────────────────────────────┐
│              SENSOR READING             │
│  LM35 → °F   DHT22 → °C + %RH          │
│  BMP280 → Pa + Altitude                 │
└────────────────────┬────────────────────┘
                     │
          ┌──────────▼──────────┐
          │   WiFi Connected?   │
          └──────┬──────────────┘
         YES     │          NO
          │      │           │
          ▼      │           ▼
   Flush SD      │    Buffer to SD Card
   (if any)      │    (sat_data.json)
          │      │
          ▼      │
   Send to ThingSpeak
   (7 fields incl. lat/lng)
                     │
          ┌──────────▼──────────┐
          │  Every 10 Cycles    │
          │  GSM Tower Scan     │
          │  → BeaconDB API     │
          │  → lat / lng update │
          └─────────────────────┘
```

---

## 📊 ThingSpeak Channel Fields

| Field | Data |
|-------|------|
| Field 1 | LM35 Temperature (°F) |
| Field 2 | DHT22 Humidity (%) |
| Field 3 | DHT22 Temperature (°C) |
| Field 4 | BMP280 Pressure (Pa) |
| Field 5 | BMP280 Altitude (m) |
| Field 6 | Latitude (GPS / GSM / WiFi) |
| Field 7 | Longitude (GPS / GSM / WiFi) |

---

## 💾 Offline Data Buffering

When WiFi is unavailable, readings are stored in `/sat_data.json` on the SD card in the following format:

```json
[
  {
    "timestamp": "AT+CCLK response string",
    "LM35_temp": 87.3,
    "DHT_Humidity": 65.4,
    "DHT_Temp": 29.1,
    "BMP_Value": 101325.0,
    "AUX_Value": 12.5
  }
]
```

On WiFi reconnection, all buffered entries are uploaded to ThingSpeak in sequence, and the file is deleted after successful upload.

---

## 📍 Location & Geolocation System

The SIM868 module provides a **3-tier location fallback chain** to ensure coordinates are always available:

```
┌─────────────────────────────────────┐
│   TIER 1: SIM868 Built-in GPS       │
│   Primary — most accurate           │
│   Uses onboard GPS receiver         │
└──────────────┬──────────────────────┘
               │ GPS fix failed?
               ▼
┌─────────────────────────────────────┐
│   TIER 2: GSM Cell Tower (BeaconDB) │
│   Secondary — works indoors         │
│   AT+CENG? → MCC, MNC, LAC, CID    │
│   POST to beacondb.net → lat/lng    │
└──────────────┬──────────────────────┘
               │ GSM triangulation failed?
               ▼
┌─────────────────────────────────────┐
│   TIER 3: WiFi-Based Geolocation    │
│   Final fallback                    │
│   Uses surrounding WiFi networks    │
└─────────────────────────────────────┘
```

### GSM Tower Data Fields (used in Tier 2)
When querying `AT+CENG?`, the SIM868 returns:

- **MCC** — Mobile Country Code
- **MNC** — Mobile Network Code
- **LAC** — Location Area Code (hex → decimal)
- **Cell ID** — Unique cell tower identifier (hex → decimal)

This is formatted as JSON and sent to the [BeaconDB API](https://beacondb.net/):

```
POST https://api.beacondb.net/v1/geolocate
```

The API returns an estimated `lat` and `lng`, which are stored globally and included in the next ThingSpeak upload cycle.

---

## 🚀 Getting Started

1. **Clone this repository**
   ```bash
   git clone https://github.com/YOUR_USERNAME/SAT-WiFi-Monitor.git
   cd SAT-WiFi-Monitor
   ```

2. **Open in Arduino IDE** or PlatformIO

3. **Install all libraries** listed in [Software Dependencies](#-software-dependencies)

4. **Update credentials** in the configuration section of the `.ino` file

5. **Select board**: `ESP32 Dev Module`

6. **Set baud rate**: `9600`

7. **Upload** to your ESP32

8. **Open Serial Monitor** at 9600 baud to see live logs

---

## ✅ Project Advantages

| Advantage | Description |
|-----------|-------------|
| **Offline Resilience** | SD card buffering ensures zero data loss during WiFi outages |
| **3-Tier Location** | GPS first, GSM tower fallback, then WiFi — location always available |
| **Redundant Temperature** | LM35 + DHT22 cross-validates readings for higher accuracy |
| **Cloud-Ready** | Streams live data to ThingSpeak for dashboards and analytics |
| **Time-Accurate Logs** | NTP-synced IST timestamps on every stored record |
| **Self-Contained** | Runs standalone on ESP32 — no external server required |
| **Auto-Recovery** | WiFi reconnection triggers automatic SD data re-upload |

---

## 📁 Folder Structure

```
SAT-WiFi-Monitor/
│
├── SAT_wificode_FINAL.ino   # Main Arduino firmware
└── README.md                # Project documentation
```

---

## 📄 License

This project is provided for educational and demonstration purposes only.  
Unauthorized use, copying, or modification is strictly prohibited.

## 🚧 Work in Progress

This project is currently under active development and is not yet complete.  
Features, architecture, and implementation may change over time.

> 🛠️ Built with ESP32, Arduino, and a passion for embedded IoT systems.
