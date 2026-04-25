#include <vitasdk.h>
#include <taihen.h>
#include <libk/stdlib.h>
#include <libk/stdio.h>
#include <libk/string.h>
#include "renderer.h"

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define HOOKS_NUM (16)
#define MENU_ENTRIES (4)
#define QUALITY_ENTRIES	(2)
#define RESOLUTION_ENTRIES (3)
#define TARGET_CHANNELS (2)

enum {
	NOT_TRIGGERED,
	CONFIG_MENU
};

// Hooks related variables
static SceUID g_hooks[HOOKS_NUM], record_thread_id;
static tai_hook_ref_t ref[HOOKS_NUM];
static uint8_t cur_hook = 0;

// Status related variables
static uint8_t status = NOT_TRIGGERED;
static uint8_t has_audio = 0;
static int cfg_i = 0;
static int qual_i = 0;
static int res_i = 0;
static char error[128] = {0};
static uint8_t is_recording = 0;
static uint32_t old_buttons = 0;
static uint32_t max_res = 0;
static char *qualities[] = {"2 MBPS", "1 MBPS"};
static uint32_t res_x[] = {640, 480, 368};
static uint32_t res_y[] = {368, 272, 208};
static char *menu[] = {"Video Bitrate: ", "Video Resolution: ", "Audio Recording: ", " Screen Recording"};
static int error_countdown = -1;
static int vts = 0; // Video Timescale
static int ats = 0; // Audio Timescale
static int audio_chns = 0;
static int audiobuf_size = 0;
static int opened_ports = 0;
static uint32_t samplerate = 0;
static char titleid[16];
void *csc_ptr;
uint32_t csc_size;
uint32_t internal_csc_size;
SceMp4RecRecorder r = {0};
uint32_t enc_size, av_size;
SceUID enc_blk, av_blk;

#define NUM_AUDIOBUFS 16
typedef struct {
	uint8_t samples[SCE_MP4REC_AUDIO_BUFFER_SIZE];
	int offs;
} audio_buffer;
audio_buffer audiobufs[NUM_AUDIOBUFS] = {};
uint8_t cur_write_idx = 0;
uint8_t cur_read_idx = 0;

// Config Menu Renderer
void drawConfigMenu() {
	int i;
	for (i = 0; i < MENU_ENTRIES; i++) {
		(i == cfg_i) ? setTextColor(0x0000FF00) : setTextColor(0x00FFFFFF);
		switch (i) {
			case 0:
				drawStringF(5, 100 + i * 20, "%s%s", menu[i], qualities[qual_i]);
				break;
			case 1:
				drawStringF(5, 100 + i * 20, "%s%dx%d", menu[i], res_x[res_i], res_y[res_i]);
				break;
			case 2:
				drawStringF(5, 100 + i * 20, "%s%s", menu[i], has_audio ? "Enabled" : "Disabled");
				break;
			case 3:
				drawStringF(5, 100 + i * 20, "%s%s", is_recording ? "Stop" : "Start", menu[i]);
				break;
			default:
				drawString(5, 100 + i * 20, menu[i]);
				break;
		}
	}
	setTextColor(0x00FFFFFF);
}

// Generic hooking function
int hookFunction(uint32_t nid, const void* func) {
	g_hooks[cur_hook] = taiHookFunctionImport(&ref[cur_hook], TAI_MAIN_MODULE, TAI_ANY_LIBRARY, nid, func);
	cur_hook++;
	return g_hooks[cur_hook - 1];
}

// Alters recording state
void alterRecordingState() {
	is_recording = !is_recording;
	if (is_recording) {
		cur_read_idx = cur_write_idx = ats = vts = 0;
		sceClibMemset(audiobufs[0].samples, 0, SCE_MP4REC_AUDIO_BUFFER_SIZE);
		sceMp4RecQueryPhysicalMemSize(&r, (res_i * 2) + qual_i, &enc_size, &av_size);
		internal_csc_size = (res_x[res_i] * res_y[res_i] * 3) / 2;
		SceMp4RecInitParam param = {};
		param.size = sizeof(SceMp4RecInitParam);
		param.mode = (res_i * 2) + qual_i;
		param.encoder_size = enc_size;
		param.av_size = av_size;
		param.affinity = 0x00070000;
		param.priority = 0x10000100;
		sceKernelGetMemBlockBase(enc_blk, &param.encoder_mem);
		sceKernelGetMemBlockBase(av_blk, &param.av_mem);
		int res = sceMp4RecInit(&r, &param);
		if (res < 0) {
			sprintf(error, "Cannot start video recording (0x%08X).", res);
			max_res = 0xFF;
			is_recording = 0;
		}
		status = NOT_TRIGGERED;
	} else {
		SceMp4RecTermParam params = {0};
		params.size = sizeof(SceMp4RecTermParam);
		params.discard = 0;
		int res = sceMp4RecTerm(&r, &params);
		if (res < 0)
			sprintf(error, "Failed to save video file (0x%08X).", res);
		else
			sprintf(error, "Video saved in ux0:video.");
		error_countdown = sceKernelGetProcessTimeWide();
	}
}

// Checking buttons startup/closeup
void checkInput(SceCtrlData *ctrl) {
	if (status != NOT_TRIGGERED) {
		if ((ctrl->buttons & SCE_CTRL_DOWN) && (!(old_buttons & SCE_CTRL_DOWN))) {
			cfg_i++;
			if (cfg_i >= MENU_ENTRIES) cfg_i = 0;
		} else if ((ctrl->buttons & SCE_CTRL_UP) && (!(old_buttons & SCE_CTRL_UP))) {
			cfg_i--;
			if (cfg_i < 0 ) cfg_i = MENU_ENTRIES-1;
		} else if ((ctrl->buttons & SCE_CTRL_CROSS) && (!(old_buttons & SCE_CTRL_CROSS))) {
			switch (cfg_i) {
			case 0:
				if (!is_recording) {
					qual_i = (qual_i + 1) % QUALITY_ENTRIES;
				}
				break;
			case 1:
				if (!is_recording) {
					res_i = (res_i + 1) % RESOLUTION_ENTRIES;
					while (res_i < max_res) {
						res_i++;
					}
				}
				break;
			case 2:
				if (opened_ports == 1) {
					has_audio = !has_audio;
				}
				break;
			case 3:
				alterRecordingState();
				break;
			default:
				break;
			}
		}else if ((ctrl->buttons & SCE_CTRL_TRIANGLE) && (!(old_buttons & SCE_CTRL_TRIANGLE))) {
			status = NOT_TRIGGERED;
		}
	} else if ((ctrl->buttons & SCE_CTRL_LTRIGGER) && (ctrl->buttons & SCE_CTRL_SELECT)) {
		status = CONFIG_MENU;
	} else if (((ctrl->buttons & SCE_CTRL_LTRIGGER) && (ctrl->buttons & SCE_CTRL_START))
		&& (!((old_buttons & SCE_CTRL_LTRIGGER) && (old_buttons & SCE_CTRL_START)))) {
		alterRecordingState();
	}
	old_buttons = ctrl->buttons;
}

// Asynchronous audio recording thread
int record_thread(SceSize args, void *argp) {
	for (;;) {
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);
		checkInput(&pad);

record_ended:		
		if (is_recording) {
			// Send audio samples until we're behind submitted video samples
			int vcurr = vts * (SCE_MP4REC_COMMON_DENOM_TIMESCALE / SCE_MP4REC_VIDEO_TIMESCALE);
			int acurr = ats * (SCE_MP4REC_COMMON_DENOM_TIMESCALE / SCE_MP4REC_AUDIO_TIMESCALE);
			while (acurr < vcurr) {
				if (!is_recording) {
					goto record_ended;
				}
                audio_buffer *buf = &audiobufs[cur_read_idx];
                if (has_audio) {
                    while (buf->offs != SCE_MP4REC_AUDIO_BUFFER_SIZE || cur_read_idx == cur_write_idx) {
                        sceKernelDelayThread(1000);
						if (!is_recording) {
							goto record_ended;
						}
                    }
                    sceMp4RecAddAudioSample(&r, buf->samples, SCE_MP4REC_AUDIO_BUFFER_SIZE);
                    cur_read_idx = (cur_read_idx + 1) % NUM_AUDIOBUFS;
                } else {
                    sceMp4RecAddAudioSample(&r, buf->samples, SCE_MP4REC_AUDIO_BUFFER_SIZE);
                }
                ats += SCE_MP4REC_AUDIO_SAMPLE_DURATION;
                acurr = ats * (SCE_MP4REC_COMMON_DENOM_TIMESCALE / SCE_MP4REC_AUDIO_TIMESCALE);
            }
		}
		sceKernelDelayThread(1000);
	}

	return 0;
}

int sceDisplaySetFrameBuf_patched(const SceDisplayFrameBuf *pParam, int sync) {
	static uint8_t firstBoot = 1;
	if (firstBoot) {
		firstBoot = 0;
		
		// Initializing internal renderer
		setTextColor(0x00FFFFFF);
	}
	
	updateFramebuf(pParam);
	
	if (status == NOT_TRIGGERED) {
		if (error[0]) {
			if (error_countdown != 0) {
				drawString(5,5, error);
				if (error_countdown > 0) {
					error_countdown = sceKernelGetProcessTimeWide() - error_countdown > 3000000 ? 0 : error_countdown;
				}
			}
		}
	} else if (max_res != 0xFF) {
		switch (status) {
		case CONFIG_MENU:
			drawString(5, 5, "Vita MP4 Recorder v.0.5 - CONFIG MENU");
			drawStringF(5, 25, "Title ID: %s", titleid);
			drawStringF(5, 45, "Game Resolution: %dx%d", pParam->width, pParam->height);
			drawStringF(5, 65, "Audio: %d Ports (%dMhz, %d, %d)", opened_ports, samplerate, audio_chns, audiobuf_size);
			drawConfigMenu();
			break;
		default:
			break;
		}
	}
	
	// FIXME: sceMp4Rec records at hardcoded 30 FPS. With the way we do recording we cause hard lock at 30 FPS on 60 FPS games.
	if (is_recording) {
		int vcurr = vts * (SCE_MP4REC_COMMON_DENOM_TIMESCALE / SCE_MP4REC_VIDEO_TIMESCALE);
		int acurr = ats * (SCE_MP4REC_COMMON_DENOM_TIMESCALE / SCE_MP4REC_AUDIO_TIMESCALE);
		if (vcurr - acurr < SCE_MP4REC_COMMON_DENOM_TIMESCALE) {
			SceMp4RecFrame dst = {0};
			dst.size = sizeof(SceMp4RecFrame);
			dst.pixelformat = SCE_MP4REC_PIXELFORMAT_YUV420_PACKED;
			dst.stride = res_x[res_i];
			dst.width = res_x[res_i];
			dst.height = res_y[res_i];
			dst.buffer = csc_ptr;
			SceMp4RecFrame src = {0};
			src.size = sizeof(SceMp4RecFrame);
			src.pixelformat = SCE_MP4REC_PIXELFORMAT_A8B8G8R8;
			src.stride = pParam->pitch;
			src.width = pParam->width;
			src.height = pParam->height;
			src.buffer = pParam->base;
			sceMp4RecCsc(&dst, &src);
			sceMp4RecAddVideoSample(&r, csc_ptr, internal_csc_size);
			vts += SCE_MP4REC_VIDEO_SAMPLE_DURATION;
		}
		setTextColor(0xFF0000FF);
		drawString(5, 5, "R");
		setTextColor(0x00FFFFFF);
	}
	
	return TAI_CONTINUE(int, ref[0], pParam, sync);
}

int genericInputDisable(int idx, int port, SceCtrlData *ctrl, int count, int is_negative) {
	int ret = TAI_CONTINUE(int, ref[idx], port, ctrl, count);
	
	if (status == CONFIG_MENU) // Disable input handling when in config menu
		ctrl->buttons = is_negative ? 0xFFFFFFFF : 0;
	
	return ret;
}

int sceCtrlPeekBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(1, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(2, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositive_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(3, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositive2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(4, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(5, port, ctrl, count, 0);
}

int sceCtrlPeekBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(6, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositiveExt_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(7, port, ctrl, count, 0);
}

int sceCtrlReadBufferPositiveExt2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(8, port, ctrl, count, 0);
}

int sceCtrlPeekBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(9, port, ctrl, count, 1);
}

int sceCtrlPeekBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(10, port, ctrl, count, 1);
}

int sceCtrlReadBufferNegative_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(11, port, ctrl, count, 1);
}

int sceCtrlReadBufferNegative2_patched(int port, SceCtrlData *ctrl, int count) {
	return genericInputDisable(12, port, ctrl, count, 1);
}

int sceAudioOutOpenPort_patched(int type, int len, int freq, int mode) {
	opened_ports++;
	audio_chns = mode + 1;
	audiobuf_size = len * audio_chns * 2;
	samplerate = freq;
	return TAI_CONTINUE(int, ref[13], type, len, freq, mode);
}

typedef struct {
	uint32_t src_srate;
	uint32_t phase;
	uint32_t phase_inc;
} resampler_state;
resampler_state resampler;

static void resampler_init(resampler_state *rs, uint32_t src_srate) {
	rs->src_srate  = src_srate;
	rs->phase_inc = (uint32_t)(((uint64_t)src_srate << 16) / SCE_MP4REC_AUDIO_TIMESCALE);
	rs->phase = 0;
}

int sceAudioOutOutput_patched(int port, const void* _buf) {
	int r = TAI_CONTINUE(int, ref[14], port, _buf);
	
	// Keep the audio info updated, in case the game kills a previous port and we have stale info
	if (opened_ports == 1) {
		audio_chns = sceAudioOutGetConfig(port, SCE_AUDIO_OUT_CONFIG_TYPE_MODE) + 1;
		audiobuf_size = sceAudioOutGetConfig(port, SCE_AUDIO_OUT_CONFIG_TYPE_LEN) * audio_chns * 2;
		samplerate = sceAudioOutGetConfig(port, SCE_AUDIO_OUT_CONFIG_TYPE_FREQ);
	}
	
	// FIXME: We'd need an audio mixer to support setups with more than one opened port.
	if (opened_ports == 1 && is_recording && has_audio) {
		int left_samples = audiobuf_size;
		uint8_t *src_buf = (uint8_t *)_buf;
		audio_buffer *buf = &audiobufs[cur_write_idx];
		if (samplerate == SCE_MP4REC_AUDIO_TIMESCALE && audio_chns == TARGET_CHANNELS) { // 48khz stereo
			while (left_samples > 0) {
				int size = (SCE_MP4REC_AUDIO_BUFFER_SIZE - buf->offs) >= left_samples ? left_samples : (SCE_MP4REC_AUDIO_BUFFER_SIZE - buf->offs);
				sceClibMemcpy(&buf->samples[buf->offs], src_buf, size);
				src_buf += size;
				buf->offs += size;
				left_samples -= size;
				if (buf->offs == SCE_MP4REC_AUDIO_BUFFER_SIZE) {
					cur_write_idx = (cur_write_idx + 1) % NUM_AUDIOBUFS;
					buf = &audiobufs[cur_write_idx];
					buf->offs = 0;
				}
			}
		} else if (samplerate == SCE_MP4REC_AUDIO_TIMESCALE) { // Needs mono -> stereo
			while (left_samples > 0) {
				int size = (SCE_MP4REC_AUDIO_BUFFER_SIZE - buf->offs) >= (left_samples * 2) ? (left_samples * 2) : (SCE_MP4REC_AUDIO_BUFFER_SIZE - buf->offs);
				for (int i = 0; i < size / 2; i += 2) {
					buf->samples[buf->offs++] = src_buf[i];
					buf->samples[buf->offs++] = src_buf[i + 1];
					buf->samples[buf->offs++] = src_buf[i];
					buf->samples[buf->offs++] = src_buf[i + 1];
				}
				src_buf += size / 2;
				left_samples -= size / 2;
				if (buf->offs == SCE_MP4REC_AUDIO_BUFFER_SIZE) {
					cur_write_idx = (cur_write_idx + 1) % NUM_AUDIOBUFS;
					buf = &audiobufs[cur_write_idx];
					buf->offs = 0;
				}
			}
		} else { // Needs resampling
			const int16_t *src = (const int16_t *)_buf;
			int src_frames = audiobuf_size / (2 * audio_chns);
			if (resampler.src_srate != samplerate) {
				resampler_init(&resampler, samplerate);
			}
			for (;;) {
				uint32_t idx  = resampler.phase >> 16;
				uint32_t frac = resampler.phase & 0xFFFF;
				if ((int)idx >= src_frames) {
					break;
				}
				int next_idx = ((int)idx + 1 < src_frames) ? (int)idx + 1 : (int)idx;
				int16_t cur_l, cur_r, nxt_l, nxt_r;
				if (audio_chns == 2) {
					cur_l = src[idx * 2 + 0];
					cur_r = src[idx * 2 + 1];
					nxt_l = src[next_idx * 2 + 0];
					nxt_r = src[next_idx * 2 + 1];
				} else {
					cur_l = cur_r = src[idx];
					nxt_l = nxt_r = src[next_idx];
				}
				int16_t out_l = (int16_t)(cur_l + ((int32_t)(nxt_l - cur_l) * (int32_t)frac >> 16));
				int16_t out_r = (int16_t)(cur_r + ((int32_t)(nxt_r - cur_r) * (int32_t)frac >> 16));
				int16_t *dst = (int16_t *)&buf->samples[buf->offs];
				dst[0] = out_l;
				dst[1] = out_r;
				buf->offs += 4;
				if (buf->offs == SCE_MP4REC_AUDIO_BUFFER_SIZE) {
					cur_write_idx = (cur_write_idx + 1) % NUM_AUDIOBUFS;
					buf	= &audiobufs[cur_write_idx];
					buf->offs = 0;
				}
				resampler.phase += resampler.phase_inc;
			}
			resampler.phase -= (uint32_t)src_frames << 16;
		}
	}
	return r; 
}

int sceAudioOutReleasePort_patched(int port) {
	opened_ports--;
	return TAI_CONTINUE(int, ref[15], port);
}

uint8_t has_hooks = 0;
void _start() __attribute__ ((weak, alias ("module_start")));
int module_start(SceSize argc, const void *args) {
	// Setting maximum clocks
	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	
	// Checking if game is blacklisted
	sceAppMgrAppParamGetString(0, 12, titleid , 256);
	if (strncmp(titleid, "NPXS", 4) == 0) { // System Apps
		return SCE_KERNEL_START_NO_RESIDENT;
	} else if (strlen(titleid) < 9) { // Livearea
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	
	char vpath[1024];
	sceAppMgrConvertVs0UserDrivePath("vs0:sys/external/libfios2.suprx", vpath, 1024);
	int res = sceKernelLoadStartModule(vpath, 0, NULL, 0, NULL, NULL);
	if (res != SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB && res < 0) {
		sprintf(error, "Cannot load libfios2.suprx. (0x%08X)", res);
		max_res = 0xFF;
		goto err;
	}
	
	sceAppMgrConvertVs0UserDrivePath("vs0:sys/external/libc.suprx", vpath, 1024);
	res = sceKernelLoadStartModule(vpath, 0, NULL, 0, NULL, NULL);
	if (res != SCE_KERNEL_ERROR_MODULEMGR_OLD_LIB && res < 0) {
		sprintf(error, "Cannot load libc.suprx. (0x%08X)", res);
		max_res = 0xFF;
		goto err;
	}
	res = sceSysmoduleLoadModule(SCE_SYSMODULE_MP4_RECORDER);
	if (res != 0) {
		sprintf(error, "Cannot load sceMp4Rec module. (0x%08X)", res);
		max_res = 0xFF;
		goto err;		
	}
	
	SceUID base_blk = sceKernelAllocMemBlock("recorder blk", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, ALIGN(4 * 1024 * 1024, 4 * 1024), NULL);
	r.size = sizeof(SceMp4RecRecorder);
	sceKernelGetMemBlockBase(base_blk, &r.base);
	r.base_size = 4 * 1024 * 1024;
	sceMp4RecCreateRecorder(&r);
	
	SceUID csc_blk;
	do {
		sceMp4RecQueryPhysicalMemSize(&r, max_res * 2, &enc_size, &av_size);
		enc_blk = sceKernelAllocMemBlock("encoder blk", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, ALIGN(enc_size, 1 * 1024 * 1024), NULL);
		if (enc_blk < 0) {
			sprintf(error, "Not enough memory to allocate encoder block.");
			goto noenc;
		}
		av_blk = sceKernelAllocMemBlock("av blk", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, ALIGN(av_size, 1 * 1024 * 1024), NULL);
		if (av_blk < 0) {
			sprintf(error, "Not enough memory to allocate AV block.");
			goto noav;
		}
	
		csc_size = (res_x[max_res] * res_y[max_res] * 3) / 2;
		csc_blk = sceKernelAllocMemBlock("csc blk", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, ALIGN(csc_size, 1 * 1024 * 1024), NULL);
		if (csc_blk < 0) {
noav:		
			sceKernelFreeMemBlock(av_blk);
noenc:
			sceKernelFreeMemBlock(enc_blk);
			max_res++;
		}
	} while (csc_blk < 0 && max_res < RESOLUTION_ENTRIES);
	
	if (csc_blk >= 0) {
		sceKernelGetMemBlockBase(csc_blk, &csc_ptr);
		res_i = max_res;
	} else {
		sprintf(error, "Not enough memory to run MP4 encoder.");
		sceMp4RecDeleteRecorder(&r);
		sceKernelFreeMemBlock(base_blk);
		sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4_RECORDER);
		goto err;
	}

	// Hooking needed functions
	hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	hookFunction(0xA9C3CED6, sceCtrlPeekBufferPositive_patched);
	hookFunction(0x15F81E8C, sceCtrlPeekBufferPositive2_patched);
	hookFunction(0x67E7AB83, sceCtrlReadBufferPositive_patched);
	hookFunction(0xC4226A3E, sceCtrlReadBufferPositive2_patched);
	hookFunction(0xA59454D3, sceCtrlPeekBufferPositiveExt_patched);
	hookFunction(0x860BF292, sceCtrlPeekBufferPositiveExt2_patched);
	hookFunction(0xE2D99296, sceCtrlReadBufferPositiveExt_patched);
	hookFunction(0xA7178860, sceCtrlReadBufferPositiveExt2_patched);
	hookFunction(0x104ED1A7, sceCtrlPeekBufferNegative_patched);
	hookFunction(0x81A89660, sceCtrlPeekBufferNegative2_patched);
	hookFunction(0x15F96FB0, sceCtrlReadBufferNegative_patched);
	hookFunction(0x27A0C5FB, sceCtrlReadBufferNegative2_patched);
	hookFunction(0x5BC341E4, sceAudioOutOpenPort_patched);
	hookFunction(0x02DB3F5F, sceAudioOutOutput_patched);
	hookFunction(0x69E2E6B5, sceAudioOutReleasePort_patched);
	has_hooks = 1;
	
	SceMp4RecInitParam param;
	sceClibMemset(&param,0,sizeof(SceMp4RecInitParam));
	param.size = sizeof(SceMp4RecInitParam);
	param.mode = (res_i * 2) + qual_i;
	param.encoder_size = enc_size;
	param.av_size = av_size;
	param.affinity = 0x00070000;
	param.priority = 0x10000100;
	sceKernelGetMemBlockBase(enc_blk, &param.encoder_mem);
	sceKernelGetMemBlockBase(av_blk, &param.av_mem);
	res = sceMp4RecInit(&r, &param);
	if (res < 0) {
		sprintf(error, "Video recorder not supported (0x%08X).", res);
		sceKernelFreeMemBlock(av_blk);
		sceKernelFreeMemBlock(enc_blk);
		sceMp4RecDeleteRecorder(&r);
		sceKernelFreeMemBlock(base_blk);
		sceKernelFreeMemBlock(csc_blk);
		sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4_RECORDER);
		goto err;
	} else {
		SceMp4RecTermParam params = {0};
		params.size = sizeof(SceMp4RecTermParam);
		params.discard = 1;
		sceMp4RecTerm(&r, &params);
	}

	record_thread_id = sceKernelCreateThread("record_thread", record_thread, 0xA0, 0x100000, 0, 0, NULL);
	if (record_thread_id >= 0)
		sceKernelStartThread(record_thread_id, 0, NULL);
	else {
		sprintf(error, "Failed to start recorder thread.");
		sceKernelFreeMemBlock(av_blk);
		sceKernelFreeMemBlock(enc_blk);
		sceMp4RecDeleteRecorder(&r);
		sceKernelFreeMemBlock(base_blk);
		sceKernelFreeMemBlock(csc_blk);
		sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4_RECORDER);
	}

err:
	if (!has_hooks)
		hookFunction(0x7A410B64, sceDisplaySetFrameBuf_patched);
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	// Freeing hooks
	if (has_hooks) {
		for (int i = 0; i < HOOKS_NUM; i++) {
			taiHookRelease(g_hooks[i], ref[i]);
		}
	} else {
		taiHookRelease(g_hooks[0], ref[0]);
	}
	
	return SCE_KERNEL_STOP_SUCCESS;
	
}