# AS_HomeAutomation — Software Requirements Specification

**Project:** AS_HomeAutomation
**Build artifact:** `AS_HomeAutomation_07_May_2026.elf`
**Target platform:** Espressif ESP32 (dual-core), ESP-IDF 5.4.1
**Configuration version:** `CFG_2026_05_V1`
**Document date:** 21 May 2026
**Status:** Derived from current source baseline (reverse-engineered spec)

---

## 1. Introduction

### 1.1 Purpose
This document specifies the functional and non-functional requirements of the AS_HomeAutomation firmware running on an ESP32-class MCU. It is derived from the current code baseline and captures the system as built, plus placeholders intended for completion.

### 1.2 Scope
The firmware controls home-automation actuators (lights and fans) on an ESP32 device. It accepts control commands from the cloud (AWS IoT — currently simulated) and from a local channel (BLE — placeholder), persists device identity / credentials in NVS, and exposes a modular, event-driven architecture.

### 1.3 Definitions / Acronyms
- **NVS** — Non-Volatile Storage (ESP-IDF flash KV store)
- **OTA** — Over-The-Air firmware update
- **SKU** — Stock Keeping Unit (product variant)
- **PWM** — Pulse-Width Modulation (LEDC peripheral)
- **Logical Control** — Abstract device endpoint (e.g. `LIGHT_1`, `FAN_1`) mapped to a physical driver
- **Driver** — Hardware abstraction operating on a GPIO/LEDC channel

### 1.4 References
- ESP-IDF v5.4.1 documentation
- Source: [main/](../main), [components/](../components), [tools/config_generator/](../tools/config_generator)

---

## 2. System Overview

The firmware is an event-driven FreeRTOS application built from modular components. A central **event bus** decouples subsystems; a **device registry** maps logical control IDs (defined per-SKU in a CSV) to hardware **drivers**; managers handle connectivity, provisioning, cloud, storage, and security concerns.

```
+---------+     +-------------+     +----------------+
|  Cloud  | --> | cloud_client| --> | device_control | --> driver_pool --> gpio_manager --> HW
+---------+     +-------------+     +----------------+
                                          ^
                       event_bus <--------+
                          ^
   provisioning / connectivity / storage / security managers
```

---

## 3. Hardware Requirements

| Item | Requirement |
|------|-------------|
| MCU | ESP32 dual-core, 40-pin GPIO |
| Wireless | Wi-Fi 802.11 b/g/n; Bluetooth/BLE radio present |
| Flash layout | OTA-capable partition table (bootloader, app, partition table, NVS) |
| Default actuators | Light on GPIO 2, Light on GPIO 4 (active-high), Fan on GPIO 5 (PWM, 5 kHz, LEDC ch.0) |
| Console | UART (default ESP-IDF logging UART) |

---

## 4. Functional Requirements

### 4.1 Boot & Initialization (FR-BOOT)
- **FR-BOOT-1** On reset, `app_main` shall initialize NVS Flash, auto-erasing on version/format mismatch.
- **FR-BOOT-2** `app_core_init` shall initialize, in order: GPIO manager, driver pool, device registry (loading generated mappings), NVS identity, feature flags, provisioning manager, connectivity manager, cloud client, device control, storage manager, security manager.
- **FR-BOOT-3** `app_core_start` shall register event handlers and spawn subsystem tasks.
- **FR-BOOT-4** The configured `CFG_2026_05_V1` version string shall be logged at boot.

### 4.2 Event Bus (FR-EVT)
- **FR-EVT-1** The event bus shall provide a single FIFO queue with capacity 10 of `{uint16_t event_id, void* data}`.
- **FR-EVT-2** A dispatcher task at FreeRTOS priority 6, stack 4096 B, shall pop events and invoke the registered handler for that `event_id`.
- **FR-EVT-3** One handler per `event_id` shall be registered (last-wins / single-handler model).
- **FR-EVT-4** Producers shall post events using a non-blocking enqueue API.

### 4.3 Device Registry & Driver Pool (FR-REG)
- **FR-REG-1** The device registry shall translate a logical control ID (e.g. `CONTROL_LIGHT_1`) to a driver instance via the generated mapping table `VENDOR_DEVICE_MAPPINGS[]`.
- **FR-REG-2** The driver pool shall implement at minimum: switch driver (GPIO on/off, active-high) and fan driver (LEDC PWM duty 0–100 %).
- **FR-REG-3** Adding/changing a control mapping shall require only updating [tools/config_generator/vendor_devices.csv](../tools/config_generator/vendor_devices.csv) and rebuilding; no manual edits to generated files.
- **FR-REG-4** `config_generated.{h,c}` shall be regenerated at CMake configure time when the input CSV changes.
- **FR-REG-5** The set of supported logical controls shall include `CONTROL_LIGHT_1..3` and `CONTROL_FAN_1..2`.

### 4.4 GPIO / PWM Abstraction (FR-HAL)
- **FR-HAL-1** `gpio_manager` shall expose `config_output`, `set_level`, `pwm_init`, `pwm_set_duty` and shall be the only module touching ESP-IDF GPIO/LEDC APIs directly.
- **FR-HAL-2** PWM channels shall be initialized at 5 kHz with duty resolution sufficient for 0–100 % control.

### 4.5 Device Control (FR-CTL)
- **FR-CTL-1** `device_control_set_switch(index, on)` shall drive the GPIO of the mapped switch driver (indices 1–3 = lights, 4–5 = fans).
- **FR-CTL-2** `device_control_set_fan_speed(speed)` shall map a 0–100 % value to PWM duty on the fan driver.
- **FR-CTL-3** `device_control_handle_cloud_packet(json_str)` shall accept a JSON command, validate it, look up the control, and execute the driver action.
- **FR-CTL-4** A weak default implementation of `device_control_handle_cloud_packet` shall exist; the strong implementation in [components/device_control/cloud_command.c](../components/device_control/cloud_command.c) shall override it when linked.
- **FR-CTL-5** A device-control task shall run at priority 5, stack 2048 B, and emit periodic readiness logs.

### 4.6 Cloud Command Protocol (FR-PROTO)
The system shall accept commands matching the following JSON envelope:

```json
{
  "protocol_ver": 1,
  "msg_type": "CMD",
  "cmd": "DEVICE_CONTROL",
  "sub_cmd": "SET_SWITCH | SET_FAN_SPEED",
  "msg_id": <uint>,
  "device_id": "DEV_xxx",
  "payload": { "control": "LIGHT_1|...|FAN_1|...", "value": 0|1, "speed": 0-100 },
  "timestamp": <unix_seconds>
}
```

- **FR-PROTO-1** Parsing shall use cJSON.
- **FR-PROTO-2** Commands with unknown `cmd`, `sub_cmd`, or `control` shall be rejected and logged.
- **FR-PROTO-3** `protocol_ver` mismatch shall be rejected.
- **FR-PROTO-4** `SET_SWITCH` shall accept `value ∈ {0,1}`; `SET_FAN_SPEED` shall accept `speed ∈ [0,100]`.

### 4.7 Cloud Client (FR-CLOUD)
- **FR-CLOUD-1** `cloud_client_init` shall start the AWS-simulation task (priority 5, stack 4096 B) which periodically generates `LIGHT_1` ON/OFF/BLINK test packets.
- **FR-CLOUD-2** `cloud_client_publish_device_info(json)` shall publish device telemetry (currently logging-only placeholder).
- **FR-CLOUD-3** `cloud_client_simulate_ble_receive(json)` shall route a BLE-shaped packet through the device-control pipeline.
- **FR-CLOUD-4** Real MQTT-over-TLS to AWS IoT is a planned extension; the build links `mqtt`, `esp-tls`, and `mbedtls` in preparation. *(placeholder)*

### 4.8 Connectivity Manager (FR-NET)
- **FR-NET-1** The connectivity manager shall be responsible for **Wi-Fi STA only**. It shall not contain any BLE / BT code; BLE is owned exclusively by `provisioning_manager` (FR-PROV-3).
- **FR-NET-2** Wi-Fi station support shall be present (`esp_wifi` / lwIP linked).
- **FR-NET-3** On `EVT_WIFI_CONFIGURED`, the connectivity manager shall load credentials from `storage_manager` (namespace `wifi`), call `esp_wifi_init` / `esp_wifi_start` / `esp_wifi_connect`, and register handlers for `WIFI_EVENT_*` and `IP_EVENT_STA_GOT_IP`.
- **FR-NET-4** On `IP_EVENT_STA_GOT_IP`, the manager shall publish `EVT_NET_READY` on the event bus.
- **FR-NET-5** On `WIFI_EVENT_STA_DISCONNECTED`, the manager shall reconnect with exponential backoff (capped, e.g. 1–60 s).
- **FR-NET-6** The manager shall be event-driven (no polling task). The current `vTaskDelay` log loop is a placeholder and shall be removed.

### 4.9 Provisioning Manager (FR-PROV)
- **FR-PROV-1** A provisioning task at priority 5, stack 4096 B, shall handle initial provisioning and factory-reset workflow. *(currently a 5-second log placeholder)*
- **FR-PROV-2** On boot, the provisioning manager shall consult the `provisioned` flag in the `identity` NVS namespace and start the BLE provisioning flow if and only if the device is not yet provisioned.
- **FR-PROV-3** **BLE provisioning transport.** The device shall expose a BLE GATT-based provisioning service using Espressif's **BluFi** protocol, compatible with the *EspBlufi* mobile application (Android/iOS). This requires `CONFIG_BT_ENABLED=y`, `CONFIG_BT_BLUEDROID_ENABLED=y`, and `CONFIG_BT_BLE_BLUFI_ENABLE=y`. *(implemented; see [provisioning_manager.c](../components/provisioning_manager/provisioning_manager.c) + vendored [blufi_init.c](../components/provisioning_manager/blufi_init.c) / [blufi_security.c](../components/provisioning_manager/blufi_security.c))*
- **FR-PROV-4** **Wi-Fi credential reception.** On `WIFI_PROV_CRED_RECV`, the SSID and password shall be persisted via `storage_manager_save_wifi_credentials()` (NVS namespace `wifi`).
- **FR-PROV-5** **AWS configuration reception.** The BluFi *custom-data* frame shall carry a JSON object `{ endpoint, device_cert, private_key, root_ca }`. The fields shall be persisted via `storage_manager_save_aws_config()` (namespace `aws`) and `security_manager_store_cert()` (namespace `certs`).
- **FR-PROV-6** **Provisioning security.** The provisioning channel shall use BluFi's built-in DH key negotiation + AES-CFB128 encryption + CRC16 integrity, so that Wi-Fi credentials and AWS material are not transmitted in clear over the air.
- **FR-PROV-7** **Completion.** On `IP_EVENT_STA_GOT_IP`, the manager shall mark the device provisioned in the `identity` namespace, deinit BluFi (host + controller), release BT controller memory (`esp_bt_mem_release(ESP_BT_MODE_BTDM)`), and publish `EVT_WIFI_CONFIGURED` on the event bus.
- **FR-PROV-8** **Factory reset.** A factory-reset trigger (e.g. long-press GPIO or cloud command) shall erase the `wifi`, `aws`, and `certs` namespaces (the `identity` namespace is write-once and shall be preserved) and reboot the device back into the BLE provisioning flow.
- **FR-PROV-9** **Provisioning timeout.** *(Not enforced by BluFi; deferred. See TBD-2.)*

### 4.10 Storage Manager (FR-STOR)
- **FR-STOR-1** Wi-Fi credentials (SSID, password) shall be stored in NVS namespace `wifi`.
- **FR-STOR-2** AWS configuration (endpoint, cert blob) shall be stored in NVS namespace `aws`.
- **FR-STOR-3** On NVS corruption / no-free-pages, the manager shall erase and reinitialize NVS automatically.

### 4.11 Security Manager (FR-SEC)
- **FR-SEC-1** Device certificate, private key, and root CA shall be stored in NVS namespace `certs`.
- **FR-SEC-2** Identity (vendor_id, device_id, provisioned flag) shall be stored in namespace `identity` using a **write-once** policy — once written, the same fields cannot be overwritten.
- **FR-SEC-3** Secure Boot V1 and Flash Encryption shall be supported by the build configuration (currently not enabled; enabling shall not require source changes outside `sdkconfig`).

### 4.12 Feature Flags (FR-FLAG)
- **FR-FLAG-1** `feature_flags_get()` shall return a struct exposing at minimum: `dimming_enabled`, `fan_speed_control`, `ota_enabled`.
- **FR-FLAG-2** Default values shall be: `dimming_enabled=false`, `fan_speed_control=true`, `ota_enabled=true`.

### 4.13 Logging (FR-LOG)
- **FR-LOG-1** Structured logging shall use the tag set `{CFG, CTRL, DRV, AWS, BLE, OTA, FACT}`.
- **FR-LOG-2** Each module shall log lifecycle events (init, start, command received, command rejected) at appropriate `ESP_LOG*` levels.

---

## 5. Task / Concurrency Requirements

| Task | Module | Priority | Stack | Purpose |
|------|--------|---------:|------:|---------|
| event_dispatch_task | event_bus | 6 | 4096 | Dispatch events |
| prov_task | provisioning_manager | 5 | 4096 | Provisioning / factory reset |
| wifi_task | connectivity_manager | 5 | 4096 | Wi-Fi state |
| mqtt_task | cloud_client | 5 | 4096 | Cloud communication |
| device_task | device_control | 5 | 2048 | Device control |
| aws_sim_task | cloud_client | 5 | 4096 | AWS simulation |

- **FR-RTOS-1** All tasks shall be created with `xTaskCreate` (no fixed core affinity unless explicitly required).
- **FR-RTOS-2** Inter-task communication shall preferentially use the event bus; direct cross-module calls shall be limited to initialization.

---

## 6. Build & Tooling Requirements

- **FR-BLD-1** The project shall build with ESP-IDF 5.4.1 against target `esp32`.
- **FR-BLD-2** Python 3 shall be available at configure time for [tools/config_generator/generate_config.py](../tools/config_generator/generate_config.py).
- **FR-BLD-3** Generated headers/sources shall not be checked in as the source of truth; the CSV is the source of truth.
- **FR-BLD-4** The project name token `AS_HomeAutomation_07_May_2026` is part of the artifact name; a release process shall update this on cadence.

---

## 7. Non-Functional Requirements

### 7.1 Reliability
- **NFR-REL-1** Bootloader watchdog shall be enabled.
- **NFR-REL-2** Region protection shall be enabled.
- **NFR-REL-3** NVS subsystem shall recover automatically from format/version mismatch (auto-erase) and from page exhaustion.

### 7.2 Maintainability / Modularity
- **NFR-MNT-1** Each subsystem shall live in a self-contained ESP-IDF component under [components/](../components) with a public header.
- **NFR-MNT-2** Adding a new logical control shall require only: (a) extending the CSV, (b) adding a driver entry to the driver pool if a new driver type is needed.
- **NFR-MNT-3** No subsystem shall call ESP-IDF GPIO/LEDC APIs directly outside of `gpio_manager`.

### 7.3 Security
- **NFR-SEC-1** Device identity shall not be reprogrammable in the field (write-once enforced in firmware).
- **NFR-SEC-2** TLS material shall reside in NVS, not in firmware images.
- **NFR-SEC-3** Production builds shall be capable of enabling Secure Boot V1 and Flash Encryption via `sdkconfig` only.

### 7.4 Observability
- **NFR-OBS-1** Every command accepted/rejected by `device_control` shall produce a structured log entry tagged `CTRL` including `msg_id` when present.
- **NFR-OBS-2** Cloud simulator output shall be tagged `AWS`.

### 7.5 Performance
- **NFR-PERF-1** Event dispatch latency shall be bounded by the queue depth (10) and dispatcher priority (6).
- **NFR-PERF-2** Switch ON/OFF response time from a received cloud packet to GPIO transition shall be < 50 ms under nominal load. *(target; not yet measured)*

---

## 8. External Interfaces

| Interface | Direction | Status | Notes |
|-----------|-----------|--------|-------|
| Wi-Fi (STA) | bidirectional | linked, stub manager | esp_wifi/lwIP |
| MQTT / AWS IoT | bidirectional | placeholder, simulated | mqtt + esp-tls linked |
| BLE | bidirectional | radio supported, disabled | `simulate_ble_receive` exists |
| GPIO (digital out) | output | implemented | GPIO 2, 4 |
| LEDC (PWM) | output | implemented | GPIO 5, 5 kHz |
| NVS | bidirectional | implemented | namespaces: identity, wifi, aws, certs |
| UART (logging) | output | implemented | default IDF console |

---

## 9. Storage / Partition Requirements
- **FR-PART-1** Partition table shall include bootloader, primary app (OTA-capable), and NVS partitions.
- **FR-PART-2** NVS namespaces in use: `identity`, `wifi`, `aws`, `certs`.

---

## 10. Test Requirements
- **FR-TEST-1** [pytest_smp_examples.py](../pytest_smp_examples.py) currently exercises FreeRTOS SMP example flows on ESP32-C3 / ESP32-S3 and is **not** an application-level test of AS_HomeAutomation. A dedicated test suite shall be added covering: command parsing, registry lookup, driver pool execution, NVS write-once policy, and cloud-simulator round-trip.

---

## 11. Open Items / Placeholders to Complete

| ID | Item | Current state |
|----|------|---------------|
| TBD-1 | Real MQTT-over-TLS to AWS IoT | linked, simulated only |
| TBD-2 | BLE provisioning transport (FR-PROV-3..7) — required for receiving Wi-Fi creds & AWS certs on first boot | **implemented** via BluFi (Bluedroid). Outstanding: provisioning-inactivity timeout (FR-PROV-9), MAC-derived BLE device name |
| TBD-3 | Provisioning workflow / state machine (FR-PROV-1..9) | **phase 1 done**: identity-flag-driven entry, factory-reset path, EVT_WIFI_CONFIGURED publish. Outstanding: connectivity-side handoff |
| TBD-4 | Connectivity state machine | task is a logging stub |
| TBD-5 | OTA update flow | feature flag only |
| TBD-6 | Secure Boot V1 / Flash Encryption | supported, not enabled |
| TBD-7 | Application-level pytest suite | absent (only SMP examples) |
| TBD-8 | Top-level [README.md](../README.md) is the FreeRTOS-SMP example README | replace with project README |

---

## 12. Traceability Summary

| Requirement group | Primary source files |
|-------------------|----------------------|
| Boot / orchestration | [main/app_main.c](../main/app_main.c), [main/app_core.c](../main/app_core.c) |
| Event bus | [main/event_bus.c](../main/event_bus.c), [main/event_bus.h](../main/event_bus.h) |
| Registry / drivers / HAL | [components/common/](../components/common) |
| Generated mappings | [tools/config_generator/](../tools/config_generator), `build/config_generated.{h,c}` |
| Device control | [components/device_control/](../components/device_control) |
| Cloud | [components/cloud_client/](../components/cloud_client) |
| Connectivity | [components/connectivity_manager/](../components/connectivity_manager) |
| Provisioning | [components/provisioning_manager/](../components/provisioning_manager) |
| Storage | [components/storage_manager/](../components/storage_manager) |
| Security | [components/security_manager/](../components/security_manager) |

---

*End of document.*
