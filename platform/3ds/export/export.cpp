/*************************************************************************/
/*  export.cpp                                                           */
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
#include "export.h"
#include "editor/editor_import_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "globals.h"
#include "io/image_loader.h"
#include "os/dir_access.h"
#include "os/file_access.h"
#include "platform/3ds/logo.gen.h"
#include "scene/resources/texture.h"

/*
 * Nintendo 3DS exporter.
 *
 * Produces a ready-to-run .3dsx from an export template (the engine built with platform=3ds,
 * as a .3dsx) entirely inside the editor, without devkitPro:
 *
 *   template .3dsx (code)  +  SMDH (title/description/author/icons)  +  RomFS { data.pck }
 *
 * At runtime the 3DS port mounts the RomFS and loads romfs:/data.pck as the main pack.
 * The SMDH, RomFS and 3DSX layouts were validated byte for byte against devkitPro's
 * smdhtool/3dsxtool output (apart from the 24x24 icon downscale, which is smdhtool-specific).
 */

namespace {

void put_u16(Vector<uint8_t> &v, uint16_t x) {
	v.push_back(x & 0xFF);
	v.push_back(x >> 8);
}

void put_u32(Vector<uint8_t> &v, uint32_t x) {
	for (int i = 0; i < 4; i++)
		v.push_back((x >> (8 * i)) & 0xFF);
}

void put_u64(Vector<uint8_t> &v, uint64_t x) {
	for (int i = 0; i < 8; i++)
		v.push_back((x >> (8 * i)) & 0xFF);
}

void set_u32(Vector<uint8_t> &v, int p_ofs, uint32_t x) {
	for (int i = 0; i < 4; i++)
		v[p_ofs + i] = (x >> (8 * i)) & 0xFF;
}

uint32_t get_u32(const Vector<uint8_t> &v, int p_ofs) {
	return v[p_ofs] | (v[p_ofs + 1] << 8) | (v[p_ofs + 2] << 16) | ((uint32_t)v[p_ofs + 3] << 24);
}

uint16_t get_u16(const Vector<uint8_t> &v, int p_ofs) {
	return v[p_ofs] | (v[p_ofs + 1] << 8);
}

void pad_to(Vector<uint8_t> &v, int p_align) {
	while (v.size() % p_align)
		v.push_back(0);
}

Vector<uint16_t> to_utf16(const String &p_str) {
	Vector<uint16_t> out;
	for (int i = 0; i < p_str.length(); i++) {
		uint32_t c = p_str[i];
		if (c >= 0x10000) {
			c -= 0x10000;
			out.push_back(0xD800 | (c >> 10));
			out.push_back(0xDC00 | (c & 0x3FF));
		} else {
			out.push_back(c);
		}
	}
	return out;
}

// Fixed-size UTF-16 field, always NUL-terminated.
void put_utf16_field(Vector<uint8_t> &v, const String &p_str, int p_chars) {
	Vector<uint16_t> s = to_utf16(p_str);
	for (int i = 0; i < p_chars; i++)
		put_u16(v, i < p_chars - 1 && i < s.size() ? s[i] : 0);
}

/* RomFS (level 3, as read by libctru's romfs_dev) holding a single file at the root. */

uint32_t romfs_hash(uint32_t p_parent, const Vector<uint16_t> &p_name) {
	uint32_t h = p_parent ^ 123456789;
	for (int i = 0; i < p_name.size(); i++) {
		h = (h >> 5) | (h << 27);
		h ^= p_name[i];
	}
	return h;
}

Vector<uint8_t> build_romfs(const String &p_file_name, const Vector<uint8_t> &p_data) {
	const uint32_t EMPTY = 0xFFFFFFFF;
	const uint32_t HASH_SLOTS = 3; // libctru/3dsxtool use 3 buckets for tables this small
	Vector<uint16_t> name = to_utf16(p_file_name);
	int name_bytes = name.size() * 2;
	int name_padded = (name_bytes + 3) & ~3;

	const uint32_t header_size = 40;
	const uint32_t dir_hash_off = header_size, dir_hash_size = HASH_SLOTS * 4;
	const uint32_t dir_table_off = dir_hash_off + dir_hash_size, dir_table_size = 24;
	const uint32_t file_hash_off = dir_table_off + dir_table_size, file_hash_size = HASH_SLOTS * 4;
	const uint32_t file_table_off = file_hash_off + file_hash_size, file_table_size = 32 + name_padded;
	const uint32_t data_off = (file_table_off + file_table_size + 7) & ~7;

	Vector<uint8_t> v;
	put_u32(v, header_size);
	put_u32(v, dir_hash_off);
	put_u32(v, dir_hash_size);
	put_u32(v, dir_table_off);
	put_u32(v, dir_table_size);
	put_u32(v, file_hash_off);
	put_u32(v, file_hash_size);
	put_u32(v, file_table_off);
	put_u32(v, file_table_size);
	put_u32(v, data_off);

	// Directory hash table: the root directory (table offset 0).
	uint32_t root_slot = romfs_hash(0, Vector<uint16_t>()) % HASH_SLOTS;
	for (uint32_t i = 0; i < HASH_SLOTS; i++)
		put_u32(v, i == root_slot ? 0 : EMPTY);

	// Root directory: parent, sibling, first child dir, first child file, next in hash, name length.
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u32(v, EMPTY);
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u32(v, 0);

	uint32_t file_slot = romfs_hash(0, name) % HASH_SLOTS;
	for (uint32_t i = 0; i < HASH_SLOTS; i++)
		put_u32(v, i == file_slot ? 0 : EMPTY);

	// File: parent, sibling, data offset, data size, next in hash, name length, name.
	put_u32(v, 0);
	put_u32(v, EMPTY);
	put_u64(v, 0);
	put_u64(v, p_data.size());
	put_u32(v, EMPTY);
	put_u32(v, name_bytes);
	for (int i = 0; i < name.size(); i++)
		put_u16(v, name[i]);
	pad_to(v, 4);

	while (v.size() < (int)data_off)
		v.push_back(0);

	int base = v.size();
	v.resize(base + p_data.size());
	copymem(&v[base], p_data.ptr(), p_data.size());
	pad_to(v, 4);
	return v;
}

/* SMDH: title/description/author for all 16 languages, default settings, 24x24 and 48x48 icons. */

// RGB565 in 8x8 tiles, pixels in Morton (Z) order, colors premultiplied by alpha (as smdhtool does).
// p_scale = 2 averages 2x2 blocks to build the small icon from the 48x48 one.
void put_icon(Vector<uint8_t> &v, const Image &p_icon48, int p_scale) {
	DVector<uint8_t> data = p_icon48.get_data();
	DVector<uint8_t>::Read r = data.read();
	int size = 48 / p_scale;
	for (int ty = 0; ty < size; ty += 8) {
		for (int tx = 0; tx < size; tx += 8) {
			for (int i = 0; i < 64; i++) {
				int x = tx + ((i & 1) | ((i >> 1) & 2) | ((i >> 2) & 4));
				int y = ty + (((i >> 1) & 1) | ((i >> 2) & 2) | ((i >> 3) & 4));
				int acc[3] = { 0, 0, 0 };
				for (int dy = 0; dy < p_scale; dy++) {
					for (int dx = 0; dx < p_scale; dx++) {
						const uint8_t *px = &r[((y * p_scale + dy) * 48 + (x * p_scale + dx)) * 4];
						for (int c = 0; c < 3; c++)
							acc[c] += px[c] * px[3] / 255;
					}
				}
				int n = p_scale * p_scale;
				int rr = (acc[0] + n / 2) / n, gg = (acc[1] + n / 2) / n, bb = (acc[2] + n / 2) / n;
				put_u16(v, ((rr >> 3) << 11) | ((gg >> 2) << 5) | (bb >> 3));
			}
		}
	}
}

Vector<uint8_t> build_smdh(const String &p_title, const String &p_desc, const String &p_author, const Image &p_icon48) {
	Vector<uint8_t> v;
	v.push_back('S');
	v.push_back('M');
	v.push_back('D');
	v.push_back('H');
	put_u16(v, 0); // version
	put_u16(v, 0);
	for (int lang = 0; lang < 16; lang++) {
		put_utf16_field(v, p_title, 0x40);
		put_utf16_field(v, p_desc, 0x80);
		put_utf16_field(v, p_author, 0x40);
	}
	// Settings, same defaults as smdhtool.
	static const uint8_t ratings[16] = { 0xa0, 0xa0, 0x00, 0xa0, 0xa0, 0x00, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0, 0, 0, 0, 0 };
	for (int i = 0; i < 16; i++)
		v.push_back(ratings[i]);
	put_u32(v, 0xFFFFFFFF); // region lock: all regions
	for (int i = 0; i < 12; i++)
		v.push_back(0); // match maker ids
	put_u32(v, 0x141); // flags: visible, allow 3D, record usage
	put_u16(v, 0); // EULA version
	put_u16(v, 0);
	put_u32(v, 0); // optimal animation frame (float 0.0)
	put_u32(v, 0); // StreetPass id
	for (int i = 0; i < 8; i++)
		v.push_back(0);
	put_icon(v, p_icon48, 2);
	put_icon(v, p_icon48, 1);
	return v;
}

/* 3DSX: keep the template's code/relocations, (re)write the extended header, append SMDH + RomFS. */

Error build_3dsx(const Vector<uint8_t> &p_template, const Vector<uint8_t> &p_smdh, const Vector<uint8_t> &p_romfs, Vector<uint8_t> &r_out) {
	const int BASE_HEADER = 0x20, EXT_HEADER = 0x2C;
	if (p_template.size() < BASE_HEADER || p_template[0] != '3' || p_template[1] != 'D' || p_template[2] != 'S' || p_template[3] != 'X')
		return ERR_FILE_CORRUPT;

	int header_size = get_u16(p_template, 4);
	int body_end = p_template.size();
	if (header_size >= EXT_HEADER) {
		// Template already has SMDH/RomFS: they always come after the code, drop them.
		uint32_t offsets[2] = { get_u32(p_template, 0x20), get_u32(p_template, 0x28) };
		for (int i = 0; i < 2; i++)
			if (offsets[i] && (int)offsets[i] < body_end)
				body_end = offsets[i];
	}
	if (header_size < BASE_HEADER || body_end < header_size)
		return ERR_FILE_CORRUPT;

	int body_size = body_end - header_size;
	r_out.resize(EXT_HEADER + body_size);
	copymem(&r_out[0], p_template.ptr(), BASE_HEADER);
	r_out[4] = EXT_HEADER & 0xFF;
	r_out[5] = EXT_HEADER >> 8;
	uint32_t smdh_off = EXT_HEADER + body_size;
	set_u32(r_out, 0x20, smdh_off);
	set_u32(r_out, 0x24, p_smdh.size());
	set_u32(r_out, 0x28, smdh_off + p_smdh.size());
	copymem(&r_out[EXT_HEADER], &p_template[header_size], body_size);

	int ofs = r_out.size();
	r_out.resize(ofs + p_smdh.size() + p_romfs.size());
	copymem(&r_out[ofs], p_smdh.ptr(), p_smdh.size());
	copymem(&r_out[ofs + p_smdh.size()], p_romfs.ptr(), p_romfs.size());
	return OK;
}

// Default makerom RSF (copy of misc/3ds-build/cia/app.rsf; keep both in sync).
const char *DEFAULT_RSF =
	"# makerom RSF for Godot 2.1 3DS games (CIA). Values in $(...) are passed with -DNAME=value.\n"
	"# Based on the common homebrew application template.\n"
	"\n"
	"BasicInfo:\n"
	"  Title                   : \"$(APP_TITLE)\"\n"
	"  ProductCode             : \"$(APP_PRODUCT_CODE)\"\n"
	"  Logo                    : Homebrew\n"
	"\n"
	"RomFs:\n"
	"  RootPath                : \"$(APP_ROMFS)\"\n"
	"\n"
	"TitleInfo:\n"
	"  Category                : Application\n"
	"  UniqueId                : $(APP_UNIQUE_ID)\n"
	"\n"
	"Option:\n"
	"  UseOnSD                 : true\n"
	"  FreeProductCode         : true\n"
	"  MediaFootPadding        : false\n"
	"  EnableCrypt             : false\n"
	"  EnableCompress          : true\n"
	"\n"
	"AccessControlInfo:\n"
	"  CoreVersion             : 2\n"
	"\n"
	"  # Exheader format version\n"
	"  DescVersion             : 2\n"
	"\n"
	"  # Minimum kernel version required\n"
	"  ReleaseKernelMajor      : \"02\"\n"
	"  ReleaseKernelMinor      : \"33\"\n"
	"\n"
	"  # Old 3DS: 64 MB for the app. New 3DS: 124 MB, 804 MHz, L2 cache, core 2 available.\n"
	"  MemoryType              : Application\n"
	"  SystemMode              : 64MB\n"
	"  SystemModeExt           : 124MB\n"
	"  CpuSpeed                : 804MHz\n"
	"  EnableL2Cache           : true\n"
	"  CanAccessCore2          : true\n"
	"\n"
	"  IdealProcessor          : 0\n"
	"  AffinityMask            : 1\n"
	"  Priority                : 16\n"
	"  MaxCpu                  : 0x9E\n"
	"  HandleTableSize         : 0x200\n"
	"\n"
	"  DisableDebug            : false\n"
	"  EnableForceDebug        : false\n"
	"  CanWriteSharedPage      : true\n"
	"  CanUsePrivilegedPriority: false\n"
	"  CanUseNonAlphabetAndNumber: true\n"
	"  PermitMainFunctionArgument: true\n"
	"  CanShareDeviceMemory    : true\n"
	"  RunnableOnSleep         : false\n"
	"  SpecialMemoryArrange    : true\n"
	"  UseExtSaveData          : false\n"
	"\n"
	"  # Physical memory the app maps directly. Without the DSP range, ndspInit() crashes writing to\n"
	"  # DSP shared memory (0x1FF5xxxx). As a .3dsx these came from the process the launcher took over.\n"
	"  IORegisterMapping:\n"
	"   - 1ff00000-1ff7ffff   # DSP memory\n"
	"  MemoryMapping:\n"
	"   - 1f000000-1f5fffff:r # VRAM (end address is inclusive)\n"
	"\n"
	"  FileSystemAccess:\n"
	"   - Debug\n"
	"   - DirectSdmc\n"
	"   - DirectSdmcWrite\n"
	"\n"
	"  IoAccessControl:\n"
	"   - FsMountNand\n"
	"   - FsMountNandRoWrite\n"
	"   - FsMountTwln\n"
	"   - FsMountWnand\n"
	"   - FsMountCardSpi\n"
	"   - UseSdif3\n"
	"   - CreateSeed\n"
	"   - UseCardSpi\n"
	"\n"
	"  SystemCallAccess:\n"
	"    ControlMemory: 1\n"
	"    QueryMemory: 2\n"
	"    ExitProcess: 3\n"
	"    GetProcessAffinityMask: 4\n"
	"    SetProcessAffinityMask: 5\n"
	"    GetProcessIdealProcessor: 6\n"
	"    SetProcessIdealProcessor: 7\n"
	"    CreateThread: 8\n"
	"    ExitThread: 9\n"
	"    SleepThread: 10\n"
	"    GetThreadPriority: 11\n"
	"    SetThreadPriority: 12\n"
	"    GetThreadAffinityMask: 13\n"
	"    SetThreadAffinityMask: 14\n"
	"    GetThreadIdealProcessor: 15\n"
	"    SetThreadIdealProcessor: 16\n"
	"    GetCurrentProcessorNumber: 17\n"
	"    Run: 18\n"
	"    CreateMutex: 19\n"
	"    ReleaseMutex: 20\n"
	"    CreateSemaphore: 21\n"
	"    ReleaseSemaphore: 22\n"
	"    CreateEvent: 23\n"
	"    SignalEvent: 24\n"
	"    ClearEvent: 25\n"
	"    CreateTimer: 26\n"
	"    SetTimer: 27\n"
	"    CancelTimer: 28\n"
	"    ClearTimer: 29\n"
	"    CreateMemoryBlock: 30\n"
	"    MapMemoryBlock: 31\n"
	"    UnmapMemoryBlock: 32\n"
	"    CreateAddressArbiter: 33\n"
	"    ArbitrateAddress: 34\n"
	"    CloseHandle: 35\n"
	"    WaitSynchronization1: 36\n"
	"    WaitSynchronizationN: 37\n"
	"    SignalAndWait: 38\n"
	"    DuplicateHandle: 39\n"
	"    GetSystemTick: 40\n"
	"    GetHandleInfo: 41\n"
	"    GetSystemInfo: 42\n"
	"    GetProcessInfo: 43\n"
	"    GetThreadInfo: 44\n"
	"    ConnectToPort: 45\n"
	"    SendSyncRequest1: 46\n"
	"    SendSyncRequest2: 47\n"
	"    SendSyncRequest3: 48\n"
	"    SendSyncRequest4: 49\n"
	"    SendSyncRequest: 50\n"
	"    OpenProcess: 51\n"
	"    OpenThread: 52\n"
	"    GetProcessId: 53\n"
	"    GetProcessIdOfThread: 54\n"
	"    GetThreadId: 55\n"
	"    GetResourceLimit: 56\n"
	"    GetResourceLimitLimitValues: 57\n"
	"    GetResourceLimitCurrentValues: 58\n"
	"    GetThreadContext: 59\n"
	"    Break: 60\n"
	"    OutputDebugString: 61\n"
	"    ControlPerformanceCounter: 62\n"
	"    CreatePort: 71\n"
	"    CreateSessionToPort: 72\n"
	"    CreateSession: 73\n"
	"    AcceptSession: 74\n"
	"    ReplyAndReceive1: 75\n"
	"    ReplyAndReceive2: 76\n"
	"    ReplyAndReceive3: 77\n"
	"    ReplyAndReceive4: 78\n"
	"    ReplyAndReceive: 79\n"
	"    BindInterrupt: 80\n"
	"    UnbindInterrupt: 81\n"
	"    InvalidateProcessDataCache: 82\n"
	"    StoreProcessDataCache: 83\n"
	"    FlushProcessDataCache: 84\n"
	"    StartInterProcessDma: 85\n"
	"    StopDma: 86\n"
	"    GetDmaState: 87\n"
	"    RestartDma: 88\n"
	"    DebugActiveProcess: 96\n"
	"    BreakDebugProcess: 97\n"
	"    TerminateDebugProcess: 98\n"
	"    GetProcessDebugEvent: 99\n"
	"    ContinueDebugEvent: 100\n"
	"    GetProcessList: 101\n"
	"    GetThreadList: 102\n"
	"    GetDebugThreadContext: 103\n"
	"    SetDebugThreadContext: 104\n"
	"    QueryDebugProcessMemory: 105\n"
	"    ReadProcessMemory: 106\n"
	"    WriteProcessMemory: 107\n"
	"    SetHardwareBreakPoint: 108\n"
	"    GetDebugThreadParam: 109\n"
	"    ControlProcessMemory: 112\n"
	"    MapProcessMemory: 113\n"
	"    UnmapProcessMemory: 114\n"
	"    CreateCodeSet: 115\n"
	"    CreateProcess: 117\n"
	"    TerminateProcess: 118\n"
	"    SetProcessResourceLimits: 119\n"
	"    CreateResourceLimit: 120\n"
	"    SetResourceLimitValues: 121\n"
	"    AddCodeSegment: 122\n"
	"    Backdoor: 123\n"
	"    KernelSetState: 124\n"
	"    QueryProcessMemory: 125\n"
	"\n"
	"  InterruptNumbers:\n"
	"\n"
	"  ServiceAccessControl:\n"
	"   - APT:U\n"
	"   - $hioFIO\n"
	"   - $hostio0\n"
	"   - $hostio1\n"
	"   - ac:u\n"
	"   - boss:U\n"
	"   - cam:u\n"
	"   - cecd:u\n"
	"   - cfg:nor\n"
	"   - cfg:u\n"
	"   - csnd:SND\n"
	"   - dsp::DSP\n"
	"   - frd:u\n"
	"   - fs:USER\n"
	"   - gsp::Gpu\n"
	"   - gsp::Lcd\n"
	"   - hid:USER\n"
	"   - http:C\n"
	"   - ir:rst\n"
	"   - ir:u\n"
	"   - ir:USER\n"
	"   - mic:u\n"
	"   - ndm:u\n"
	"   - news:s\n"
	"   - nwm::EXT\n"
	"   - nwm::UDS\n"
	"   - ptm:sysm\n"
	"   - ptm:u\n"
	"   - pxi:dev\n"
	"   - soc:U\n"
	"   - ssl:C\n"
	"   - y2r:u\n"
	"\n"
	"SystemControlInfo:\n"
	"  SaveDataSize: 0KB\n"
	"  RemasterVersion: 0\n"
	"  StackSize: 0x40000\n"
	"  Dependency:\n"
	"    ac: 0x0004013000002402\n"
	"    act: 0x0004013000003802\n"
	"    am: 0x0004013000001502\n"
	"    boss: 0x0004013000003402\n"
	"    camera: 0x0004013000001602\n"
	"    cecd: 0x0004013000002602\n"
	"    cfg: 0x0004013000001702\n"
	"    codec: 0x0004013000001802\n"
	"    csnd: 0x0004013000002702\n"
	"    dlp: 0x0004013000002802\n"
	"    dsp: 0x0004013000001a02\n"
	"    friends: 0x0004013000003202\n"
	"    gpio: 0x0004013000001b02\n"
	"    gsp: 0x0004013000001c02\n"
	"    hid: 0x0004013000001d02\n"
	"    http: 0x0004013000002902\n"
	"    i2c: 0x0004013000001e02\n"
	"    ir: 0x0004013000003302\n"
	"    mcu: 0x0004013000001f02\n"
	"    mic: 0x0004013000002002\n"
	"    ndm: 0x0004013000002b02\n"
	"    news: 0x0004013000003502\n"
	"    nim: 0x0004013000002c02\n"
	"    nwm: 0x0004013000002d02\n"
	"    pdn: 0x0004013000002102\n"
	"    ps: 0x0004013000003102\n"
	"    ptm: 0x0004013000002202\n"
	"    ro: 0x0004013000003702\n"
	"    socket: 0x0004013000002e02\n"
	"    spi: 0x0004013000002302\n"
	"    ssl: 0x0004013000002f02\n";

// Short silent PCM WAV, used when the project has no banner jingle (bannertool requires one).
Vector<uint8_t> build_silent_wav() {
	const int rate = 22050, samples = rate / 2;
	Vector<uint8_t> v;
	const char *riff = "RIFF", *wave = "WAVEfmt ", *data = "data";
	for (int i = 0; i < 4; i++)
		v.push_back(riff[i]);
	put_u32(v, 36 + samples * 2);
	for (int i = 0; i < 8; i++)
		v.push_back(wave[i]);
	put_u32(v, 16);
	put_u16(v, 1); // PCM
	put_u16(v, 1); // mono
	put_u32(v, rate);
	put_u32(v, rate * 2);
	put_u16(v, 2);
	put_u16(v, 16);
	for (int i = 0; i < 4; i++)
		v.push_back(data[i]);
	put_u32(v, samples * 2);
	for (int i = 0; i < samples * 2; i++)
		v.push_back(0);
	return v;
}

Error write_file(const String &p_path, const Vector<uint8_t> &p_data) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
	if (!f)
		return ERR_FILE_CANT_WRITE;
	f->store_buffer(p_data.ptr(), p_data.size());
	memdelete(f);
	return OK;
}

// makerom's RSF values must be plain ASCII.
String ascii_only(const String &p_str, const String &p_fallback) {
	String out;
	for (int i = 0; i < p_str.length(); i++) {
		CharType c = p_str[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_')
			out += String::chr(c);
	}
	out = out.strip_edges();
	return out != "" ? out : p_fallback;
}

} // namespace

class EditorExportPlatform3DS : public EditorExportPlatform {

	OBJ_TYPE(EditorExportPlatform3DS, EditorExportPlatform);

	enum Format {
		FORMAT_3DSX,
		FORMAT_CIA,
	};

	int format;
	String custom_template;
	String custom_template_elf;
	String unique_id;
	String product_code;
	String banner_image;
	String banner_audio;
	String custom_rsf;
	String title;
	String description;
	String author;
	String icon;
	Ref<ImageTexture> logo;

	static const char *TEMPLATE_NAME;
	static const char *TEMPLATE_NAME_ELF;

	String _get_template_path(String *r_error) const {
		bool cia = format == FORMAT_CIA;
		String custom = cia ? custom_template_elf : custom_template;
		if (custom != "") {
			if (FileAccess::exists(custom))
				return custom;
			if (r_error)
				*r_error = "Custom template not found:\n" + custom;
			return "";
		}
		return find_export_template(cia ? TEMPLATE_NAME_ELF : TEMPLATE_NAME, r_error);
	}

	// Unique ID in the homebrew range (0x00300-0xF7FFF). Derived from the project name unless set,
	// so different games don't overwrite each other once installed.
	uint32_t _get_unique_id() const {
		String id = unique_id.strip_edges();
		if (id != "") {
			if (id.begins_with("0x") || id.begins_with("0X"))
				id = id.substr(2, id.length());
			uint32_t v = id.hex_to_int(false);
			if (v >= 0x300 && v <= 0xF7FFF)
				return v;
		}
		String name = Globals::get_singleton()->get("application/name");
		return 0x300 + (name.hash() % (0xF7FFF - 0x300));
	}

	String _tool_path(const String &p_setting, const String &p_default) const {
		String path = EditorSettings::get_singleton()->get(p_setting);
		return path != "" ? path : p_default;
	}

	// Success is judged by the expected output file: with a captured pipe, OS::execute gets the exit
	// status from pclose(), which can report -1 even when the tool succeeded.
	Error _run_tool(const String &p_tool, const List<String> &p_args, const String &p_what, const String &p_expected_output) {
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (FileAccess::exists(p_expected_output))
			da->remove(p_expected_output);
		memdelete(da);

		String output;
		int exit_code = -1;
		Error err = OS::get_singleton()->execute(p_tool, p_args, true, NULL, &output, &exit_code, true);
		if (err != OK || !FileAccess::exists(p_expected_output)) {
			String cmd = p_tool;
			for (const List<String>::Element *E = p_args.front(); E; E = E->next())
				cmd += " \"" + E->get() + "\"";
			print_line("3DS export: " + p_what + " failed: " + cmd + "\n" + output);
			EditorNode::add_io_error("Failed running " + p_what + " (" + p_tool + ").\n"
					"Set its path in Editor Settings > 3ds, or put it in the PATH.\n\n" + output);
			return err != OK ? err : FAILED;
		}
		return OK;
	}

	Error _make_default_banner_image(const Image &p_icon48, const String &p_path) const {
		Image banner(256, 128, false, Image::FORMAT_RGBA);
		banner.fill(Color(0.18, 0.33, 0.5));
		String path = icon;
		if (path == "")
			path = Globals::get_singleton()->get("application/icon");
		Image big;
		if (path == "" || ImageLoader::load_image(path, &big) != OK || big.empty())
			big = p_icon48;
		big.convert(Image::FORMAT_RGBA);
		big.resize(96, 96, Image::INTERPOLATE_CUBIC);
		// Alpha-blend the icon over the background (blit_rect would copy transparent pixels as-is).
		Color bg = banner.get_pixel(0, 0);
		for (int y = 0; y < 96; y++) {
			for (int x = 0; x < 96; x++) {
				Color c = big.get_pixel(x, y);
				banner.put_pixel(80 + x, 16 + y, Color(bg.r + (c.r - bg.r) * c.a, bg.g + (c.g - bg.g) * c.a, bg.b + (c.b - bg.b) * c.a, 1.0));
			}
		}
		return banner.save_png(p_path);
	}

	Error _export_cia(const String &p_path, const String &p_elf, const Vector<uint8_t> &p_pck, const Vector<uint8_t> &p_smdh, const Image &p_icon48, const String &p_title) {
		String tmp = EditorSettings::get_singleton()->get_settings_path() + "/tmp/3ds_cia";
		DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		dir->make_dir_recursive(tmp + "/romfs");
		memdelete(dir);

		if (write_file(tmp + "/romfs/data.pck", p_pck) != OK || write_file(tmp + "/icon.icn", p_smdh) != OK) {
			EditorNode::add_io_error("Couldn't write temporary files in:\n" + tmp);
			return ERR_FILE_CANT_WRITE;
		}

		String rsf = tmp + "/app.rsf";
		if (custom_rsf != "") {
			rsf = Globals::get_singleton()->globalize_path(custom_rsf);
		} else {
			FileAccess *f = FileAccess::open(rsf, FileAccess::WRITE);
			if (!f)
				return ERR_FILE_CANT_WRITE;
			f->store_string(DEFAULT_RSF);
			memdelete(f);
		}

		String image = banner_image != "" ? Globals::get_singleton()->globalize_path(banner_image) : tmp + "/banner.png";
		if (banner_image == "" && _make_default_banner_image(p_icon48, image) != OK) {
			EditorNode::add_io_error("Couldn't create the default banner image.");
			return ERR_FILE_CANT_WRITE;
		}
		String audio = banner_audio != "" ? Globals::get_singleton()->globalize_path(banner_audio) : tmp + "/banner.wav";
		if (banner_audio == "")
			write_file(audio, build_silent_wav());

		List<String> args;
		args.push_back("makebanner");
		args.push_back("-i");
		args.push_back(image);
		args.push_back("-a");
		args.push_back(audio);
		args.push_back("-o");
		args.push_back(tmp + "/banner.bnr");
		Error err = _run_tool(_tool_path("3ds/bannertool", "bannertool"), args, "bannertool", tmp + "/banner.bnr");
		if (err != OK)
			return err;

		String code = product_code.strip_edges().to_upper();
		if (code == "")
			code = "CTR-P-GDOT";

		args.clear();
		args.push_back("-f");
		args.push_back("cia");
		args.push_back("-o");
		args.push_back(p_path);
		args.push_back("-elf");
		args.push_back(p_elf);
		args.push_back("-rsf");
		args.push_back(rsf);
		args.push_back("-icon");
		args.push_back(tmp + "/icon.icn");
		args.push_back("-banner");
		args.push_back(tmp + "/banner.bnr");
		args.push_back("-DAPP_TITLE=" + ascii_only(p_title, "Godot Game"));
		args.push_back("-DAPP_PRODUCT_CODE=" + ascii_only(code, "CTR-P-GDOT"));
		args.push_back("-DAPP_UNIQUE_ID=0x" + String::num_int64(_get_unique_id(), 16));
		args.push_back("-DAPP_ROMFS=" + tmp + "/romfs");
		return _run_tool(_tool_path("3ds/makerom", "makerom"), args, "makerom", p_path);
	}

	Image _load_icon() const {
		String path = icon;
		if (path == "")
			path = Globals::get_singleton()->get("application/icon");
		Image img;
		if (path == "" || ImageLoader::load_image(path, &img) != OK || img.empty())
			img = Image(_3ds_logo);
		img.convert(Image::FORMAT_RGBA);
		img.resize(48, 48, Image::INTERPOLATE_CUBIC);
		return img;
	}

protected:
	bool _set(const StringName &p_name, const Variant &p_value) {
		String n = p_name;
		if (n == "binary/format")
			format = p_value;
		else if (n == "custom_template/release")
			custom_template = p_value;
		else if (n == "custom_template/release_elf")
			custom_template_elf = p_value;
		else if (n == "cia/unique_id")
			unique_id = p_value;
		else if (n == "cia/product_code")
			product_code = p_value;
		else if (n == "cia/banner_image")
			banner_image = p_value;
		else if (n == "cia/banner_audio")
			banner_audio = p_value;
		else if (n == "cia/custom_rsf")
			custom_rsf = p_value;
		else if (n == "application/title")
			title = p_value;
		else if (n == "application/description")
			description = p_value;
		else if (n == "application/author")
			author = p_value;
		else if (n == "application/icon")
			icon = p_value;
		else
			return false;
		return true;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		String n = p_name;
		if (n == "binary/format")
			r_ret = format;
		else if (n == "custom_template/release")
			r_ret = custom_template;
		else if (n == "custom_template/release_elf")
			r_ret = custom_template_elf;
		else if (n == "cia/unique_id")
			r_ret = unique_id;
		else if (n == "cia/product_code")
			r_ret = product_code;
		else if (n == "cia/banner_image")
			r_ret = banner_image;
		else if (n == "cia/banner_audio")
			r_ret = banner_audio;
		else if (n == "cia/custom_rsf")
			r_ret = custom_rsf;
		else if (n == "application/title")
			r_ret = title;
		else if (n == "application/description")
			r_ret = description;
		else if (n == "application/author")
			r_ret = author;
		else if (n == "application/icon")
			r_ret = icon;
		else
			return false;
		return true;
	}

	void _get_property_list(List<PropertyInfo> *p_list) const {
		// Enum hint items are comma-separated: no commas inside the labels.
		p_list->push_back(PropertyInfo(Variant::INT, "binary/format", PROPERTY_HINT_ENUM, "3dsx (Homebrew Launcher),cia (installable - needs makerom + bannertool)"));
		p_list->push_back(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "3dsx"));
		p_list->push_back(PropertyInfo(Variant::STRING, "custom_template/release_elf", PROPERTY_HINT_GLOBAL_FILE, "elf"));
		p_list->push_back(PropertyInfo(Variant::STRING, "cia/unique_id"));
		p_list->push_back(PropertyInfo(Variant::STRING, "cia/product_code"));
		p_list->push_back(PropertyInfo(Variant::STRING, "cia/banner_image", PROPERTY_HINT_FILE, "png"));
		p_list->push_back(PropertyInfo(Variant::STRING, "cia/banner_audio", PROPERTY_HINT_FILE, "wav"));
		p_list->push_back(PropertyInfo(Variant::STRING, "cia/custom_rsf", PROPERTY_HINT_FILE, "rsf"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/title"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/description"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/author"));
		p_list->push_back(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "png"));
	}

public:
	virtual String get_name() const { return "Nintendo 3DS"; }
	virtual ImageCompression get_image_compression() const { return IMAGE_COMPRESSION_NONE; }
	virtual Ref<Texture> get_logo() const { return logo; }
	virtual String get_binary_extension() const { return format == FORMAT_CIA ? "cia" : "3dsx"; }

	virtual bool can_export(String *r_error = NULL) const {
		String err;
		bool ok = _get_template_path(&err) != "";
		if (!ok && r_error)
			*r_error = err + "\nBuild it with misc/3ds-build/build.sh template (installs " + TEMPLATE_NAME + " and " + TEMPLATE_NAME_ELF + ").";
		return ok;
	}

	virtual Error export_project(const String &p_path, bool p_debug, int p_flags = 0) {
		EditorProgress ep("export", "Exporting for Nintendo 3DS", 5);

		ep.step("Reading template..", 0);
		String err;
		String template_path = _get_template_path(&err);
		if (template_path == "") {
			EditorNode::add_io_error(err);
			return ERR_FILE_NOT_FOUND;
		}
		Vector<uint8_t> tmpl;
		if (format != FORMAT_CIA)
			tmpl = FileAccess::get_file_as_array(template_path);
		if (format != FORMAT_CIA && tmpl.size() == 0) {
			EditorNode::add_io_error("Couldn't read export template:\n" + template_path);
			return ERR_FILE_CANT_READ;
		}

		ep.step("Packing project (data.pck)..", 1);
		String tmp_dir = EditorSettings::get_singleton()->get_settings_path() + "/tmp";
		String pck_path = tmp_dir + "/3ds_data.pck";
		{
			DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			dir->make_dir_recursive(tmp_dir);
			memdelete(dir);
		}
		FileAccess *pck = FileAccess::open(pck_path, FileAccess::WRITE);
		if (!pck) {
			EditorNode::add_io_error("Couldn't create temporary file:\n" + pck_path);
			return ERR_FILE_CANT_WRITE;
		}
		Error pack_err = save_pack(pck);
		memdelete(pck);
		if (pack_err != OK) {
			EditorNode::add_io_error("Couldn't pack the project.");
			return pack_err;
		}
		Vector<uint8_t> pck_data = FileAccess::get_file_as_array(pck_path);
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->remove(pck_path);
		memdelete(da);

		ep.step("Building RomFS and SMDH..", 2);
		String app_name = Globals::get_singleton()->get("application/name");
		String smdh_title = title != "" ? title : (app_name != "" ? app_name : String("Godot Game"));
		String smdh_desc = description != "" ? description : String("Made with Godot Engine");
		String smdh_author = author != "" ? author : String("Unknown");
		Image icon48 = _load_icon();
		Vector<uint8_t> smdh = build_smdh(smdh_title, smdh_desc, smdh_author, icon48);

		if (format == FORMAT_CIA) {
			ep.step("Building .cia (bannertool + makerom)..", 3);
			Error cia_err = _export_cia(p_path, template_path, pck_data, smdh, icon48, smdh_title);
			if (cia_err == OK)
				ep.step("Done.", 4);
			return cia_err;
		}

		Vector<uint8_t> romfs = build_romfs("data.pck", pck_data);

		ep.step("Writing .3dsx..", 3);
		Vector<uint8_t> out;
		Error build_err = build_3dsx(tmpl, smdh, romfs, out);
		if (build_err != OK) {
			EditorNode::add_io_error("Invalid export template (not a .3dsx):\n" + template_path);
			return build_err;
		}
		FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
		if (!f) {
			EditorNode::add_io_error("Couldn't write:\n" + p_path);
			return ERR_FILE_CANT_WRITE;
		}
		f->store_buffer(out.ptr(), out.size());
		memdelete(f);

		ep.step("Done.", 4);
		return OK;
	}

	EditorExportPlatform3DS() {
		format = FORMAT_3DSX;
		Image img(_3ds_logo);
		logo = Ref<ImageTexture>(memnew(ImageTexture));
		logo->create_from_image(img);
	}
};

const char *EditorExportPlatform3DS::TEMPLATE_NAME = "3ds_release.3dsx";
const char *EditorExportPlatform3DS::TEMPLATE_NAME_ELF = "3ds_release.elf";

void register_3ds_exporter() {

	// Tools used only for .cia exports (Project_CTR makerom and bannertool). Empty = look in PATH.
	String exe_ext = OS::get_singleton()->get_name() == "Windows" ? "exe" : "";
	EDITOR_DEF("3ds/makerom", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "3ds/makerom", PROPERTY_HINT_GLOBAL_FILE, exe_ext));
	EDITOR_DEF("3ds/bannertool", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "3ds/bannertool", PROPERTY_HINT_GLOBAL_FILE, exe_ext));

	Ref<EditorExportPlatform3DS> exporter = Ref<EditorExportPlatform3DS>(memnew(EditorExportPlatform3DS));
	EditorImportExport::get_singleton()->add_export_platform(exporter);
}
