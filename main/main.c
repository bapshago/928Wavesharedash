// Porsche 928 EV dash on the Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C.
//
// Port of the Angry Pixie web dash (leaf_dash_v9): instead of serving a web
// page to a tablet, the same CAN data is drawn straight onto the 800x800
// round touch display with LVGL.
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "dash_model_esp.h"
#include "dash_ui.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "928 EV dash booting");
    ESP_ERROR_CHECK(dash_model_start());

    // Display + touch + LVGL task, as in Waveshare's 08_lvgl_demo_v9 example.
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    if (bsp_display_start_with_config(&cfg) == NULL) {
        ESP_LOGE(TAG, "display start failed");
        return;
    }

    bsp_display_lock(-1);
    dash_ui_create(lv_screen_active());
    bsp_display_unlock();

    bsp_display_brightness_set(CONFIG_DASH_BRIGHTNESS);
    ESP_LOGI(TAG, "dash running");
}
