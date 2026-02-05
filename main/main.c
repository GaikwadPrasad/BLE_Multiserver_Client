#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "nvs_flash.h"
#include "esp_err.h"


#define MAX_BLE_SERVERS 5 // Max Server connection

typedef struct
{
 bool connected;
 esp_bd_addr_t bda;
 uint16_t conn_id;
 uint16_t service_start;
 uint16_t service_end;
 uint16_t char_handle;
}ble_peer_t;

static ble_peer_t peers[MAX_BLE_SERVERS]; 

static esp_gatt_if_t client_gatt_if;

static const char *TAG = "BLE_CLIENT";

static const uint8_t SERVICE_UUID_ARR[16] = {
    0xab,0x00,0xcb,0x0c,0xcd,0x00,0xc0,0xbc,
    0x0a,0xca,0xad,0x00,0xa1,0xbb,0x00,0x01
};

static const uint8_t CHAR_UUID_ARR[16] = {
    0xab,0x10,0xcb,0x0c,0xcd,0x00,0xc0,0xbc,
    0x0a,0xca,0xad,0x00,0xa1,0xbb,0x00,0x02
};

static esp_bt_uuid_t service_uuid = {
    .len = ESP_UUID_LEN_128
};

static esp_bt_uuid_t char_uuid = {
    .len = ESP_UUID_LEN_128
};

static int get_free_peer(void) //Finds empty slot for a new server connection.
{
   for (int i = 0; i < MAX_BLE_SERVERS; i++){
    if(!peers[i].connected){
        return i;}}
    return -1;
}

static int find_peer(uint16_t conn_id) //conn_id is the KEY.
{
    for (int i = 0; i < MAX_BLE_SERVERS; i++)
        if (peers[i].connected && peers[i].conn_id == conn_id)
            return i;
    return -1;
}

static int find_peer_by_bda(esp_bd_addr_t bda)
{
    for (int i = 0; i < MAX_BLE_SERVERS; i++) {
        if (peers[i].connected &&
            memcmp(peers[i].bda, bda, ESP_BD_ADDR_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_peer_by_char_handle(uint16_t handle)
{
    for (int i = 0; i < MAX_BLE_SERVERS; i++) {
        if (peers[i].connected &&
            peers[i].char_handle == handle) {
            return i;
        }
    }
    return -1;
}

static bool uuid_match(uint8_t *uuid)
{
    return memcmp(uuid, SERVICE_UUID_ARR, 16) == 0;
}

static esp_ble_scan_params_t scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scanning...");
        esp_ble_gap_start_scanning(0);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        uint8_t len;
        uint8_t *uuid = esp_ble_resolve_adv_data(
            param->scan_rst.ble_adv,
            ESP_BLE_AD_TYPE_128SRV_CMPL,
            &len);

        if (uuid && uuid_match(uuid)) {

            if (get_free_peer() < 0) return; // max reached

            ESP_LOGI(TAG, "Server found → Connecting");

            esp_ble_gattc_open(
                client_gatt_if,
                param->scan_rst.bda,
                param->scan_rst.ble_addr_type,
                true);
        }
        break;
    }
    default:
        break;
    }
}

static void gattc_event_handler(esp_gattc_cb_event_t event,
                                esp_gatt_if_t gattc_if,
                                esp_ble_gattc_cb_param_t *param)
{
    switch (event) {

    case ESP_GATTC_REG_EVT:
        client_gatt_if = gattc_if;
        memcpy(service_uuid.uuid.uuid128, SERVICE_UUID_ARR, 16);
        memcpy(char_uuid.uuid.uuid128, CHAR_UUID_ARR, 16);
        esp_ble_gap_set_scan_params(&scan_params);
        break;

    case ESP_GATTC_CONNECT_EVT: {
        int idx = get_free_peer();
        if (idx < 0) break;

        peers[idx].connected = true;
        peers[idx].conn_id = param->connect.conn_id;
        memcpy(peers[idx].bda, param->connect.remote_bda, ESP_BD_ADDR_LEN);

        ESP_LOGI(TAG, "Connected peer %d", idx);

        esp_ble_gattc_search_service(
            gattc_if,
            peers[idx].conn_id,
            &service_uuid);
        break;
    }

    case ESP_GATTC_SEARCH_RES_EVT: {
        int idx = find_peer(param->search_res.conn_id);
        if (idx < 0) break;

        peers[idx].service_start = param->search_res.start_handle;
        peers[idx].service_end   = param->search_res.end_handle;
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
        int idx = find_peer(param->search_cmpl.conn_id);
        if (idx < 0) break;

        esp_gattc_char_elem_t char_elem;
        uint16_t count = 1;

        esp_ble_gattc_get_char_by_uuid(
            gattc_if,
            peers[idx].conn_id,
            peers[idx].service_start,
            peers[idx].service_end,
            char_uuid,
            &char_elem,
            &count);

        if (count > 0) {
            peers[idx].char_handle = char_elem.char_handle;

            esp_ble_gattc_register_for_notify(
                gattc_if,
                peers[idx].bda,
                peers[idx].char_handle);
        }
        break;
    }

case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
    int idx = find_peer_by_char_handle(param->reg_for_notify.handle);
    if (idx < 0) break;

    // uint8_t notify_en[] = "HELLO";

    // esp_ble_gattc_write_char_descr(
    //     gattc_if,
    //     peers[idx].conn_id,                 // VALID
    //     param->reg_for_notify.handle + 1,   // CCCD
    //     sizeof(notify_en),
    //     notify_en,
    //     ESP_GATT_WRITE_TYPE_RSP,
    //     ESP_GATT_AUTH_REQ_NONE
    // );
     
uint8_t msg[] = "HELLO SERVER";
            esp_ble_gattc_write_char_descr(
                gattc_if,
                peers[idx].conn_id,
                param->reg_for_notify.handle + 1,
                sizeof(msg),
                msg,
                ESP_GATT_WRITE_TYPE_RSP,
                ESP_GATT_AUTH_REQ_NONE);

    ESP_LOGI(TAG, "Notifications enabled for peer %d", idx);
    break;
}



    case ESP_GATTC_NOTIFY_EVT: {
        int idx = find_peer(param->notify.conn_id);
        if (idx < 0) break;

        ESP_LOGI(TAG, "Notify from peer %d", idx);
        ESP_LOG_BUFFER_HEX(TAG,
            param->notify.value,
            param->notify.value_len);
        break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
        int idx = find_peer(param->disconnect.conn_id);
        if (idx >= 0) {
            ESP_LOGI(TAG, "Peer %d disconnected", idx);
            memset(&peers[idx], 0, sizeof(ble_peer_t));
        }
        break;
    }

    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT); 

    esp_bt_controller_config_t bt_cfg =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gattc_register_callback(gattc_event_handler);
    esp_ble_gattc_app_register(0);

    ESP_LOGI(TAG, "BLE Multi-Server Client Ready");
}


