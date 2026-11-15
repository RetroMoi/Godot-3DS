/*************************************************************************/
/*  util.cpp                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                    http://www.godotengine.org                         */
/*************************************************************************/
/* Copyright (c) 2007-2016 Juan Linietsky, Ariel Manzur.                 */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifdef _3DS

#include "util.h"

u32 next_pow2(u32 v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v >= TEX_MIN_SIZE ? v : TEX_MIN_SIZE;
}

// Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 morton_interleave(u32 x, u32 y)
{
	u32 i = (x & 7) | ((y & 7) << 8); // ---- -210
	i = (i ^ (i << 2)) & 0x1313;      // ---2 --10
	i = (i ^ (i << 1)) & 0x1515;      // ---2 -1-0
	i = (i | (	i >> 7)) & 0x3F;
	return i;
}

//Grabbed from Citra Emulator (citra/src/video_core/utils.h)
static inline u32 get_morton_offset(u32 x, u32 y, u32 bytes_per_pixel)
{
	u32 i = morton_interleave(x, y);
	unsigned int offset = (x & ~7) * 8;
	return (i + offset) * bytes_per_pixel;
}

// Morton (Z-order) position of each pixel inside an 8x8 tile, indexed [y][x].
static u8 morton_table[64];
static bool morton_table_ready = false;

static void ensure_morton_table()
{
	if (!morton_table_ready) {
		for (u32 y = 0; y < 8; y++)
			for (u32 x = 0; x < 8; x++)
				morton_table[y * 8 + x] = morton_interleave(x, y);
		morton_table_ready = true;
	}
}

void texture_tile_indices(const C3D_Tex *tex, const u8 *src, int w, int h, u8 *dest)
{
	// Same layout as texture_tile_sw, one byte (a palette index) per texel, padding repeating
	// the last column/row.
	if (w <= 0 || h <= 0)
		return;
	ensure_morton_table();
	const int tw = tex->width;
	for (int y = 0; y < tex->height; y++)
	{
		const u8 *src_row = src + (y < h ? y : h - 1) * w;
		int dest_y = tex->height - 1 - y;
		u8 *row_base = dest + (dest_y & ~7) * tw;
		const u8 *m = morton_table + (dest_y & 7) * 8;
		for (int bx = 0; bx < tw; bx += 8)
		{
			u8 *tile = row_base + bx * 8;
			for (int k = 0; k < 8; k++) {
				int sx = bx + k < w ? bx + k : w - 1;
				tile[m[k]] = src_row[sx];
			}
		}
	}
}

void texture_tile_sw(C3D_Tex *tex, const void *data, int w, int h)
{
	const u32* src = reinterpret_cast<const u32*>(data);
	u32* dest = reinterpret_cast<u32*>(tex->data);
	if (w <= 0 || h <= 0)
		return;
	ensure_morton_table();
	// The image goes in the top-left corner of the (power-of-two) texture. The padding repeats
	// the last column/row instead of keeping whatever was in memory: linear filtering and
	// clamping at the image's edges then look as they do on the PC.
	// Row by row, 8 pixels (one tile row) at a time with the Morton offsets from the table: the
	// same layout as get_morton_offset() per pixel, several times faster (this runs for every
	// texture upload, e.g. each step of an ImageTexture.create_from_blend crossfade).
	const int tw = tex->width;
	for (int y = 0; y < tex->height; y++)
	{
		const u32 *src_row = src + (y < h ? y : h - 1) * w;
		int dest_y = tex->height - 1 - y;
		u32 *row_base = dest + (dest_y & ~7) * tw;
		const u8 *m = morton_table + (dest_y & 7) * 8;
		for (int bx = 0; bx < tw; bx += 8)
		{
			u32 *tile = row_base + bx * 8;
			if (bx + 8 <= w) {
				const u32 *s = src_row + bx;
				tile[m[0]] = __builtin_bswap32(s[0]);
				tile[m[1]] = __builtin_bswap32(s[1]);
				tile[m[2]] = __builtin_bswap32(s[2]);
				tile[m[3]] = __builtin_bswap32(s[3]);
				tile[m[4]] = __builtin_bswap32(s[4]);
				tile[m[5]] = __builtin_bswap32(s[5]);
				tile[m[6]] = __builtin_bswap32(s[6]);
				tile[m[7]] = __builtin_bswap32(s[7]);
			} else {
				for (int k = 0; k < 8; k++) {
					int sx = bx + k < w ? bx + k : w - 1;
					tile[m[k]] = __builtin_bswap32(src_row[sx]);
				}
			}
		}
	}

	C3D_TexFlush(tex);
}

// Minimum texture dimension seems to be 64 pixels
void texture_tile_hw(C3D_Tex *tex, const void *data, int w, int h)
{
	const u32 flags = (GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
		GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	GSPGPU_FlushDataCache(data, w*h*4);

	GX_DisplayTransfer(
		(u32*)data,
		GX_BUFFER_DIM(w, h),
		(u32*)tex->data,
		GX_BUFFER_DIM(tex->width, tex->height),
		flags
	);

	gspWaitForPPF();
	C3D_TexFlush(tex);
}

#endif
