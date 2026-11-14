// Minimal headless libretro frontend to test Nintendo 3DS homebrew (.3dsx) with the Azahar core.
//
//   lr_harness <core.so> <game.3dsx> <frames> <out_dir> [script]
//
// Runs the game for <frames> frames with the software renderer and writes screenshots
// (<out_dir>/frame_NNNNN.ppm) at the frames listed in the script. Script lines:
//   shot N               screenshot after frame N
//   press N BUTTON       hold BUTTON from frame N (A B X Y L R START SELECT UP DOWN LEFT RIGHT)
//   release N BUTTON     release it at frame N
//   timing               print emulated frame time stats at the end
// Options for the core: env LR_OLD3DS=1 emulates an Old 3DS.
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "libretro.h"

static const char *out_dir;
static const void *last_frame;
static unsigned last_w, last_h;
static size_t last_pitch;
static int pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;
static char sys_dir[512];

#define MAX_EVENTS 4096
static struct { int frame, type, button; } events[MAX_EVENTS]; // type 0 shot, 1 press, 2 release
static int event_count;
static int held[16];

static int button_id(const char *name) {
	static const char *names[] = { "B", "Y", "SELECT", "START", "UP", "DOWN", "LEFT", "RIGHT", "A", "X", "L", "R" };
	for (int i = 0; i < 12; i++)
		if (!strcmp(names[i], name))
			return i; // RETRO_DEVICE_ID_JOYPAD_* order
	fprintf(stderr, "unknown button %s\n", name);
	exit(1);
}

static void log_cb(enum retro_log_level level, const char *fmt, ...) {
	if (level < RETRO_LOG_WARN && !getenv("LR_VERBOSE"))
		return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static const char *option_value(const char *key) {
	size_t n = strlen(key);
#define ENDS(s) (n >= strlen(s) && !strcmp(key + n - strlen(s), s))
	if (ENDS("graphics_api")) return "Software";
	if (ENDS("is_new_3ds")) return getenv("LR_OLD3DS") ? "Old 3DS" : "New 3DS";
	if (ENDS("input_type")) return "none";
	if (ENDS("layout_option")) return "default";
	return NULL;
}

static bool environment(unsigned cmd, void *data) {
	switch (cmd) {
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			((struct retro_log_callback *)data)->log = log_cb;
			return true;
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
			pixel_format = *(const enum retro_pixel_format *)data;
			return true;
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
			*(const char **)data = sys_dir;
			return true;
		case RETRO_ENVIRONMENT_GET_VARIABLE: {
			struct retro_variable *v = data;
			v->value = option_value(v->key);
			return v->value != NULL;
		}
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			*(bool *)data = false;
			return true;
		case RETRO_ENVIRONMENT_GET_CAN_DUPE:
			*(bool *)data = true;
			return true;
		case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
			*(unsigned *)data = 0; // use SET_VARIABLES
			return true;
		case RETRO_ENVIRONMENT_SET_VARIABLES:
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
		case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
		case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
		case RETRO_ENVIRONMENT_SET_GEOMETRY:
		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
			return true;
		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
			return false;
		case RETRO_ENVIRONMENT_SET_HW_RENDER:
			return false; // software only
		default:
			return false;
	}
}

static void video_refresh(const void *data, unsigned w, unsigned h, size_t pitch) {
	if (data) {
		last_frame = data;
		last_w = w;
		last_h = h;
		last_pitch = pitch;
	}
}

static void save_ppm(int frame) {
	if (!last_frame) {
		fprintf(stderr, "frame %d: no image yet\n", frame);
		return;
	}
	char path[1024];
	snprintf(path, sizeof(path), "%s/frame_%05d.ppm", out_dir, frame);
	FILE *f = fopen(path, "wb");
	fprintf(f, "P6\n%u %u\n255\n", last_w, last_h);
	for (unsigned y = 0; y < last_h; y++) {
		const unsigned char *row = (const unsigned char *)last_frame + y * last_pitch;
		for (unsigned x = 0; x < last_w; x++) {
			unsigned char rgb[3];
			if (pixel_format == RETRO_PIXEL_FORMAT_XRGB8888) {
				unsigned p = ((const unsigned *)row)[x];
				rgb[0] = p >> 16; rgb[1] = p >> 8; rgb[2] = p;
			} else {
				unsigned short p = ((const unsigned short *)row)[x];
				if (pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
					rgb[0] = (p >> 11) << 3; rgb[1] = ((p >> 5) & 63) << 2; rgb[2] = (p & 31) << 3;
				} else {
					rgb[0] = ((p >> 10) & 31) << 3; rgb[1] = ((p >> 5) & 31) << 3; rgb[2] = (p & 31) << 3;
				}
			}
			fwrite(rgb, 1, 3, f);
		}
	}
	fclose(f);
	printf("shot %s (%ux%u)\n", path, last_w, last_h);
	fflush(stdout);
}

static int audio_peak; // loudest sample this frame
static long audio_frames_total;
static void audio_sample(int16_t l, int16_t r) {}
static size_t audio_batch(const int16_t *d, size_t frames) {
	audio_frames_total += frames;
	for (size_t i = 0; i < frames * 2; i++) {
		int a = d[i] < 0 ? -d[i] : d[i];
		if (a > audio_peak) audio_peak = a;
	}
	return frames;
}
static void input_poll(void) {}
static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port == 0 && device == RETRO_DEVICE_JOYPAD && id < 16)
		return held[id];
	return 0;
}

#define SYM(name) name##_t name = (name##_t)dlsym(core, #name); if (!name) { fprintf(stderr, "missing %s\n", #name); return 1; }
typedef void (*retro_init_t)(void);
typedef void (*retro_deinit_t)(void);
typedef void (*retro_run_t)(void);
typedef bool (*retro_load_game_t)(const struct retro_game_info *);
typedef void (*retro_set_environment_t)(retro_environment_t);
typedef void (*retro_set_video_refresh_t)(retro_video_refresh_t);
typedef void (*retro_set_audio_sample_t)(retro_audio_sample_t);
typedef void (*retro_set_audio_sample_batch_t)(retro_audio_sample_batch_t);
typedef void (*retro_set_input_poll_t)(retro_input_poll_t);
typedef void (*retro_set_input_state_t)(retro_input_state_t);

int main(int argc, char **argv) {
	if (argc < 5) {
		fprintf(stderr, "usage: %s core.so game.3dsx frames out_dir [script]\n", argv[0]);
		return 1;
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	out_dir = argv[4];
	mkdir(out_dir, 0755);
	snprintf(sys_dir, sizeof(sys_dir), "%s/system", out_dir);
	mkdir(sys_dir, 0755);
	int frames = atoi(argv[3]);

	if (argc > 5) {
		FILE *s = fopen(argv[5], "r");
		char line[256], name[64];
		int n;
		while (s && fgets(line, sizeof(line), s) && event_count < MAX_EVENTS) {
			if (sscanf(line, "shot %d", &n) == 1) {
				events[event_count].frame = n; events[event_count].type = 0; event_count++;
			} else if (sscanf(line, "press %d %63s", &n, name) == 2) {
				events[event_count].frame = n; events[event_count].type = 1; events[event_count].button = button_id(name); event_count++;
			} else if (sscanf(line, "release %d %63s", &n, name) == 2) {
				events[event_count].frame = n; events[event_count].type = 2; events[event_count].button = button_id(name); event_count++;
			}
		}
		if (s) fclose(s);
	}

	void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!core) {
		fprintf(stderr, "dlopen: %s\n", dlerror());
		return 1;
	}
	SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample)
	SYM(retro_set_audio_sample_batch) SYM(retro_set_input_poll) SYM(retro_set_input_state)
	SYM(retro_init) SYM(retro_load_game) SYM(retro_run) SYM(retro_deinit)

	retro_set_environment(environment);
	retro_set_video_refresh(video_refresh);
	retro_set_audio_sample(audio_sample);
	retro_set_audio_sample_batch(audio_batch);
	retro_set_input_poll(input_poll);
	retro_set_input_state(input_state);
	retro_init();

	struct retro_game_info info = { argv[2], NULL, 0, NULL };
	if (!retro_load_game(&info)) {
		fprintf(stderr, "retro_load_game failed\n");
		return 1;
	}

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (int f = 1; f <= frames; f++) {
		for (int e = 0; e < event_count; e++)
			if (events[e].frame == f && events[e].type)
				held[events[e].button] = events[e].type == 1;
		audio_peak = 0;
		retro_run();
		if (getenv("LR_AUDIO") && audio_peak > 64)
			printf("audio frame %d peak %d\n", f, audio_peak);
		for (int e = 0; e < event_count; e++)
			if (events[e].frame == f && events[e].type == 0)
				save_ppm(f);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("audio frames received: %ld\n", audio_frames_total);
	printf("%d frames in %.1f s of host time\n", frames, (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
	_exit(0); // skip the core's teardown
}
