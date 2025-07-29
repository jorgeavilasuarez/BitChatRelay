#include <Arduino.h>
#include <WiFi.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "zh_network.h"
#include <NimBLEDevice.h>

// =================================================================
// =================== CONFIGURATION ===============================
// =================================================================

/**
 * @brief Uncomment this line to enable serial debug messages.
 * If commented out, logs will be disabled and not compiled into the binary.
 */
#define DEBUG_SERIAL_ENABLED

/**
 * @brief Enables or disables the BLE functionality.
 */
#define ENABLED_BLE

// --- Application Constants ---
static const char* DEVICE_NAME_BITCHAT = "BitchatGateway";
static const char* SERVER_NAME_BITCHAT = "NimBLE-Server";
static const NimBLEUUID SERVICE_UUID_BITCHAT("F47B5E2D-4A9E-4C5A-9B3F-8E1D2C3A4B5C");
static const NimBLEUUID CHARACTERISTIC_UUID_BITCHAT("A1B2C3D4-E5F6-4A5B-8C9D-0E1F2A3B4C5D");

// =================================================================
// =================== HELPERS =====================================
// =================================================================

#ifdef DEBUG_SERIAL_ENABLED
#define LOG(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
#define LOG(format, ...)  // Compiles to nothing if debug is disabled
#endif

// Pointers for the BLE server components.
static NimBLEServer* pBleServer = nullptr;
static NimBLEService* pBleService = nullptr;
static NimBLECharacteristic* pBleCharacteristic = nullptr;

// =================================================================
// =================== BLE CALLBACKS ===============================
// =================================================================

/**
 * @class ServerCallbacks
 * @brief Handles connection and disconnection events from BLE clients.
 */
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    LOG("BLE client connected. Active connections: %d\n", pServer->getConnectedCount());
    NimBLEDevice::getAdvertising()->start();
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    LOG("BLE client disconnected. Active connections: %d\n", pServer->getConnectedCount());
    NimBLEDevice::getAdvertising()->start();
  }
} serverCallbacks;

/**
 * @class CharacteristicCallbacks
 * @brief Handles write events on the BLE characteristic.
 */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    LOG("onWrite: Received %d bytes from BLE client.\n", pCharacteristic->getLength());

    if (pCharacteristic->getLength() > 0) {
      const uint8_t* data = pCharacteristic->getValue();
      size_t length = pCharacteristic->getLength();
      // Forward the received data to the mesh network.
      zh_network_send(NULL, data, length);
      pCharacteristic->setValue(data, length);
      auto clientHandles = NimBLEDevice::getServer()->getPeerDevices();
      for (auto handle : clientHandles) {
        if (handle != connInfo.getConnHandle()) {
          pCharacteristic->notify(handle);
        }
      }
    }
  }
} chrCallbacks;

// =================================================================
// =================== EVENT HANDLERS ==============================
// =================================================================

/**
 * @brief Handles events from the zh_network (ESP-NOW Mesh) library.
 */
void onZhNetworkEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_id == ZH_NETWORK_ON_RECV_EVENT) {
    auto* zhRecvData = (zh_network_event_on_recv_t*)event_data;

    LOG("ZHNetwork Rx: %d bytes from MAC " MACSTR "\n", zhRecvData->data_len, MAC2STR(zhRecvData->mac_addr));

#ifdef ENABLED_BLE
    // Update the characteristic's value and notify BLE clients.
    if (pBleCharacteristic != nullptr) {
      pBleCharacteristic->setValue(zhRecvData->data, zhRecvData->data_len);
      pBleCharacteristic->notify();
    }
#endif

    // Important: Free the memory of the received data.
    heap_caps_free(zhRecvData->data);
  }
}

// =================================================================
// =================== INITIALIZATION ==============================
// =================================================================

/**
 * @brief Initializes the zh_network (ESP-NOW) mesh.
 */
void initZhNetworkMesh() {
  LOG("Initializing ZHNetwork (ESP-NOW Mesh)...\n");
  // Silence low-level logs for a cleaner output.
  esp_log_level_set("zh_vector", ESP_LOG_NONE);
  esp_log_level_set("zh_network", ESP_LOG_NONE);
  esp_log_level_set("wifi", ESP_LOG_WARN);  // Only show WiFi warnings and errors

  // Low-level network initialization
  nvs_flash_init();
  esp_netif_init();
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // máxima potencia de transmisión
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));  // Enable Long Range
  ESP_ERROR_CHECK(esp_wifi_start());

  // Mesh network initialization.
  zh_network_init_config_t network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
  zh_network_init(&network_init_config);

  // Register the event handler for the network.
  ESP_ERROR_CHECK(esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &onZhNetworkEvent, NULL, NULL));
}

/**
 * @brief Initializes the BLE (Bluetooth Low Energy) server.
 */
void initBLE() {
  LOG("Initializing BLE server...\n");

  // Initialize BLE device
  NimBLEDevice::init(DEVICE_NAME_BITCHAT);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // Maximum BLE transmit power

  // Create the BLE server
  pBleServer = NimBLEDevice::createServer();
  if (!pBleServer) {
    LOG("Error: Could not create BLE server.\n");
    return;
  }
  pBleServer->setCallbacks(&serverCallbacks);

  // Create the BLE service
  pBleService = pBleServer->createService(SERVICE_UUID_BITCHAT);
  if (!pBleService) {
    LOG("Error: Could not create BLE service.\n");
    return;
  }

  // Create the BLE characteristic
  pBleCharacteristic = pBleService->createCharacteristic(
    CHARACTERISTIC_UUID_BITCHAT,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  if (!pBleCharacteristic) {
    LOG("Error: Could not create BLE characteristic.\n");
    return;
  }
  pBleCharacteristic->setCallbacks(&chrCallbacks);

  // Start the service and advertising
  pBleService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID_BITCHAT);
  pAdvertising->enableScanResponse(true);
  pAdvertising->start();

  LOG("BLE server '%s' is advertising and waiting for connections.\n", DEVICE_NAME_BITCHAT);
}

// =================================================================
// =================== MAIN PROGRAM ================================
// =================================================================

void setup() {
#ifdef DEBUG_SERIAL_ENABLED
  Serial.begin(115200);
  while (!Serial)
    ;  // Wait for the serial port to be ready
#endif

  LOG("\n--- Starting Bitchat Repeater: BLE <-> ZHNetwork ---\n");

#ifdef ENABLED_BLE
  initBLE();
#endif
  initZhNetworkMesh();

  LOG("--- Hybrid Bitchat Repeater Ready ---\n");
}

void loop() {
   // Nothing here – all handled by events
}