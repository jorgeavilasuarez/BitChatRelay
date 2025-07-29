# ESP32 BLE to ESP-NOW Mesh Gateway

This project transforms an ESP32 into a powerful and efficient gateway, bridging communication between **Bluetooth Low Energy (BLE)** devices and an **ESP-NOW mesh network**.

It allows a smartphone or computer to communicate with a network of ESP32/ESP8266 devices, and vice-versa, using BLE as the user-facing interface and ESP-NOW for the device-to-device backbone.

## ✨ Features

-   **Bi-directional Communication:** Forwards messages from BLE to the mesh and from the mesh back to BLE clients.
-   **High-Performance BLE:** Uses the **NimBLE-Arduino** library for a memory-efficient and fast BLE stack.
-   **Robust Mesh Network:** Integrates the `zh_network` library for a simple and effective mesh based on the connectionless ESP-NOW protocol.
-   **Long-Range Ready:** The firmware enables the ESP32's Long-Range (LR) protocol for ESP-NOW communication.
-   **Configurable:** Easily enable or disable features like serial logging through macros in the source code.

---

## ⚙️ How It Works

The gateway operates as a simple and transparent data bridge.

#### Data Flow: BLE to Mesh
A client (like a smartphone app) writes data to the gateway's BLE characteristic. The gateway then broadcasts this data to all nodes in the ESP-NOW mesh.

```
BLE Client 📲 ---[Write]--> ESP32 Gateway 📡 ---[zh_network_send]--> ESP-NOW Mesh Nodes 📶
```

#### Data Flow: Mesh to BLE
A node in the ESP-NOW mesh sends data to the gateway. The gateway receives this data and sends a BLE notification with the data payload to all connected and subscribed BLE clients.

```
ESP-NOW Mesh Node 📶 ---[onRecv]--> ESP32 Gateway 📡 ---[Notify]--> All connected BLE Clients 📲
```

---

## 📋 Requirements

### Hardware
-   An **ESP32** development board (e.g., ESP32-DevKitC, NodeMCU-32S, etc.).

### Software & Dependencies
-   [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/).
-   **ESP32 Board Support Package** for the Arduino IDE.
-   The following Arduino libraries:
    -   [`NimBLE-Arduino`](https://github.com/h2zero/NimBLE-Arduino): Can be installed from the Arduino Library Manager.
    -   `zh_network`: This library must be added to your Arduino `libraries` folder manually.

---

## 🚀 Getting Started

### 1. Installation
1.  Clone this repository to your local machine.
2.  Open the `.ino` file in the Arduino IDE.
3.  Install the required libraries listed above. For `NimBLE-Arduino`, use the Library Manager (`Sketch` > `Include Library` > `Manage Libraries...`).
4.  Place the `zh_network` library folder inside your Arduino libraries directory (e.g., `C:\Users\YourUser\Documents\Arduino\libraries`).

### 2. Configuration
All main configuration options are at the top of the `.ino` file:
-   `DEBUG_SERIAL_ENABLED`: Uncomment this line to enable `Serial.print` logs for debugging.
-   `DEVICE_NAME_BITCHAT`: The device name advertised over BLE.
-   `SERVICE_UUID_BITCHAT` / `CHARACTERISTIC_UUID_BITCHAT`: The UUIDs for the BLE service and characteristic.

### 3. Uploading
1.  In the Arduino IDE, select your ESP32 board from the `Tools` > `Board` menu.
2.  Select the correct COM port from `Tools` > `Port`.
3.  Click the "Upload" button.

### 4. Usage
Once the firmware is running, the ESP32 will begin advertising as a BLE server.
-   Use a BLE scanner app (like **nRF Connect** for Mobile or **LightBlue**) to connect to the device.
-   Once connected, you can **write** data to the specified characteristic to send a message to the ESP-NOW mesh.
-   **Subscribe to notifications** on the same characteristic to receive messages originating from the mesh.

---

##  BLE Service Details

| Item                | UUID                                     | Properties             |
| ------------------- | ---------------------------------------- | ---------------------- |
| **Service** | `F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C`   | -                      |
| **Characteristic** | `A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D`   | `READ`, `WRITE`, `NOTIFY` |

---

## 📄 License

This project is licensed under the MIT License. See the `LICENSE` file for details.

## 🙏 Acknowledgments

-   A big thank you to the creators of the **NimBLE-Arduino** and **zh_network** libraries for their excellent work.