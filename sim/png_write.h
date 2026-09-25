#pragma once

#include <stdint.h>

// Write an 8-bit RGB image as an (uncompressed) PNG. Returns 0 on success.
int png_write_rgb(const char *path, const uint8_t *rgb, uint32_t w, uint32_t h);
