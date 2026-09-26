// ESP32-P4 implementation of dash_model.h: TWAI (CAN) receive, NVS-backed
// settings and odometer, and test mode. Replaces the CAN task, Preferences
// and /api endpoints of leaf_dash_v9.ino.
#include "dash_model.h"

#include <math.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "dash_model_esp.h"
#include "leaf_can.h"

static const char *TAG = "dash_model";

#if CONFIG_DASH_CAN_LISTEN_ONLY
#define CAN_LISTEN_ONLY 1
#else
#define CAN_LISTEN_ONLY 0
#endif

// Same NVS namespace and keys as the web dash's Preferences, so the layout is familiar.
#define NVS_NAMESPACE        "apdash"
#define ODO_TICK_MS          250      // integrate ~4x/second
#define ODO_SAVE_INTERVAL_MS 30000    // flash-wear limit: worst case a power cut loses 30 s of distance

typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} rx_msg_t;

static SemaphoreHandle_t s_lock;
static dash_state_t      s_state;
static dash_settings_t   s_settings;
static double            s_odo_m;
static dash_can_state_t  s_can_state = DASH_CAN_DOWN;
static nvs_handle_t      s_nvs;

static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

// ── snapshot API used by the UI ────────────────────────────────────────────

void dash_model_snapshot(dash_snapshot_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    out->vehicle = s_state;
    out->settings = s_settings;
    out->odo_m = s_odo_m;
    out->can_state = s_can_state;
    xSemaphoreGive(s_lock);
    out->now_ms = now_ms();
}

void dash_model_set_settings(const dash_settings_t *cfg)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool changed = memcmp(&s_settings, cfg, sizeof(*cfg)) != 0;
    s_settings = *cfg;
    xSemaphoreGive(s_lock);
    if (!changed) {
        return;
    }
    // Retained across reboots, like the web dash's /api/set* handlers.
    nvs_set_u8(s_nvs, "use_mph", cfg->use_mph);
    nvs_set_u8(s_nvs, "use_f", cfg->use_fahrenheit);
    nvs_set_u8(s_nvs, "odo_mi", cfg->use_odo_miles);
    nvs_commit(s_nvs);
    ESP_LOGI(TAG, "settings: mph=%d f=%d odo_mi=%d", cfg->use_mph, cfg->use_fahrenheit, cfg->use_odo_miles);
}

// ── NVS ────────────────────────────────────────────────────────────────────

static bool prefs_get_bool(const char *key, bool dflt)
{
    uint8_t v;
    return nvs_get_u8(s_nvs, key, &v) == ESP_OK ? v != 0 : dflt;
}

// Doubles are stored as their raw 64-bit pattern.
static void prefs_put_double(const char *key, double v)
{
    uint64_t raw;
    memcpy(&raw, &v, sizeof(raw));
    nvs_set_u64(s_nvs, key, raw);
}

static bool prefs_get_double(const char *key, double *out)
{
    uint64_t raw;
    if (nvs_get_u64(s_nvs, key, &raw) != ESP_OK) {
        return false;
    }
    memcpy(out, &raw, sizeof(*out));
    return isfinite(*out);
}

static esp_err_t load_persisted(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs init");
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs), TAG, "nvs open");

    s_settings.use_mph = prefs_get_bool("use_mph", false);
    s_settings.use_fahrenheit = prefs_get_bool("use_f", false);
    s_settings.use_odo_miles = prefs_get_bool("odo_mi", false);
    if (!prefs_get_double("odo_m", &s_odo_m)) {
        s_odo_m = 0.0;
    }

    // One-time odometer seed: set the starting reading once on a device that has
    // never been seeded, then never overwrite it so real accumulated mileage is
    // preserved. Erasing flash re-seeds it.
    if (!prefs_get_bool("odo_seeded", false)) {
        s_odo_m = DASH_ODO_INITIAL_MILES * DASH_METERS_PER_MILE;
        prefs_put_double("odo_m", s_odo_m);
        nvs_set_u8(s_nvs, "odo_seeded", 1);
        nvs_commit(s_nvs);
        ESP_LOGI(TAG, "odometer seeded to %.0f miles", DASH_ODO_INITIAL_MILES);
    }
    ESP_LOGI(TAG, "prefs: mph=%d f=%d odo_mi=%d odo=%.1f km", s_settings.use_mph, s_settings.use_fahrenheit,
             s_settings.use_odo_miles, s_odo_m / 1000.0);
    return ESP_OK;
}

#if !CONFIG_DASH_TEST_MODE  // test mode never starts CAN
static QueueHandle_t      s_rx_queue;
static twai_node_handle_t s_node;

// ── CAN ────────────────────────────────────────────────────────────────────

static bool IRAM_ATTR on_rx_done(twai_node_handle_t node, const twai_rx_done_event_data_t *edata, void *ctx)
{
    rx_msg_t msg = { 0 };
    twai_frame_t frame = { .buffer = msg.data, .buffer_len = sizeof(msg.data) };
    if (twai_node_receive_from_isr(node, &frame) != ESP_OK) {
        return false;
    }
    if (frame.header.ide || frame.header.rtr) {
        return false;  // Leaf / ZombieVerter frames are standard 11-bit data frames
    }
    msg.id = frame.header.id;
    msg.dlc = frame.header.dlc > 8 ? 8 : (uint8_t)frame.header.dlc;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_rx_queue, &msg, &woken);
    return woken == pdTRUE;
}

static bool IRAM_ATTR on_state_change(twai_node_handle_t node, const twai_state_change_event_data_t *edata,
                                      void *ctx)
{
    static const dash_can_state_t map[] = {
        [TWAI_ERROR_ACTIVE] = DASH_CAN_OK,
        [TWAI_ERROR_WARNING] = DASH_CAN_WARNING,
        [TWAI_ERROR_PASSIVE] = DASH_CAN_PASSIVE,
        [TWAI_ERROR_BUS_OFF] = DASH_CAN_BUS_OFF,
    };
    s_can_state = map[edata->new_sta];  // single aligned word write; read under the lock elsewhere
    return false;
}

static void can_rx_task(void *arg)
{
    rx_msg_t msg;
    for (;;) {
        if (xQueueReceive(s_rx_queue, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
            if (s_can_state == DASH_CAN_BUS_OFF) {
                ESP_LOGW(TAG, "bus off, recovering");
                twai_node_recover(s_node);
            }
            continue;
        }
        uint32_t t = now_ms();
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_state.frames_rx++;
        s_state.last_rx_ms = t ? t : 1;
        if (leaf_can_decode(msg.id, msg.data, msg.dlc, &s_state)) {
            s_state.frames_decoded++;
        }
        xSemaphoreGive(s_lock);
    }
}

static esp_err_t can_start(void)
{
    s_rx_queue = xQueueCreate(64, sizeof(rx_msg_t));
    ESP_RETURN_ON_FALSE(s_rx_queue, ESP_ERR_NO_MEM, TAG, "rx queue");

    twai_onchip_node_config_t cfg = {
        .io_cfg = {
            .tx = CONFIG_DASH_CAN_TX_GPIO,
            .rx = CONFIG_DASH_CAN_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = { .bitrate = CONFIG_DASH_CAN_BITRATE },
        .tx_queue_depth = 1,  // the dash never transmits
        .flags = {
            .enable_listen_only = CAN_LISTEN_ONLY,
            .no_receive_rtr = 1,
        },
    };
    ESP_RETURN_ON_ERROR(twai_new_node_onchip(&cfg, &s_node), TAG, "twai node");
    twai_event_callbacks_t cbs = {
        .on_rx_done = on_rx_done,
        .on_state_change = on_state_change,
    };
    ESP_RETURN_ON_ERROR(twai_node_register_event_callbacks(s_node, &cbs, NULL), TAG, "twai callbacks");
    ESP_RETURN_ON_ERROR(twai_node_enable(s_node), TAG, "twai enable");
    s_can_state = DASH_CAN_OK;

    xTaskCreate(can_rx_task, "can_rx", 4096, NULL, 8, NULL);
    ESP_LOGI(TAG, "CAN started: TX=GPIO%d RX=GPIO%d %d bit/s%s", CONFIG_DASH_CAN_TX_GPIO,
             CONFIG_DASH_CAN_RX_GPIO, CONFIG_DASH_CAN_BITRATE,
             CAN_LISTEN_ONLY ? " (listen-only)" : "");
    return ESP_OK;
}

#endif  // !CONFIG_DASH_TEST_MODE

// ── odometer + test data ───────────────────────────────────────────────────

static void housekeeping_task(void *arg)
{
    uint32_t last_save = now_ms();
    double last_saved_m = s_odo_m;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(ODO_TICK_MS));
        uint32_t t = now_ms();

        xSemaphoreTake(s_lock, portMAX_DELAY);
#if CONFIG_DASH_TEST_MODE
        // Fake data only: don't add bench-test "miles" to the car's odometer.
        dash_test_data(&s_state, t, &s_settings);
#else
        s_odo_m = dash_odo_integrate(s_odo_m, s_state.motor_rpm, ODO_TICK_MS);
#endif
        double odo = s_odo_m;
        xSemaphoreGive(s_lock);

        // Persist at most every 30 s and only when it actually changed.
        if (t - last_save >= ODO_SAVE_INTERVAL_MS) {
            last_save = t;
            if (fabs(odo - last_saved_m) >= 1.0) {
                prefs_put_double("odo_m", odo);
                nvs_commit(s_nvs);
                last_saved_m = odo;
            }
        }
    }
}

esp_err_t dash_model_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "mutex");
    dash_state_init(&s_state);
    ESP_RETURN_ON_ERROR(load_persisted(), TAG, "settings");

#if CONFIG_DASH_TEST_MODE
    s_can_state = DASH_CAN_TEST_MODE;
    ESP_LOGW(TAG, "TEST MODE: showing fake data, CAN not started");
#else
    esp_err_t err = can_start();
    if (err != ESP_OK) {
        // Keep the screen running so the fault is visible on the service page.
        ESP_LOGE(TAG, "CAN init failed: %s", esp_err_to_name(err));
        s_can_state = DASH_CAN_DOWN;
    }
#endif

    xTaskCreate(housekeeping_task, "dash_house", 4096, NULL, 5, NULL);

    const float mph_per_krpm = dash_kph_to_mph(dash_rpm_to_kph(1000));
    ESP_LOGI(TAG, "speed scale: %.3f mph per 1k RPM | ~%.1f mph @ 10k RPM", mph_per_krpm, mph_per_krpm * 10);
    return ESP_OK;
}
