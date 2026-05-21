/*
 * provisioning_manager.c -- BluFi-based Wi-Fi & AWS provisioning.
 *
 * Owns the BLE side of first-boot provisioning per FR-PROV-1..9.
 *
 * BluFi (Espressif's BLE-based Wi-Fi config protocol) is preferred over
 * `wifi_provisioning` here because (a) the user already has a working BluFi
 * sample, (b) the Espressif BluFi mobile app is freely available, and (c) the
 * "custom data" frame gives us a clean side-channel for the AWS endpoint +
 * X.509 material without needing a custom protocomm endpoint.
 *
 * Lifecycle:
 *   1. provisioning_manager_init():  create FreeRTOS sync primitives.
 *   2. provisioning_manager_task():  if already provisioned, publish
 *      EVT_WIFI_CONFIGURED and exit; otherwise bring up Wi-Fi (STA) + BT
 *      (Bluedroid) + BluFi profile and wait for IP_EVENT_STA_GOT_IP.
 *   3. On STA_GOT_IP: mark identity provisioned, tear BLE down (reclaiming
 *      ~30 KB via esp_bt_mem_release), publish EVT_WIFI_CONFIGURED, exit.
 *
 * Inbound BluFi messages we handle:
 *   - RECV_STA_SSID / RECV_STA_PASSWD -> staged into sta_config
 *   - REQ_CONNECT_TO_AP               -> persist creds + esp_wifi_connect()
 *   - RECV_CUSTOM_DATA (JSON)         -> parse + persist AWS endpoint/certs
 *
 * Expected custom-data JSON payload:
 *   { "endpoint":"...", "device_cert":"...", "private_key":"...", "root_ca":"..." }
 */

#include "provisioning_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "nvs.h"

#include "esp_blufi_api.h"
#include "esp_blufi.h"

#include "cJSON.h"

#include "event_bus.h"
#include "nvs_identity.h"
#include "storage_manager.h"
#include "security_manager.h"
#include "blufi_internal.h"

static const char *TAG = "provisioning_mgr";

#define PROV_WIFI_MAX_RETRY 5

static EventGroupHandle_t s_wifi_evt_group;
#define WIFI_GOT_IP_BIT  BIT0

static volatile bool s_complete       = false;
static volatile bool s_ble_connected  = false;
static volatile bool s_sta_connected  = false;
static volatile bool s_sta_got_ip     = false;
static volatile bool s_sta_connecting = false;
static uint8_t       s_retry_count    = 0;

static wifi_config_t s_sta_cfg;

// -------------------- helpers --------------------

static void post_event(uint16_t id)
{
    app_event_t ev = { .event_id = id, .data = NULL };
    event_bus_post(&ev);
}

static esp_err_t erase_namespace(const char *ns)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

// -------------------- Wi-Fi / IP event handlers --------------------

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "STA got IP");
        s_sta_got_ip = true;
        s_sta_connecting = false;
        xEventGroupSetBits(s_wifi_evt_group, WIFI_GOT_IP_BIT);
        if (s_ble_connected) {
            esp_blufi_extra_info_t info = {0};
            info.sta_ssid     = s_sta_cfg.sta.ssid;
            info.sta_ssid_len = strlen((char *)s_sta_cfg.sta.ssid);
            wifi_mode_t mode = WIFI_MODE_STA;
            esp_wifi_get_mode(&mode);
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    switch (id) {
    case WIFI_EVENT_STA_START:
        // We do not auto-connect on STA_START; we wait for REQ_CONNECT_TO_AP
        // (during provisioning) or for the connectivity_manager to kick a
        // connect after EVT_WIFI_CONFIGURED.
        break;
    case WIFI_EVENT_STA_CONNECTED:
        s_sta_connected  = true;
        s_sta_connecting = false;
        s_retry_count    = 0;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        s_sta_connected = false;
        s_sta_got_ip    = false;
        if (s_sta_connecting && s_retry_count++ < PROV_WIFI_MAX_RETRY) {
            ESP_LOGW(TAG, "STA disconnected, retry %u", s_retry_count);
            s_sta_connecting = (esp_wifi_connect() == ESP_OK);
        } else {
            s_sta_connecting = false;
            if (s_ble_connected) {
                esp_blufi_extra_info_t info = {0};
                wifi_mode_t mode = WIFI_MODE_STA;
                esp_wifi_get_mode(&mode);
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, &info);
            }
        }
        break;
    default:
        break;
    }
}

// -------------------- BluFi callback --------------------

static void try_persist_aws_json(const uint8_t *data, uint32_t len)
{
    // Try parse the custom-data buffer as JSON; silently ignore if not JSON
    // (the field is a general-purpose side-channel and may carry other data
    // in the future).
    cJSON *root = cJSON_ParseWithLength((const char *)data, len);
    if (!root) {
        ESP_LOGW(TAG, "custom_data: not JSON (%lu bytes ignored)", (unsigned long)len);
        return;
    }
    cJSON *jep   = cJSON_GetObjectItemCaseSensitive(root, "endpoint");
    cJSON *jcert = cJSON_GetObjectItemCaseSensitive(root, "device_cert");
    cJSON *jkey  = cJSON_GetObjectItemCaseSensitive(root, "private_key");
    cJSON *jca   = cJSON_GetObjectItemCaseSensitive(root, "root_ca");

    if (cJSON_IsString(jep)   && jep->valuestring &&
        cJSON_IsString(jcert) && jcert->valuestring &&
        cJSON_IsString(jkey)  && jkey->valuestring &&
        cJSON_IsString(jca)   && jca->valuestring) {
        bool ok = storage_manager_save_aws_config(jep->valuestring, jcert->valuestring);
        if (ok) {
            ok = security_manager_store_cert(jcert->valuestring,
                                             jkey->valuestring,
                                             jca->valuestring);
        }
        ESP_LOGI(TAG, "custom_data: AWS material persist %s", ok ? "ok" : "FAILED");
    } else {
        ESP_LOGW(TAG, "custom_data: JSON missing required AWS fields");
    }
    cJSON_Delete(root);
}

static void blufi_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_LOGI(TAG, "BluFi init finished, starting BLE adv");
        esp_blufi_adv_start();
        break;

    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        ESP_LOGI(TAG, "BluFi deinit finished");
        break;

    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_LOGI(TAG, "BLE connected");
        s_ble_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;

    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_LOGI(TAG, "BLE disconnected");
        s_ble_connected = false;
        blufi_security_deinit();
        // Restart advertising only if we still need provisioning.
        if (!s_sta_got_ip) esp_blufi_adv_start();
        break;

    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        ESP_ERROR_CHECK(esp_wifi_set_mode(param->wifi_mode.op_mode));
        break;

    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_LOGI(TAG, "REQ_CONNECT_TO_AP ssid=\"%s\"", (char *)s_sta_cfg.sta.ssid);
        // Persist Wi-Fi credentials before kicking off the connect so a
        // reboot mid-provisioning doesn't lose them.
        storage_manager_save_wifi_credentials((char *)s_sta_cfg.sta.ssid,
                                              (char *)s_sta_cfg.sta.password);
        esp_wifi_disconnect();
        esp_wifi_set_config(WIFI_IF_STA, &s_sta_cfg);
        s_retry_count = 0;
        s_sta_connecting = (esp_wifi_connect() == ESP_OK);
        break;

    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        esp_wifi_disconnect();
        break;

    case ESP_BLUFI_EVENT_REPORT_ERROR:
        ESP_LOGE(TAG, "BluFi report error %d", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;

    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode = WIFI_MODE_STA;
        esp_wifi_get_mode(&mode);
        esp_blufi_extra_info_t info = {0};
        info.sta_ssid     = s_sta_cfg.sta.ssid;
        info.sta_ssid_len = strlen((char *)s_sta_cfg.sta.ssid);
        if (s_sta_got_ip) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        } else if (s_sta_connecting) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, 0, &info);
        } else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, &info);
        }
        break;
    }

    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        esp_blufi_disconnect();
        break;

    case ESP_BLUFI_EVENT_RECV_STA_SSID: {
        size_t maxlen = sizeof(s_sta_cfg.sta.ssid);
        if (param->sta_ssid.ssid_len >= maxlen) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memcpy(s_sta_cfg.sta.ssid, param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        s_sta_cfg.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        ESP_LOGI(TAG, "RECV STA SSID \"%s\"", (char *)s_sta_cfg.sta.ssid);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_STA_PASSWD: {
        size_t maxlen = sizeof(s_sta_cfg.sta.password);
        if (param->sta_passwd.passwd_len >= maxlen) {
            esp_blufi_send_error_info(ESP_BLUFI_DATA_FORMAT_ERROR);
            break;
        }
        memcpy(s_sta_cfg.sta.password, param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        s_sta_cfg.sta.password[param->sta_passwd.passwd_len] = '\0';
        ESP_LOGI(TAG, "RECV STA PASSWORD (%u bytes)", (unsigned)param->sta_passwd.passwd_len);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        ESP_LOGI(TAG, "RECV CUSTOM_DATA (%lu bytes)", (unsigned long)param->custom_data.data_len);
        try_persist_aws_json(param->custom_data.data, param->custom_data.data_len);
        break;

    // Unused frames -- declared explicitly so the compiler enum-coverage warns
    // us when ESP-IDF adds new ones.
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
    case ESP_BLUFI_EVENT_RECV_USERNAME:
    case ESP_BLUFI_EVENT_RECV_CA_CERT:
    case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
    case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
    case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
    case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
    default:
        break;
    }
}

static esp_blufi_callbacks_t s_blufi_callbacks = {
    .event_cb               = blufi_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func           = blufi_aes_encrypt,
    .decrypt_func           = blufi_aes_decrypt,
    .checksum_func          = blufi_crc_checksum,
};

// -------------------- bring-up helpers --------------------

static esp_err_t bring_up_wifi(void)
{
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;

    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;

    static bool s_sta_netif_created = false;
    if (!s_sta_netif_created) {
        esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
        if (!sta_netif) return ESP_FAIL;
        s_sta_netif_created = true;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &ip_event_handler,   NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    e = esp_wifi_init(&cfg);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

static esp_err_t bring_up_blufi(void)
{
    esp_err_t e = esp_blufi_controller_init();
    if (e != ESP_OK) return e;

    e = esp_blufi_host_and_cb_init(&s_blufi_callbacks);
    if (e != ESP_OK) return e;

    ESP_LOGI(TAG, "BluFi version %04x", esp_blufi_get_version());
    return ESP_OK;
}

static void tear_down_blufi(void)
{
    // Best-effort teardown; ignore individual failures since we are heading
    // out of the BLE phase anyway. Frees ~30 KB.
    esp_blufi_host_deinit();
    esp_blufi_controller_deinit();
    (void)esp_bt_mem_release(ESP_BT_MODE_BTDM);
}

// -------------------- public API --------------------

void provisioning_manager_init(void)
{
    if (s_wifi_evt_group == NULL) {
        s_wifi_evt_group = xEventGroupCreate();
    }
    ESP_LOGI(TAG, "provisioning manager initialized");
}

void provisioning_manager_task(void *arg)
{
    (void)arg;

    if (nvs_identity_is_provisioned()) {
        ESP_LOGI(TAG, "device already provisioned; skipping BluFi");
        s_complete = true;
        post_event(EVT_WIFI_CONFIGURED);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "device not provisioned; starting BluFi flow");

    esp_err_t e = bring_up_wifi();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi bring-up failed: %s", esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    e = bring_up_blufi();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "BluFi bring-up failed: %s", esp_err_to_name(e));
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    // Wait until the device has joined Wi-Fi (got IP). The user may need
    // several rounds with the app to deliver SSID/PASSWORD/custom-data;
    // BluFi keeps the BLE link alive while we wait.
    ESP_LOGI(TAG, "waiting for STA_GOT_IP via BluFi...");
    xEventGroupWaitBits(s_wifi_evt_group, WIFI_GOT_IP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // Mark identity provisioned and tear BLE down.
    esp_err_t pe = nvs_identity_set_provisioned(true);
    if (pe != ESP_OK) {
        ESP_LOGE(TAG, "failed to mark identity provisioned: %s", esp_err_to_name(pe));
    }

    ESP_LOGI(TAG, "provisioning complete; tearing down BLE");
    tear_down_blufi();

    s_complete = true;
    ESP_LOGI(TAG, "publishing EVT_WIFI_CONFIGURED");
    post_event(EVT_WIFI_CONFIGURED);

    vTaskDelete(NULL);
}

void provisioning_manager_handle_event(app_event_t *event)
{
    if (!event) return;
    if (event->event_id == EVT_FACTORY_RESET) {
        provisioning_manager_factory_reset();
        return;
    }
    ESP_LOGD(TAG, "unhandled event id %u", event->event_id);
}

bool provisioning_manager_is_complete(void)
{
    return s_complete;
}

void provisioning_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "FACT: factory reset requested - erasing wifi/aws/certs");
    erase_namespace("wifi");
    erase_namespace("aws");
    erase_namespace("certs");
    nvs_identity_set_provisioned(false);
    ESP_LOGW(TAG, "FACT: rebooting into BluFi provisioning");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}
