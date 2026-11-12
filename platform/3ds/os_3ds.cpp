/*************************************************************************/
/*  os_3ds.cpp                                                           */
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
#include "servers/visual/visual_server_raster.h"
// #include "servers/visual/rasterizer_dummy.h"
#include "drivers/3ds/citro3d/rasterizer_citro3d.h"
#include "os_3ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "print_string.h"
#include "servers/physics/physics_server_sw.h"
#include "errno.h"

#include "drivers/unix/memory_pool_static_malloc.h"
#include "os/memory_pool_dynamic_static.h"
// #include "thread_posix.h"
// #include "semaphore_posix.h"
// #include "mutex_posix.h"
#include "drivers/3ds/thread_3ds.h"

//#include "core/io/file_access_buffered_fa.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/dir_access_unix.h"
// #include "tcp_server_posix.h"
// #include "stream_peer_tcp_posix.h"
// #include "packet_peer_udp_posix.h"

#include "main/main.h"
#include "globals.h"
#include "platform_3ds.h"
#include "local_wireless_3ds.h"
#include "network_3ds.h"

extern "C" {
#include <3ds/svc.h>
#include <3ds/gfx.h>
#include <3ds/console.h>
#include <3ds/services/apt.h>
#include <3ds/services/hid.h>
// C-Stick / ZL / ZR on New 3DS: hidInit() starts IRRST there and hidScanInput() merges it.
#include <3ds/services/irrst.h>
// Big stack thanks to CANVAS_ITEM_Z_MAX among other things
u32 __stacksize__ = 1024 * 128;

}

// ---------------------------------------------------------------------------------------------
// Memory split between the malloc heap and the linear heap (GPU textures/buffers, audio).
//
// libctru's default gives linear memory up to 32 MB, which Godot doesn't need. Setting
// __ctru_linear_heap_size alone is not enough: libctru then hands *all* the remaining memory to
// the heap, but the heap must fit in the 96 MB heap area (0x08000000-0x0E000000). On a New 3DS
// (~124 MB for the app) that request fails and libctru calls svcBreak before main().
//
// So we replace libctru's weak __system_allocateHeaps (same steps as libctru 2.7, different
// sizes): linear gets LINEAR_HEAP_MB, the heap gets the rest capped to the heap area, and any
// memory left over goes back to the linear heap. Set with scons linear_heap_mb=N.
// ---------------------------------------------------------------------------------------------
#ifndef LINEAR_HEAP_MB
#define LINEAR_HEAP_MB 12
#endif

extern "C" {
#include <3ds/os.h>
#include <3ds/allocator/mappable.h>

extern u32 __ctru_heap, __ctru_heap_size, __ctru_linear_heap, __ctru_linear_heap_size;
extern char *fake_heap_start, *fake_heap_end;

void __system_allocateHeaps(void)
{
	Handle reslimit = 0;
	if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE)))
		svcBreak(USERBREAK_PANIC);

	s64 max_commit = 0, current_commit = 0;
	ResourceLimitType type = RESLIMIT_COMMIT;
	svcGetResourceLimitLimitValues(&max_commit, reslimit, &type, 1);
	svcGetResourceLimitCurrentValues(&current_commit, reslimit, &type, 1);
	svcCloseHandle(reslimit);

	u32 remaining = (u32)(max_commit - current_commit) & ~0xFFF;
	u32 linear = (LINEAR_HEAP_MB * 1024 * 1024) & ~0xFFF;
	if (linear >= remaining)
		svcBreak(USERBREAK_PANIC);

	const u32 heap_area = OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN;
	u32 heap = remaining - linear;
	if (heap > heap_area) {
		linear += heap - heap_area;
		heap = heap_area;
	}

	__ctru_heap_size = heap;
	__ctru_linear_heap_size = linear;

	if (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, __ctru_heap_size,
			MEMOP_ALLOC, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
		svcBreak(USERBREAK_PANIC);

	if (R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size,
			MEMOP_ALLOC_LINEAR, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
		svcBreak(USERBREAK_PANIC);

	mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

	fake_heap_start = (char *)__ctru_heap;
	fake_heap_end = fake_heap_start + __ctru_heap_size;
}
}

static aptHookCookie apt_hook_cookie;

// APT hooks may run outside the main loop (e.g. on the APT event thread), so they only record
// what happened; OS_3DS::run() turns that into MainLoop notifications on the main thread.
static volatile bool apt_lost_focus = false;
static volatile bool apt_got_focus = false;
static volatile bool apt_exiting = false;

static void apt_hook_callback(APT_HookType hook, void* param)
{
	switch (hook) {
		case APTHOOK_ONSUSPEND: // HOME menu / another applet opened
		case APTHOOK_ONSLEEP:   // lid closed
			apt_lost_focus = true;
			break;
		case APTHOOK_ONRESTORE:
		case APTHOOK_ONWAKEUP:
			apt_got_focus = true;
			break;
		case APTHOOK_ONEXIT:    // closed from the HOME menu (or power button)
			apt_exiting = true;
			break;
		default:
			break;
	}
}


OS_3DS::OS_3DS()
: video_mode(400, 240, true, false, false) // top screen, native resolution
{	
	platform_3ds = NULL;
	local_wireless = NULL;
	touch_pressed = false;
	dual_screen = false;
	mouse_button_state = 0;
	data_dir_created = false;
	gfxInitDefault();
	// Send stderr to svcOutputDebugString (shown in the Azahar/Citra log and by debuggers);
	// stdout is redirected to stderr in vprint() below.
	consoleDebugInit(debugDevice_SVC);
	//consoleInit(GFX_BOTTOM, NULL);
	gfxSetDoubleBuffering(GFX_TOP, true);
	aptHook(&apt_hook_cookie, apt_hook_callback, this);
	osSetSpeedupEnable(true);
	APT_SetAppCpuTimeLimit(30); // Needed for QR Scanner to work.
// 	set_low_processor_usage_mode(true);
	_render_thread_mode=RENDER_THREAD_UNSAFE;
	AudioDriverManagerSW::add_driver(&audio_driver);
	use_vsync = true;
	last_id = 1;
}

OS_3DS::~OS_3DS()
{
	gfxExit();
}

void OS_3DS::run()
{
	if (!main_loop)
		return;
	
	main_loop->init();
	unsigned int frame = 0;
	bool closed_by_system = false;
	while (true)
	{
		if (!aptMainLoop()) {
			closed_by_system = true;
			break;
		}

		// Sample memory peaks about once per second (mallinfo walks the heap).
		if ((frame++ % 60) == 0)
			platform_3ds->update_peaks();

		processInput();
		_dispatch_apt_events();

		if (Main::iteration()==true)
			break;
		
		//printf("fps:%f\n", get_frames_per_second());
	}
	
	// Closed through the HOME menu / power button: give the game a chance to save.
	if (closed_by_system || apt_exiting)
		main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);

	main_loop->finish();
}

void OS_3DS::_dispatch_apt_events()
{
	if (!main_loop)
		return;
	// Both can be pending after a suspend: aptMainLoop() blocks while the HOME menu is open.
	if (apt_lost_focus) {
		apt_lost_focus = false;
		main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
	}
	if (apt_got_focus) {
		apt_got_focus = false;
		main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
	}
}

String OS_3DS::get_data_dir() const
{
	// user:// lives on the SD card, next to the homebrew: sdmc:/3ds/<application name>
	String name = get_safe_application_name().strip_edges();
	if (name == "")
		name = "godot";
	String dir = "sdmc:/3ds/" + name;
	if (!data_dir_created) {
		mkdir("sdmc:/3ds", 0777);
		mkdir(dir.utf8().get_data(), 0777);
		data_dir_created = true;
	}
	return dir;
}

static MemoryPoolStaticMalloc *mempool_static=NULL;
static MemoryPoolDynamicStatic *mempool_dynamic=NULL;

void OS_3DS::initialize_core()
{
	Thread3ds::make_default();
	Semaphore3ds::make_default();
	Mutex3ds::make_default();
	
	mempool_static = new MemoryPoolStaticMalloc;
	mempool_dynamic = memnew( MemoryPoolDynamicStatic );
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	//FileAccessBufferedFA<FileAccessUnix>::make_default();
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

#ifndef NO_NETWORK
	// TCP, UDP, TCP server and hostname resolution over Wi-Fi (SOC started on first use).
	Network3DS::make_default();
#endif

	ticks_start=svcGetSystemTick();
}

void OS_3DS::initialize(const VideoMode& p_desired,int p_video_driver,int p_audio_drive)
{
	main_loop=NULL;
	
	// Both screens (project setting display/3ds_dual_screen): the game area becomes 400x480.
	dual_screen = Globals::get_singleton()->has("display/3ds_dual_screen") && bool(Globals::get_singleton()->get("display/3ds_dual_screen"));
	if (dual_screen) {
		video_mode.height = 480;
		gfxSetDoubleBuffering(GFX_BOTTOM, true);
	}

	RasterizerCitro3d *rasterizer_citro3d = memnew( RasterizerCitro3d );
	rasterizer_citro3d->set_dual_screen(dual_screen);
	rasterizer = rasterizer_citro3d;
	
	visual_server = memnew( VisualServerRaster(rasterizer) );
	
	if (get_render_thread_mode()!=RENDER_THREAD_UNSAFE) {
		visual_server =memnew(VisualServerWrapMT(visual_server,get_render_thread_mode()==RENDER_SEPARATE_THREAD));
	}
	
	AudioDriverManagerSW::get_driver(0)->set_singleton();
	if (AudioDriverManagerSW::get_driver(0)->init()!=OK)
	{
		ERR_PRINT("Initializing NDSP audio failed; continuing without sound.");
		audio_driver_dummy.set_singleton();
		audio_driver_dummy.init();
	}

	sample_manager = memnew( SampleManagerMallocSW );
	audio_server = memnew( AudioServerSW(sample_manager) );
	audio_server->init();
	spatial_sound_server = memnew( SpatialSoundServerSW );
	spatial_sound_server->init();
	spatial_sound_2d_server = memnew( SpatialSound2DServerSW );
	spatial_sound_2d_server->init();

	ERR_FAIL_COND(!visual_server);
	
	visual_server->init();
	
	physics_server = memnew( PhysicsServerSW );
	physics_server->init();
// 	physics_2d_server = memnew( Physics2DServerSW );
	physics_2d_server = Physics2DServerWrapMT::init_server<Physics2DServerSW>();
	physics_2d_server->init();

	input = memnew( InputDefault );
	input->joy_connection_changed(0, true, "Nintendo 3DS");

	platform_3ds = memnew( Platform3DS );
	Globals::get_singleton()->add_singleton(Globals::Singleton("Platform3DS", platform_3ds));

	local_wireless = memnew( LocalWireless );
	Globals::get_singleton()->add_singleton(Globals::Singleton("LocalWireless", local_wireless));
}

void OS_3DS::delete_main_loop()
{
	if (main_loop)
		memdelete(main_loop);
	main_loop=NULL;
}

void OS_3DS::set_main_loop( MainLoop * p_main_loop )
{
	main_loop=p_main_loop;
	input->set_main_loop(p_main_loop);
}

void OS_3DS::finalize()
{
	if(main_loop)
		memdelete(main_loop);
	main_loop=NULL;

	spatial_sound_server->finish();
	memdelete(spatial_sound_server);
	spatial_sound_2d_server->finish();
	memdelete(spatial_sound_2d_server);

	memdelete(input);
	
	memdelete(sample_manager);

	audio_server->finish();
	memdelete(audio_server);

	visual_server->finish();
	memdelete(visual_server);
	memdelete(rasterizer);

	physics_server->finish();
	memdelete(physics_server);

	physics_2d_server->finish();
	memdelete(physics_2d_server);

	memdelete(local_wireless);
	local_wireless = NULL;
	Network3DS::finish();
	memdelete(platform_3ds);
	platform_3ds = NULL;
}

void OS_3DS::finalize_core()
{
	if (mempool_dynamic)
		memdelete( mempool_dynamic );
	delete mempool_static;
}

void OS_3DS::vprint(const char* p_format, va_list p_list,bool p_stder)
{
	// Without a console, libctru discards stdout; stderr goes to the debug device.
	vfprintf(stderr,p_format,p_list);
	fflush(stderr);
}

int OS_3DS::get_static_memory_usage() const
{
	return platform_3ds ? platform_3ds->get_heap_used() : 0;
}

int OS_3DS::get_static_memory_peak_usage() const
{
	return platform_3ds ? platform_3ds->get_heap_peak() : 0;
}

int OS_3DS::get_free_static_memory() const
{
	return platform_3ds ? platform_3ds->get_heap_size() - platform_3ds->get_heap_used() : 0;
}

void OS_3DS::alert(const String& p_alert,const String& p_title)
{
	fprintf(stderr,"ERROR: %s\n",p_alert.utf8().get_data());
}

Error OS_3DS::set_cwd(const String& p_cwd)
{
	printf("set cwd: %s", p_cwd.utf8().get_data());
	if (chdir(p_cwd.utf8().get_data())!=0)
		return ERR_CANT_OPEN;

	return OK;
}

OS::Date OS_3DS::get_date(bool utc) const
{

	time_t t=time(NULL);
	struct tm *lt;
	if (utc)
		lt=gmtime(&t);
	else
		lt=localtime(&t);
	Date ret;
	ret.year=1900+lt->tm_year;
	// Index starting at 1 to match OS_Unix::get_date
	//   and Windows SYSTEMTIME and tm_mon follows the typical structure 
	//   of 0-11, noted here: http://www.cplusplus.com/reference/ctime/tm/
	ret.month=(Month)(lt->tm_mon + 1);
	ret.day=lt->tm_mday;
	ret.weekday=(Weekday)lt->tm_wday;
	ret.dst=lt->tm_isdst;
	
	return ret;
}

OS::Time OS_3DS::get_time(bool utc) const
{
	time_t t=time(NULL);
	struct tm *lt;
	if (utc)
		lt=gmtime(&t);
	else
		lt=localtime(&t);
	Time ret;
	ret.hour=lt->tm_hour;
	ret.min=lt->tm_min;
	ret.sec=lt->tm_sec;
	get_time_zone_info();
	return ret;
}

OS::TimeZoneInfo OS_3DS::get_time_zone_info() const
{
	time_t t = time(NULL);
	struct tm *lt = localtime(&t);
	char name[16];
	strftime(name, 16, "%Z", lt);
	name[15] = 0;
	TimeZoneInfo ret;
	ret.name = name;

	char bias_buf[16];
	strftime(bias_buf, 16, "%z", lt);
	int bias;
	bias_buf[15] = 0;
	sscanf(bias_buf, "%d", &bias);

	// convert from ISO 8601 (1 minute=1, 1 hour=100) to minutes
	int hour = (int)bias / 100;
	int minutes = bias % 100;
	if (bias < 0)
		ret.bias = hour * 60 - minutes;
	else
		ret.bias = hour * 60 + minutes;

	return ret;
}

void OS_3DS::delay_usec(uint32_t p_usec) const
{
// 	printf("delay_usec: %lu\n", p_usec);
	svcSleepThread(1000ULL * p_usec);
}

#define TICKS_PER_SEC 268123480ULL
#define TICKS_PER_USEC 268

uint64_t OS_3DS::get_ticks_usec() const
{
	return (svcGetSystemTick() - ticks_start) / TICKS_PER_USEC;
}

uint64_t OS_3DS::get_unix_time() const
{
	return time(NULL);
}

uint64_t OS_3DS::get_system_time_secs() const
{
	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);
	//localtime(&tv_now.tv_usec);
	//localtime((const long *)&tv_now.tv_usec);
	return uint64_t(tv_now.tv_sec);
}

Point2 OS_3DS::_touch_to_canvas(int p_x, int p_y) const
{
	if (dual_screen)
		return Point2(p_x + (400 - 320) / 2, p_y + 240);
	return Point2(p_x, p_y);
}

void OS_3DS::_send_mouse_motion(const Point2 &p_pos, int p_button_mask)
{
	InputEvent ev;
	ev.type = InputEvent::MOUSE_MOTION;
	ev.ID = ++last_id;
	ev.device = 0;
	ev.mouse_motion.button_mask = p_button_mask;
	ev.mouse_motion.x = p_pos.x;
	ev.mouse_motion.y = p_pos.y;
	ev.mouse_motion.global_x = p_pos.x;
	ev.mouse_motion.global_y = p_pos.y;
	input->set_mouse_pos(p_pos);
	ev.mouse_motion.speed_x = input->get_mouse_speed().x;
	ev.mouse_motion.speed_y = input->get_mouse_speed().y;
	ev.mouse_motion.relative_x = p_pos.x - last_mouse_pos.x;
	ev.mouse_motion.relative_y = p_pos.y - last_mouse_pos.y;
	last_mouse_pos = p_pos;
	input->parse_input_event(ev);
}

void OS_3DS::_send_mouse_button(const Point2 &p_pos, bool p_pressed)
{
	mouse_button_state = p_pressed ? BUTTON_MASK_LEFT : 0;

	InputEvent ev;
	ev.type = InputEvent::MOUSE_BUTTON;
	ev.ID = ++last_id;
	ev.device = 0;
	ev.mouse_button.button_mask = mouse_button_state;
	ev.mouse_button.x = p_pos.x;
	ev.mouse_button.y = p_pos.y;
	ev.mouse_button.global_x = p_pos.x;
	ev.mouse_button.global_y = p_pos.y;
	ev.mouse_button.button_index = BUTTON_LEFT;
	ev.mouse_button.pressed = p_pressed;
	ev.mouse_button.doubleclick = false;
	ev.mouse_button.factor = 1.0;
	last_mouse_pos = p_pos;
	input->parse_input_event(ev);
}

void OS_3DS::swap_buffers()
{
 	//gfxFlushBuffers();
	gfxSwapBuffersGpu();
	if (use_vsync)
		gspWaitForVBlank();
}

int OS_3DS::get_processor_count() const
{
	return 1;
}

// Godot joystick button index -> 3DS key. Indices follow Godot's layout (JOY_BUTTON_0 = bottom
// face button, 12-15 = D-pad), so the default ui_* actions work out of the box.
static const u32 buttons[16] = {
	KEY_B,      // 0  JOY_BUTTON_0 (bottom)
	KEY_A,      // 1  JOY_BUTTON_1 (right)
	KEY_Y,      // 2  JOY_BUTTON_2 (left)
	KEY_X,      // 3  JOY_BUTTON_3 (top)
	KEY_L,      // 4  L
	KEY_R,      // 5  R
	KEY_ZL,     // 6  L2 (New 3DS)
	KEY_ZR,     // 7  R2 (New 3DS)
	0,          // 8  L3: no stick click on 3DS
	0,          // 9  R3: no stick click on 3DS
	KEY_SELECT, // 10 SELECT
	KEY_START,  // 11 START
	KEY_DUP,    // 12 D-pad up
	KEY_DDOWN,  // 13 D-pad down
	KEY_DLEFT,  // 14 D-pad left
	KEY_DRIGHT, // 15 D-pad right
};

// Circle Pad / C-Stick raw range is about +-156; ignore small noise around the center.
#define STICK_RANGE 156.0f
#define STICK_DEADZONE 15

static float _stick_value(s16 raw)
{
	if (raw > -STICK_DEADZONE && raw < STICK_DEADZONE)
		return 0.0f;
	float v = raw / STICK_RANGE;
	return v > 1.0f ? 1.0f : (v < -1.0f ? -1.0f : v);
}

void OS_3DS::_send_axis(int p_axis, float p_value)
{
	InputDefault::JoyAxis jx;
	jx.min = -1;
	jx.value = p_value;
	last_id = input->joy_axis(last_id, 0, p_axis, jx);
}

void OS_3DS::processInput()
{
	hidScanInput();
	u32 kDown = hidKeysDown();
	u32 kUp = hidKeysUp();
	u32 kHeld = hidKeysHeld();
	last_id++;

	for (int i = 0; i < 16; ++i)
	{
		if (!buttons[i])
			continue;
		if (buttons[i] & kDown)
			last_id = input->joy_button(last_id, 0, i, true);
		else if (buttons[i] & kUp)
			last_id = input->joy_button(last_id, 0, i, false);
	}

	// Analog sticks: Y is up-positive on 3DS, down-positive in Godot.
	circlePosition circle;
	hidCircleRead(&circle);
	_send_axis(JOY_ANALOG_0_X, _stick_value(circle.dx));
	_send_axis(JOY_ANALOG_0_Y, 0.0f - _stick_value(circle.dy));

	circlePosition cstick;
	hidCstickRead(&cstick); // New 3DS only; reads 0 on Old 3DS
	_send_axis(JOY_ANALOG_1_X, _stick_value(cstick.dx));
	_send_axis(JOY_ANALOG_1_Y, 0.0f - _stick_value(cstick.dy));

	// Touch screen (bottom screen, 320x240 pixels) as touch index 0. In dual screen mode the
	// bottom screen shows x 40-360, y 240-480 of the canvas: the touch is moved there and also
	// sent as the left mouse button, so Buttons and other Controls work by touch.
	if (kDown & KEY_TOUCH)
	{
		touchPosition t;
		hidTouchRead(&t);
		touch_pos = _touch_to_canvas(t.px, t.py);
		touch_pressed = true;

		InputEvent ev;
		ev.type = InputEvent::SCREEN_TOUCH;
		ev.ID = ++last_id;
		ev.screen_touch.index = 0;
		ev.screen_touch.pressed = true;
		ev.screen_touch.x = touch_pos.x;
		ev.screen_touch.y = touch_pos.y;
		input->parse_input_event(ev);

		if (dual_screen) {
			_send_mouse_motion(touch_pos, 0);
			_send_mouse_button(touch_pos, true);
		}
	}
	else if ((kHeld & KEY_TOUCH) && touch_pressed)
	{
		touchPosition t;
		hidTouchRead(&t);
		Point2 pos = _touch_to_canvas(t.px, t.py);
		if (pos != touch_pos)
		{
			InputEvent ev;
			ev.type = InputEvent::SCREEN_DRAG;
			ev.ID = ++last_id;
			ev.screen_drag.index = 0;
			ev.screen_drag.x = pos.x;
			ev.screen_drag.y = pos.y;
			ev.screen_drag.relative_x = pos.x - touch_pos.x;
			ev.screen_drag.relative_y = pos.y - touch_pos.y;
			input->parse_input_event(ev);
			touch_pos = pos;

			if (dual_screen)
				_send_mouse_motion(pos, BUTTON_MASK_LEFT);
		}
	}
	else if ((kUp & KEY_TOUCH) && touch_pressed)
	{
		// hidTouchRead returns 0,0 once released, so report the last known position.
		touch_pressed = false;

		InputEvent ev;
		ev.type = InputEvent::SCREEN_TOUCH;
		ev.ID = ++last_id;
		ev.screen_touch.index = 0;
		ev.screen_touch.pressed = false;
		ev.screen_touch.x = touch_pos.x;
		ev.screen_touch.y = touch_pos.y;
		input->parse_input_event(ev);

		if (dual_screen)
			_send_mouse_button(touch_pos, false);
	}
}
