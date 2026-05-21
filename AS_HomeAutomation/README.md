# AS_HomeAutomation

ESP32-based home-automation firmware that controls lights and fans, accepts cloud commands (AWS IoT — currently simulated), and exposes a modular, event-driven architecture built on ESP-IDF 5.4.1.

| | |
|---|---|
| **Target** | ESP32 (dual-core) |
| **Framework** | ESP-IDF 5.4.1 |
| **Config version** | `CFG_2026_05_V1` |
| **Build artifact** | `AS_HomeAutomation_07_May_2026.elf` |

A full reverse-engineered requirements spec is available at [docs/Requirements_Specification.md](docs/Requirements_Specification.md).

---

## Features

- **Modular component architecture** — each subsystem is an ESP-IDF component under [components/](components).
- **Event bus** — FreeRTOS queue-based dispatcher decouples subsystems ([main/event_bus.c](main/event_bus.c)).
- **Configuration-driven control mapping** — logical controls (`LIGHT_1`, `FAN_1`, …) are mapped to drivers via a CSV processed at build time by [tools/config_generator/generate_config.py](tools/config_generator/generate_config.py).
- **Driver pool / GPIO HAL** — switch (digital) and fan (LEDC PWM) drivers; all GPIO/LEDC access is funneled through `gpio_manager`.
- **JSON cloud command protocol** — cJSON-based parser in [components/device_control/cloud_command.c](components/device_control/cloud_command.c).
- **AWS IoT simulation** — generates ON/OFF/BLINK test packets to exercise the control pipeline ([components/cloud_client/](components/cloud_client)).
- **Persistent storage** — Wi-Fi credentials, AWS config, X.509 material, and write-once device identity in NVS.
- **Feature flags** — compile-time gating of `dimming_enabled`, `fan_speed_control`, `ota_enabled`.
- **Structured logging** — tagged log streams (`CFG`, `CTRL`, `DRV`, `AWS`, `BLE`, `OTA`, `FACT`).

---

## Architecture

```
+---------+     +--------------+     +----------------+
|  Cloud  | --> | cloud_client | --> | device_control | --> driver_pool --> gpio_manager --> HW
+---------+     +--------------+     +----------------+
                                            ^
                          event_bus <-------+
                              ^
   provisioning_manager / connectivity_manager / storage_manager / security_manager
```

### Components
| Component | Responsibility |
|-----------|----------------|
| [components/common](components/common) | `device_registry`, `driver_pool`, `gpio_manager`, `nvs_identity`, `feature_flags`, `structured_logging` |
| [components/device_control](components/device_control) | Switch / fan-speed control, cloud command parsing |
| [components/cloud_client](components/cloud_client) | Cloud transport + AWS simulation task |
| [components/connectivity_manager](components/connectivity_manager) | **Wi-Fi STA only** — connect / reconnect, publish `EVT_NET_READY` *(stub)* |
| [components/provisioning_manager](components/provisioning_manager) | **BLE end-to-end** — BT controller, NimBLE host, `wifi_prov_mgr`, `aws-config` endpoint, factory reset *(stub)* |
| [components/storage_manager](components/storage_manager) | NVS-backed Wi-Fi / AWS config storage |
| [components/security_manager](components/security_manager) | NVS-backed certificate storage |

### Tasks
| Task | Priority | Stack | Module |
|------|---------:|------:|--------|
| `event_dispatch_task` | 6 | 4096 | event_bus |
| `prov_task` | 5 | 4096 | provisioning_manager |
| `wifi_task` | 5 | 4096 | connectivity_manager |
| `mqtt_task` | 5 | 4096 | cloud_client |
| `device_task` | 5 | 2048 | device_control |
| `aws_sim_task` | 5 | 4096 | cloud_client (simulator) |

---

## Default Hardware Mapping

| Logical control | Driver | Pin / Channel |
|-----------------|--------|---------------|
| `LIGHT_1` | SWITCH1 | GPIO 2 (active-high) |
| `LIGHT_2` | SWITCH2 | GPIO 4 (active-high) |
| `FAN_1`   | FAN1 (PWM) | GPIO 5, LEDC ch.0, 5 kHz |

To add or change mappings, edit [tools/config_generator/vendor_devices.csv](tools/config_generator) and rebuild — `config_generated.{h,c}` are regenerated at CMake configure time.

---

## Cloud Command Protocol

All commands routed through `device_control_handle_cloud_packet()` use this JSON envelope:

```json
{
  "protocol_ver": 1,
  "msg_type": "CMD",
  "cmd": "DEVICE_CONTROL",
  "sub_cmd": "SET_SWITCH",
  "msg_id": 1001,
  "device_id": "DEV_001",
  "payload": { "control": "LIGHT_1", "value": 1 },
  "timestamp": 1715352000
}
```

`sub_cmd` values:
- `SET_SWITCH` — `payload.value ∈ {0, 1}`
- `SET_FAN_SPEED` — `payload.speed ∈ [0, 100]`

---

## NVS Layout

| Namespace | Contents | Notes |
|-----------|----------|-------|
| `identity` | vendor_id, device_id, provisioned flag | **write-once** |
| `wifi` | SSID, password | populated by provisioning |
| `aws` | endpoint, cert blob | populated by provisioning |
| `certs` | device cert, private key, root CA | populated by provisioning |

---

## Provisioning (BluFi over BLE)

On first boot the device has no Wi-Fi credentials and no AWS certificates. The transport for receiving them from a companion mobile app is **BluFi** — Espressif's BLE-based Wi-Fi configuration protocol — compatible with the [EspBlufi](https://github.com/EspressifApp/EspBlufi) mobile app (Android / iOS).

Intended flow:

1. On boot, [provisioning_manager](components/provisioning_manager) reads the `provisioned` flag from the `identity` NVS namespace.
2. If **not provisioned**: bring up Wi-Fi (STA), BT controller (BLE-only), Bluedroid host, and the BluFi profile. Advertise as `AS_HomeAutomation`.
   - `RECV_STA_SSID` / `RECV_STA_PASSWD` -> staged into `wifi_config_t`.
   - `REQ_CONNECT_TO_AP` -> persisted via [storage_manager](components/storage_manager) `save_wifi_credentials()`, then `esp_wifi_connect()`.
   - `RECV_CUSTOM_DATA` (JSON `{endpoint, device_cert, private_key, root_ca}`) -> persisted via [storage_manager](components/storage_manager) `save_aws_config()` + [security_manager](components/security_manager) `store_cert()`.
   - `IP_EVENT_STA_GOT_IP` -> mark identity provisioned, deinit BluFi, release BT memory (~30 KB), publish `EVT_WIFI_CONFIGURED`.
3. If **already provisioned**: skip BluFi, publish `EVT_WIFI_CONFIGURED` for [connectivity_manager](components/connectivity_manager).
4. Factory-reset event: clear `wifi`, `aws`, `certs` (identity is write-once and preserved) and reboot back into BluFi.

BluFi link-layer security: DH key negotiation + AES-CFB128 + CRC16 (built into BluFi; no PoP required).

Current code status:

| Piece | State |
|-------|-------|
| `CONFIG_BT_ENABLED` / `CONFIG_BT_BLUEDROID_ENABLED` / `CONFIG_BT_BLE_BLUFI_ENABLE` in [sdkconfig](sdkconfig) | **on** |
| BluFi support files vendored from ESP-IDF example | [blufi_init.c](components/provisioning_manager/blufi_init.c), [blufi_security.c](components/provisioning_manager/blufi_security.c) |
| `provisioning_manager` logic | **implemented**: BluFi callback handler, AWS JSON via custom-data, factory reset, EVT_WIFI_CONFIGURED publish |
| `cloud_client_simulate_ble_receive()` | test helper only — not a real BLE GATT server |

This work is tracked as **TBD-2 / TBD-3** in [docs/Requirements_Specification.md](docs/Requirements_Specification.md).

### Module ownership: BLE vs Wi-Fi

BLE and Wi-Fi are intentionally owned by **different** modules — `connectivity_manager` is **not** a catch-all for both.

- **BLE lives entirely in [provisioning_manager](components/provisioning_manager).** It is brought up only when the device is unprovisioned, and torn down (with `esp_bt_mem_release()` to reclaim ~30 KB) immediately after `WIFI_PROV_END`. No other module links against the BT controller or host stack.
- **Wi-Fi lives entirely in [connectivity_manager](components/connectivity_manager).** It subscribes to `EVT_WIFI_CONFIGURED`, reads creds from `storage_manager`, runs `esp_wifi`, handles reconnect / backoff, and publishes `EVT_NET_READY`.
- **AWS / MQTT lives entirely in [cloud_client](components/cloud_client).** It subscribes to `EVT_NET_READY` and brings up the MQTT client.

This split exists because the two radios have very different lifecycles (BLE: one-shot at first boot; Wi-Fi: continuous) and very different dependencies, and bundling them would force every build to link the BT stack even when it cannot run.

---

## End-to-End Responsibility Matrix

Where each piece of the credential -> Wi-Fi -> AWS pipeline lives, and what is implemented today.

| Concern | Module that owns it | Status |
|---|---|---|
| BLE GATT + Wi-Fi / AWS-cert reception from mobile app | [provisioning_manager](components/provisioning_manager) (uses **BluFi** profile + Bluedroid host) | **implemented** (BluFi, custom-data JSON for AWS material) |
| Wi-Fi credentials -> NVS | [storage_manager](components/storage_manager) (namespace `wifi`) | implemented, **plaintext** |
| AWS endpoint + cert blob -> NVS | [storage_manager](components/storage_manager) (namespace `aws`) | implemented, **plaintext** |
| Device cert / private key / root CA -> NVS | [security_manager](components/security_manager) (namespace `certs`) | implemented, **plaintext** |
| NVS encryption (XTS-AES via `nvs_keys` partition) | [security_manager](components/security_manager) (owns key lifecycle + `nvs_flash_secure_init`) | not configured |
| Wi-Fi STA connect + reconnect | [connectivity_manager](components/connectivity_manager) | stub |
| AWS IoT MQTT/TLS connect (mTLS with X.509) | [cloud_client](components/cloud_client) (`esp-mqtt` + `esp-tls` + `mbedtls`) | stub (simulator only) |
| Inbound command parsing & routing (cJSON envelope) | [device_control](components/device_control) / [cloud_command.c](components/device_control/cloud_command.c) | **implemented** |
| Driver execution & GPIO / PWM | [driver_pool](components/common) + [gpio_manager](components/common) | implemented |
| Outbound telemetry / state publish | [cloud_client](components/cloud_client) | stub (logs only) |

Target data flow:

```
mobile app --BLE--> provisioning_manager --(saves)--> storage_manager / security_manager  (encrypted NVS)
                              |
                         EVT_WIFI_CONFIGURED
                              v
                      connectivity_manager  --(esp_wifi)-->  Wi-Fi AP
                              |
                         EVT_NET_READY
                              v
                         cloud_client  --(MQTT / TLS / X.509)-->  AWS IoT
                            ^       |
                    publish |       | MQTT_EVENT_DATA (subscribe)
                            |       v
                  device_control_handle_cloud_packet()  (cJSON envelope validation)
                                    |
                            device_registry lookup
                                    v
                            driver_pool.execute()
                                    v
                            gpio_manager (GPIO / LEDC)
```

---

## Build & Flash

Prerequisites:
- ESP-IDF **5.4.1** installed and `IDF_PATH` exported
- Python 3 (for the config generator)

```bash
# from the project root
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

The config generator runs automatically during `idf.py build`.

---

## Repository Layout

```
.
├── main/                       # app_main, app_core, event_bus
├── components/
│   ├── common/                 # registry, driver pool, GPIO HAL, NVS identity, flags, logging
│   ├── device_control/         # switch / fan control + cloud command parser
│   ├── cloud_client/           # cloud transport + AWS simulation
│   ├── connectivity_manager/   # Wi-Fi state (stub)
│   ├── provisioning_manager/   # provisioning / factory reset (stub)
│   ├── storage_manager/        # NVS Wi-Fi / AWS config
│   └── security_manager/       # NVS certificate storage
├── tools/config_generator/     # CSV -> config_generated.{h,c}
├── docs/                       # Requirements spec, design notes
├── CMakeLists.txt
└── sdkconfig
```

---

## Status / Roadmap

The following are **placeholders** today and are tracked in the requirements spec:

- Real MQTT-over-TLS to AWS IoT (currently simulated; `mqtt`, `esp-tls`, `mbedtls` are linked)
- **BLE provisioning** — required to receive Wi-Fi credentials and AWS certificates on first boot; `CONFIG_BT_ENABLED` is currently off and `wifi_provisioning` is not linked (see the *Provisioning (BLE)* section above)
- Wi-Fi provisioning workflow (`provisioning_manager` is a logging stub)
- Connectivity state machine (`connectivity_manager` is a logging stub)
- OTA update flow (feature flag exists)
- Secure Boot V1 / Flash Encryption (supported by build, not enabled)
- Application-level pytest suite

See [docs/Requirements_Specification.md](docs/Requirements_Specification.md) for the full requirement-by-requirement breakdown.
