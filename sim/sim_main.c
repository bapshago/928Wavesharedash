// Headless desktop renderer for the dash UI. Runs the same main/ui and
// main/core code as the firmware against LVGL on the PC and writes PNG
// screenshots of the 800x800 round panel.
//
//   dash_sim <scenario> <out.png>     one scenario
//   dash_sim --all <out_dir>          every scenario → <out_dir>/<scenario>.png
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dash_ui.h"
#include "lvgl.h"
#include "png_write.h"
#include "sim_model.h"

#define W 800
#define H 800

static uint16_t g_fb[W * H];            // RGB565, what the panel would show
static uint8_t  g_rgb[W * H * 3];
static uint16_t g_draw_buf[W * H];

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const uint16_t *src = (const uint16_t *)px_map;
    int32_t w = lv_area_get_width(area);
    for (int32_t y = area->y1; y <= area->y2; y++) {
        memcpy(&g_fb[y * W + area->x1], &src[(y - area->y1) * w], (size_t)w * 2);
    }
    lv_display_flush_ready(disp);
}

static uint32_t g_ms;

static void run_for(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        lv_tick_inc(10);
        g_ms += 10;
        sim_model_tick(g_ms);
        lv_timer_handler();
    }
}

static int render(const char *scenario, const char *out_path)
{
    g_ms = 100000;  // arbitrary non-zero time so "last frame" reads sensibly
    if (!sim_model_init(scenario, g_ms)) {
        fprintf(stderr, "unknown scenario '%s'\n", scenario);
        return 1;
    }

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_screen_load(scr);
    dash_ui_create(scr);
    if (strcmp(scenario, "service") == 0) {
        dash_ui_show_page(1);
    }
    run_for(1500);  // let needles settle and page transitions finish
    lv_refr_now(NULL);

    // Round glass: anything outside the 800 px circle is not visible on the panel.
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            uint16_t p = g_fb[y * W + x];
            uint8_t *o = &g_rgb[(y * W + x) * 3];
            int dx = x - W / 2, dy = y - H / 2;
            if (dx * dx + dy * dy > (W / 2) * (W / 2)) {
                o[0] = o[1] = o[2] = 0x30;
                continue;
            }
            o[0] = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            o[1] = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
            o[2] = (uint8_t)((p & 0x1F) * 255 / 31);
        }
    }
    lv_obj_delete(scr);

    if (png_write_rgb(out_path, g_rgb, W, H) != 0) {
        fprintf(stderr, "failed to write %s\n", out_path);
        return 1;
    }
    printf("wrote %s\n", out_path);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <scenario> <out.png> | --all <out_dir>\nscenarios:", argv[0]);
        for (int i = 0; sim_scenarios[i]; i++) {
            fprintf(stderr, " %s", sim_scenarios[i]);
        }
        fprintf(stderr, "\n");
        return 2;
    }

    lv_init();
    lv_display_t *disp = lv_display_create(W, H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, g_draw_buf, NULL, sizeof(g_draw_buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);

    if (strcmp(argv[1], "--all") == 0) {
        int rc = 0;
        for (int i = 0; sim_scenarios[i]; i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s.png", argv[2], sim_scenarios[i]);
            rc |= render(sim_scenarios[i], path);
        }
        return rc;
    }
    return render(argv[1], argv[2]);
}
