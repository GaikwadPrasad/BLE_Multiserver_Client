# BLE Projects using ESP-IDF (ESP32)

This repository contains **Bluetooth Low Energy (BLE) implementations using ESP-IDF** on the ESP32 platform.  
It demonstrates BLE **Server**, **Client**, and **Multi-Server Client** architectures commonly used in IoT and embedded systems.

---

## 📘 Projects Included

### 1️⃣ BLE Server (ESP-IDF)
- ESP32 acts as a **BLE GATT Server**
- Advertises BLE services and characteristics
- Allows BLE clients (mobile apps / ESP32) to read and write data

### 2️⃣ BLE Multi-Server & Client
- ESP32 works in **dual role**
  - BLE GATT Server
  - BLE GATT Client
- Hosts multiple BLE services
- Connects to external BLE peripherals

### 3️⃣ One BLE Client Connecting to Multiple BLE Servers
- Single ESP32 BLE Client
- Scans, connects, and communicates with **multiple BLE Servers**
- Ideal for sensor networks and centralized data collection

---

## 🧠 Key Features
- BLE GATT Server implementation
- BLE GATT Client implementation
- One client → multiple servers communication
- Read / Write BLE characteristics
- ESP-IDF based architecture
- Tested using nRF Connect / LightBlue apps

