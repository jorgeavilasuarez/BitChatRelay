#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "zh_network.h"

// =================================================================
// =================== CONFIGURATION ===============================
// =================================================================

/**
 * @brief Uncomment this line to enable serial debug messages.
 * If commented out, all serial logs will be disabled and removed from the compiled binary.
 */
#define DEBUG_SERIAL_ENABLED

/**
 * @brief Set to true to initialize and run the BLE functionality.
 */
#define ENABLED_BLE

// BitChat official UUIDs
const char *SERVICE_UUID = "F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C";
const char *CHARACTERISTIC_UUID = "A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D";
const char *BLE_DEVICE_NAME = "BitRelay";

// =================================================================
// =================== HELPERS =====================================
// =================================================================

// Helper macro for logging. It will compile to nothing if DEBUG_SERIAL_ENABLED is not defined.
#ifdef DEBUG_SERIAL_ENABLED
#define LOG(message) Serial.println(message)
#else
#define LOG(message)
#endif

// =================================================================
// =================== GLOBAL VARIABLES ============================
// =================================================================

// Static pointer to the BLE Characteristic.
// It's used by the network event handler to send notifications.
static BLECharacteristic *pCharacteristic = nullptr;

// =================================================================
// =================== BLE CALLBACKS ===============================
// =================================================================

/**
 * @class ServerCallbacks
 * @brief Handles connection and disconnection events from BLE clients.
 */
class ServerCallbacks : public BLEServerCallbacks {
  /**
   * @brief Called when a BLE client connects.
   * @param pServer A pointer to the BLE server instance.
   */
  void onConnect(BLEServer *pServer) override {
    LOG("📲 BLE client connected");
  }

  /**
   * @brief Called when a BLE client disconnects.
   * Restarts advertising to allow new connections.
   * @param pServer A pointer to the BLE server instance.
   */
  void onDisconnect(BLEServer *pServer) override {
    LOG("📴 BLE client disconnected. Restarting advertising...");
    pServer->startAdvertising();
  }
};

/**
 * @class CharacteristicCallbacks
 * @brief Handles write events on the BLE characteristic.
 */
class CharacteristicCallbacks : public BLECharacteristicCallbacks {
  /**
   * @brief Called when a BLE client writes data to the characteristic.
   * The received data is then broadcasted over the ESP-NOW network.
   * @param pChar A pointer to the characteristic that was written to.
   */
  void onWrite(BLECharacteristic *pChar) override {
    LOG("✉️ Message received via BLE. Broadcasting to ESP-NOW...");
    zh_network_send(NULL, pChar->getData(), pChar->getLength());
  }
};

// =================================================================
// =================== INITIALIZATION ==============================
// =================================================================

/**
 * @brief Initializes the BLE Server, Service, and Characteristic.
 * Sets up callbacks and starts advertising.
 */
void init_BLE() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  LOG("🟢 BLE ready. Waiting for messages from BitChat.");
}

/**
 * @brief Initializes the zh_network (ESP-NOW mesh) configuration.
 * This includes Wi-Fi setup in STA mode with LR protocol and registering the event handler.
 */
void init_zh_network() {
  // Silence logs from the underlying libraries for a cleaner output
  esp_log_level_set("zh_vector", ESP_LOG_NONE);
  esp_log_level_set("zh_network", ESP_LOG_NONE);

  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_init_config);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
  esp_wifi_start();

  zh_network_init_config_t network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
  zh_network_init(&network_init_config);

  // Register the handler for network events
  esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler, NULL, NULL);
}

// =================================================================
// =================== EVENT HANDLERS ==============================
// =================================================================

/**
 * @brief Handles events from the zh_network library.
 * @param arg User arguments (not used).
 * @param event_base The base of the event.
 * @param event_id The ID of the event.
 * @param event_data The data associated with the event.
 */
void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  switch (event_id) {
    case ZH_NETWORK_ON_RECV_EVENT:
      {
        LOG("✉️ Message received from ESP-NOW. Notifying BLE clients...");
        zh_network_event_on_recv_t *recv_data = (zh_network_event_on_recv_t *)event_data;

#ifdef ENABLED_BLE
        if (pCharacteristic != nullptr) {
          pCharacteristic->setValue(recv_data->data, recv_data->data_len);
          pCharacteristic->notify();
        }
#endif

        heap_caps_free(recv_data->data);  // Free the received data buffer
        break;
      }
    case ZH_NETWORK_ON_SEND_EVENT:
      {
#ifdef DEBUG_SERIAL_ENABLED
        zh_network_event_on_send_t *send_data = (zh_network_event_on_send_t *)event_data;
        char mac_str[18];
        sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(send_data->mac_addr));

        if (send_data->status == ZH_NETWORK_SEND_SUCCESS) {
          Serial.printf("📤 Message to MAC %s sent successfully.\n", mac_str);
        } else {
          Serial.printf("❌ Message to MAC %s failed to send.\n", mac_str);
        }
#endif
        break;
      }
    default:
      break;
  }
}

// =================================================================
// =================== MAIN PROGRAM ================================
// =================================================================

/**
 * @brief Standard Arduino setup function. Runs once on boot.
 */
void setup() {
#ifdef DEBUG_SERIAL_ENABLED
  Serial.begin(115200);
  delay(1000);  // Wait for serial to be ready
#endif

  init_zh_network();

#ifdef ENABLED_BLE
  init_BLE();
#endif
}

/**
 * @brief Standard Arduino loop function.
 * Kept empty as this application is event-driven.
 */
void loop() {
  // Nothing to do here, all work is done in callbacks and event handlers.
}