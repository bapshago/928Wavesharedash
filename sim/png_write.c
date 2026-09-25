// Minimal dependency-free PNG writer using stored (uncompressed) deflate
// blocks. Files are larger than a real encoder's but need no zlib.
#include "png_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static void crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        }
        crc_table[n] = c;
    }
}

static uint32_t crc_update(uint32_t crc, const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static int chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[8];
    put32(hdr, len);
    memcpy(hdr + 4, type, 4);
    uint32_t crc = crc_update(0xFFFFFFFFu, hdr + 4, 4);
    crc = crc_update(crc, data, len) ^ 0xFFFFFFFFu;
    uint8_t tail[4];
    put32(tail, crc);
    return (fwrite(hdr, 1, 8, f) == 8 && (len == 0 || fwrite(data, 1, len, f) == len) &&
            fwrite(tail, 1, 4, f) == 4) ? 0 : -1;
}

int png_write_rgb(const char *path, const uint8_t *rgb, uint32_t w, uint32_t h)
{
    crc_init();

    // Raw scanlines: filter byte 0 + RGB row.
    const size_t row = (size_t)w * 3 + 1;
    const size_t raw_len = row * h;
    uint8_t *raw = malloc(raw_len);
    if (!raw) {
        return -1;
    }
    for (uint32_t y = 0; y < h; y++) {
        raw[y * row] = 0;
        memcpy(&raw[y * row + 1], &rgb[(size_t)y * w * 3], (size_t)w * 3);
    }

    // zlib stream of stored blocks.
    const size_t nblocks = (raw_len + 65534) / 65535;
    const size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    uint8_t *z = malloc(z_len);
    if (!z) {
        free(raw);
        return -1;
    }
    size_t o = 0;
    z[o++] = 0x78;
    z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t pos = 0; pos < raw_len;) {
        size_t n = raw_len - pos > 65535 ? 65535 : raw_len - pos;
        z[o++] = (pos + n == raw_len) ? 1 : 0;
        z[o++] = (uint8_t)n;
        z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)~n;
        z[o++] = (uint8_t)(~n >> 8);
        memcpy(&z[o], &raw[pos], n);
        for (size_t i = 0; i < n; i++) {
            a = (a + raw[pos + i]) % 65521;
            b = (b + a) % 65521;
        }
        o += n;
        pos += n;
    }
    put32(&z[o], (b << 16) | a);
    o += 4;

    int rc = -1;
    FILE *f = fopen(path, "wb");
    if (f) {
        static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
        uint8_t ihdr[13];
        put32(ihdr, w);
        put32(ihdr + 4, h);
        ihdr[8] = 8;   // bit depth
        ihdr[9] = 2;   // truecolour RGB
        ihdr[10] = ihdr[11] = ihdr[12] = 0;
        rc = (fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13) == 0 &&
              chunk(f, "IDAT", z, (uint32_t)o) == 0 && chunk(f, "IEND", NULL, 0) == 0) ? 0 : -1;
        if (fclose(f) != 0) {
            rc = -1;
        }
    }
    free(z);
    free(raw);
    return rc;
}
