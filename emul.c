/**
 * @file
 * @author  Miguel Vanhove / Crazy Piri <redbug@crazypiri.eu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * https://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * Convert printer output from Oxford PAO (Amtrad CPC software) to gif
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gifenc.h"

#define WIDTH  (1488)
#define HEIGHT (2104 * 2)

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;

// https://escpos.readthedocs.io/en/latest/layout.html#b33
// https://download.brother.com/welcome/docp000584/cv_pt9700_eng_escp_103.pdf
// https://www.starmicronics.com/support/Mannualfolder/escpos_cm_en.pdf

typedef struct printer_paper_s {
    u8 *paper;  // 8,27 x 11,69 with 180 DPI
    u32 mx, my, mc;
    u32 px, py;

} printer_paper_t;



void saveScreen(printer_paper_t *p, char *output)
{
    u16 x, y;

    if ((p->mx == 0) || (p->my == 0)) {
        return;
    }

    u8 palette[32 * 3];

    palette[0 * 3 + 0] = 0xff;
    palette[0 * 3 + 1] = 0xff;
    palette[0 * 3 + 2] = 0xff;

    palette[1 * 3 + 0] = 0x0;
    palette[1 * 3 + 1] = 0x0;
    palette[1 * 3 + 2] = 0x0;


// Ajust border
    p->mx += 10;
    p->my += 10;


    ge_GIF *gif = ge_new_gif(output, p->mx, p->my, palette, 2, -1);


    for (y = 0; y < p->my; y++) {
        for (x = 0; x < p->mx; x++) {
            gif->frame[x + y * p->mx] = p->paper[x + y * WIDTH];
        }
    }

    // memcpy(gif->frame, paper, WIDTH * HEIGHT);

    ge_add_frame(gif, 0);
    ge_close_gif(gif);


} /* saveScreen */



void putPixel(printer_paper_t *p, u32 x, u32 y)
{
    if ((x >= WIDTH) || (y >= HEIGHT)) {
        return;
    }

    if (x > p->mx) {
        p->mx = x + 1;
    }
    if (y > p->my) {
        p->my = y + 1;
    }

    p->paper[y * WIDTH + x] = 1;
    // paper[y * WIDTH + x]++;
    // if ( paper[y * WIDTH + x] > p->mc) {
    //     p->mc = paper[y * WIDTH + x];
    // }
}

u8 bitMode(printer_paper_t *p, u8 *raw, u8 m, u8 nL, u8 nH)
{
    u8 c = ((m == 0) || (m == 1)) ? 1 : 3;

    u32 z;
    u8 z1;

    u32 py0;

    for (z = 0; z < nL + nH * 256 * c; z++) {
        py0 = p->py;
        u8 car = raw[z];
        for (z1 = 0; z1 < 8; z1++) {
            if ((p->px < WIDTH) && (py0 < HEIGHT)) {
                if ((car & 128) != 0) {
                    putPixel(p, p->px, py0);
                    putPixel(p, p->px, py0 + 1);
                }
                car = (car << 1);
            }
            py0 += 2;
        }
        p->px++;
    }

    return c;

} /* bitMode */

int main(int argc, char **argv)
{
    u32 size;
    u8 *raw;

    printer_paper_t *p;

    p = (printer_paper_t *)malloc(sizeof(printer_paper_t));

    p->paper = malloc(WIDTH * HEIGHT);
    memset(p->paper, 0, WIDTH * HEIGHT);

    FILE *fic = fopen(argv[1], "rb");
    if (fic == NULL) {
        printf("Couldn't open %s\n", argv[1]);
        return 0;
    }

    fseek(fic, 0, SEEK_END);
    size = ftell(fic);
    fseek(fic, 0, SEEK_SET);

    printf("File size: %u\n", size);

    raw = (u8 *)malloc(size);
    if (raw == NULL) {
        return 0;
    }
    fread(raw, 1, size, fic);
    fclose(fic);

    p->mx = 0;
    p->my = 0;

    u32 pos = 0;
    u8 c, m, nL, nH;
    u8 lineFeed = 34;  // 1/6 inch - 30/180

    p->px = p->py = 0;

    while (pos < size) {
        switch (raw[pos]) {
            case 0x1b:

                switch (raw[pos + 1]) {
                    case 0x2a:  // Specify bit image mode
                        m = raw[pos + 2];   // 0,1,32 or 33
                        nL = raw[pos + 3];  // 0 ≤ nH ≤ 3
                        nH = raw[pos + 4];  // 0 ≤ nH ≤ 7

                        c = bitMode(p, raw + pos + 4, m, nL, nH);
                        // printf("@%08x: Bit %d %dx%d\n", pos, m, nL, nH);

                        pos += 4 + nL + nH * 256 * c;
                        break;

                    case 0x4c:     // 8-dot double-density bit image
                        nL = raw[pos + 2]; // 0 ≤ nH ≤ 3
                        nH = raw[pos + 3]; // 0 ≤ nH ≤ 7

                        c = bitMode(p, raw + pos + 3, 1, nL, nH);
                        // printf("@%08x: Bit %d %dx%d\n", pos, m, nL, nH);

                        pos += 3 + nL + nH * 256 * c;
                        break;

                    case 0x33: // Set line feed amount
                        c = raw[pos + 2];
                        // printf("@%08x: Linefeed height: %d\n", pos, c);
                        // 1/6 inch per c - 30 dots per c
                        lineFeed = c;
                        pos += 2;
                        break;

                    case 0x35: // Cancel italic font -- TODO: check
                        c = raw[pos + 2];
                        // printf("@%08x: Cancel italic font: %02X\n", pos, c);
                        pos += 2;
                        break;

                    case 0x40: // Initialize printer
                        pos++;
                        break;

                    case 0x6c: // Set left margin
                        c = raw[pos + 2];
                        // printf("@%08x: Set left margin: %02X\n", pos, c);
                        pos += 2;
                        break;

                    default:
                        printf("ESC %02X (%c)\n", raw[pos + 1], raw[pos + 1]);
                        break;
                } /* switch */

                break;
            case 0x0a:
                // printf("@%08x: Linefeed %d at %d,%d\n", pos, lineFeed, p->px, p->py);
                if (lineFeed < 4) lineFeed = 2;
                p->py += lineFeed / 2;
                // if (lineFeed == 20) {
                //     py += 8;
                // } else {
                //     py += 1;
                // }
                p->px = 0;
                break;
            default:
                if (raw[pos] >= 32) {
                    printf("@%08x: %02X (%c)\n", pos, raw[pos], raw[pos]);
                } else {
                    printf("@%08x: %02X at %d,%d\n", pos, raw[pos], p->px, p->py);
                }
                break;
        } /* switch */
        pos++;
    } /* main */

    // printf("Pos %d,%d\n", p->px, p->py);
    printf("Pixel size %d,%d\n", p->mx, p->my);
    // printf("Max color %d\n", p->mc);


    saveScreen(p, argv[2]);

    free(p->paper);
    free(p);

    // printf("Done\n");

    return 0;
} /* main */