/* Эталонное чтение PNG библиотекой libpng для tests/golden/png.txt.
 *
 * Те же правила, что у переноса: всё сводится к 8-битному RGBA, 16 бит —
 * старшим байтом, низкие глубины серого растягиваются, tRNS даёт альфу.
 * Печатает имя файла, размер и хэш FNV-1a пикселей построчно. */
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t h;
static void hbyte(int b) { h = (h ^ (uint32_t)(b & 255)) * 16777619u; }

int main(int argc, char** argv) {
    for (int a = 1; a < argc; ++a) {
        FILE* f = fopen(argv[a], "rb");
        const char* name = strrchr(argv[a], '/');
        name = name ? name + 1 : argv[a];
        if (!f) { printf("png %s error\n", name); continue; }
        png_structp ps = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_infop pi = png_create_info_struct(ps);
        if (setjmp(png_jmpbuf(ps))) {
            printf("png %s error\n", name);
            png_destroy_read_struct(&ps, &pi, NULL);
            fclose(f);
            continue;
        }
        png_init_io(ps, f);
        png_read_info(ps, pi);
        int ct = png_get_color_type(ps, pi);
        png_set_expand(ps);
        png_set_strip_16(ps);
        if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(ps);
        if (!(ct & PNG_COLOR_MASK_ALPHA) && !png_get_valid(ps, pi, PNG_INFO_tRNS))
            png_set_add_alpha(ps, 0xFF, PNG_FILLER_AFTER);
        png_set_interlace_handling(ps);
        png_read_update_info(ps, pi);
        png_uint_32 w = png_get_image_width(ps, pi), hh = png_get_image_height(ps, pi);
        size_t rb = png_get_rowbytes(ps, pi);
        png_bytep img = malloc(rb * hh);
        png_bytepp rows = malloc(sizeof(png_bytep) * hh);
        for (png_uint_32 y = 0; y < hh; ++y) rows[y] = img + y * rb;
        png_read_image(ps, rows);
        h = 2166136261u;
        for (size_t i = 0; i < rb * hh; ++i) hbyte(img[i]);
        printf("png %s %u %u %08x\n", name, (unsigned)w, (unsigned)hh, h);
        free(rows);
        free(img);
        png_destroy_read_struct(&ps, &pi, NULL);
        fclose(f);
    }
    return 0;
}
