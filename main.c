/*
 * Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdatomic.h>

#ifdef HAVE_LIBFTDI1
#	include <libftdi1/ftdi.h>
#else
#	include <ftdi.h>
#endif // HAVE_LIBFTDI1

#include <osc.lv2/stream.h>

#include <varchunk.h>
#include <osc2ftdidmx.h>

#define FTDI_VID   0x0403
#define FT232_PID  0x6001
#define NSECS      1000000000
#define JAN_1970   2208988800ULL

//#define FTDI_SKIP

typedef struct _sched_t sched_t;
typedef struct _app_t app_t;

struct _sched_t {
	sched_t *next;
	struct timespec to;
	size_t len;
	uint8_t buf [];
};

struct _app_t {
	uint16_t vid;
	uint16_t pid;
	const char *sid;
	const char *des;
	uint32_t fps;
	const char *url;

	LV2_OSC_Stream stream;
	pthread_t thread;

	struct ftdi_context ftdi;

	sched_t *list;

	struct {
		int out;
		int inp;
	} priority;

	struct {
		varchunk_t *rx;
		varchunk_t *tx;
	} rb;

	state_t state;

	struct {
		uint8_t start_code [1];
		uint8_t data [512];
	} __attribute__((packed)) dmx;
};

static atomic_bool reconnect = ATOMIC_VAR_INIT(false);
static atomic_bool done = ATOMIC_VAR_INIT(false);

static void
_sig(int num __attribute__((unused)))
{
	atomic_store(&reconnect, false);
	atomic_store(&done, true);
}

static void *
_write_req(void *data, size_t minimum, size_t *maximum)
{
	app_t *app = data;

	return varchunk_write_request_max(app->rb.rx, minimum, maximum);
}

static void
_write_adv(void *data, size_t written)
{
	app_t *app = data;

	varchunk_write_advance(app->rb.rx, written);
}

static const void *
_read_req(void *data, size_t *toread)
{
	app_t *app = data;

	return varchunk_read_request(app->rb.tx, toread);
}

static void
_read_adv(void *data)
{
	app_t *app = data;

	varchunk_read_advance(app->rb.tx);
}

static const LV2_OSC_Driver driver = {
	.write_req = _write_req,
	.write_adv = _write_adv,
	.read_req = _read_req,
	.read_adv = _read_adv
};

static void
_handle_osc_packet(app_t *app, uint64_t timetag, const uint8_t *buf, size_t len);

static void
_handle_osc_message(app_t *app __attribute__((unused)), LV2_OSC_Reader *reader,
	size_t len)
{
	state_t *state = &app->state;

	state->cur_channel = 0;
	state->cur_value = 0;
	state->cur_set = false;

	state->cur_reader = *reader;
	state->cur_arg = OSC_READER_MESSAGE_BEGIN(&state->cur_reader, len);

	lv2_osc_reader_match(reader, len, tree_root, state);
}

static void
_handle_osc_bundle(app_t *app, LV2_OSC_Reader *reader, size_t len)
{
	OSC_READER_BUNDLE_FOREACH(reader, itm, len)
	{
		_handle_osc_packet(app, itm->timetag, itm->body, itm->size);
	}
}

static sched_t *
_sched_append(sched_t *list, sched_t *elmnt)
{
	if(!list)
	{
		elmnt->next = NULL;
		return elmnt;
	}

	sched_t *prev = NULL;
	for(sched_t *ptr = list; ptr; prev = ptr, ptr = ptr->next)
	{
		if(  (ptr->to.tv_sec > elmnt->to.tv_sec)
			&& (ptr->to.tv_nsec > elmnt->to.tv_nsec) )
		{
			break;
		}
	}

	if(!prev)
	{
		elmnt->next = list;
		return elmnt;
	}

	elmnt->next = prev->next;
	prev->next = elmnt;

	return list;
}

static void
_handle_osc_packet(app_t *app, uint64_t timetag, const uint8_t *buf, size_t len)
{
	LV2_OSC_Reader reader;
	lv2_osc_reader_initialize(&reader, buf, len);

	if(lv2_osc_reader_is_bundle(&reader))
	{
		_handle_osc_bundle(app, &reader, len);
	}
	else if(lv2_osc_reader_is_message(&reader))
	{
		if(timetag == LV2_OSC_IMMEDIATE)
		{
			_handle_osc_message(app, &reader, len);
		}
		else
		{
			sched_t *elmnt = malloc(sizeof(sched_t) + len);
			if(elmnt)
			{
				elmnt->next = NULL;
				elmnt->to.tv_sec = (timetag >> 32) - JAN_1970;
				elmnt->to.tv_nsec = (timetag && 32) * 0x1p-32 * 1e9;
				elmnt->len = len;
				memcpy(elmnt->buf, buf, len);

				app->list = _sched_append(app->list, elmnt);
			}
			else
			{
				syslog(LOG_ERR, "[%s] malloc failed", __func__);
			}
		}
	}
}

static int
_ftdi_xmit(app_t *app)
{
#if !defined(FTDI_SKIP)
	if(ftdi_set_line_property2(&app->ftdi, BITS_8, STOP_BIT_2, NONE,
		BREAK_ON) != 0)
	{
		goto failure;
	}

	if(ftdi_set_line_property2(&app->ftdi, BITS_8, STOP_BIT_2, NONE,
		BREAK_OFF) != 0)
	{
		goto failure;
	}

	const ssize_t sz = sizeof(app->dmx.start_code) + sizeof(app->dmx.data);
	if(ftdi_write_data(&app->ftdi, app->dmx.start_code, sz) != sz)
	{
		goto failure;
	}
#else
	(void)app;
#endif

	return 0;

#if !defined(FTDI_SKIP)
failure:
	syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
	return 1;
#endif
}

static int
_ftdi_init(app_t *app)
{
#if !defined(FTDI_SKIP)
	app->ftdi.module_detach_mode = AUTO_DETACH_SIO_MODULE;

	if(ftdi_init(&app->ftdi) != 0)
	{
		goto failure;
	}

	if(ftdi_set_interface(&app->ftdi, INTERFACE_ANY) != 0)
	{
		goto failure_deinit;
	}

	if(app->des || app->sid)
	{
		if(ftdi_usb_open_desc(&app->ftdi, app->vid, app->pid,
			app->des, app->sid) != 0)
		{
			goto failure_deinit;
		}
	}
	else
	{
		if(ftdi_usb_open(&app->ftdi, app->vid, app->pid) != 0)
		{
			goto failure_deinit;
		}
	}

	if(ftdi_usb_reset(&app->ftdi) != 0)
	{
		goto failure_close;
	}

	if(ftdi_set_baudrate(&app->ftdi, 250000) != 0)
	{
		goto failure_close;
	}

	if(ftdi_set_line_property2(&app->ftdi, BITS_8, STOP_BIT_2, NONE,
		BREAK_ON) != 0)
	{
		goto failure_close;
	}

	if(ftdi_usb_purge_buffers(&app->ftdi) != 0)
	{
		goto failure_close;
	}

	if(ftdi_setflowctrl(&app->ftdi, SIO_DISABLE_FLOW_CTRL) != 0)
	{
		goto failure_close;
	}

	if(ftdi_setrts(&app->ftdi, 0) != 0)
	{
		goto failure_close;
	}
#else
	(void)app;
#endif

	return 0;

#if !defined(FTDI_SKIP)
failure_close:
	ftdi_usb_close(&app->ftdi);

failure_deinit:
	ftdi_deinit(&app->ftdi);

failure:
	syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
	return -1;
#endif
}

static void
_ftdi_deinit(app_t *app)
{
#if !defined(FTDI_SKIP)
	if(ftdi_usb_close(&app->ftdi) != 0)
	{
		syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
	}

	ftdi_deinit(&app->ftdi);
#else
	(void)app;
#endif
}

static void
_osc_deinit(app_t *app)
{
	lv2_osc_stream_deinit(&app->stream);

	if(app->rb.rx)
	{
		varchunk_free(app->rb.rx);
	}

	if(app->rb.tx)
	{
		varchunk_free(app->rb.tx);
	}
}

static int
_osc_init(app_t *app)
{
	app->rb.rx = varchunk_new(8192, true);
	if(!app->rb.rx)
	{
		goto failure;
	}

	app->rb.tx = varchunk_new(8192, true);
	if(!app->rb.tx)
	{
		goto failure;
	}

	if(lv2_osc_stream_init(&app->stream, app->url, &driver, app) != 0)
	{
		syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
		goto failure;
	}

	return 0;

failure:
	_osc_deinit(app);
	return -1;
}

static void
_thread_priority(int priority)
{
		if(priority == 0)
		{
			return;
		}

		const struct sched_param schedp = {
			.sched_priority = priority
		};

		const pthread_t self = pthread_self();

		if(pthread_setschedparam(self, SCHED_FIFO, &schedp) != 0)
		{
			syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
		}
}

static void *
_beat(void *data)
{
	app_t *app = data;
	state_t *state = &app->state;

	const uint64_t step_ns = NSECS / app->fps;

	_thread_priority(app->priority.out);

	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);

	while(!atomic_load(&done))
	{
		// sleep until next beat timestamp
		if(clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &to, NULL) == -1)
		{
			continue;
		}

		// read OSC messages from ringbuffer
		const uint8_t *buf;
		size_t len;
		while( (buf = varchunk_read_request(app->rb.rx, &len)) )
		{
			_handle_osc_packet(app, LV2_OSC_IMMEDIATE, buf, len);

			varchunk_read_advance(app->rb.rx);
		}

		// read OSC messages from list
		for(sched_t *elmnt = app->list; elmnt; elmnt = app->list)
		{
			double diff = to.tv_sec - elmnt->to.tv_sec;
			diff += (to.tv_nsec - elmnt->to.tv_nsec) * 1e-9;

			if(diff < 0.0)
			{
				break;
			}

			_handle_osc_packet(app, LV2_OSC_IMMEDIATE, elmnt->buf, elmnt->len);

			app->list = elmnt->next;
			free(elmnt);
		}

		// fill dmx buffer
		for(uint32_t i = 0; i < 512; i++)
		{
			slot_t *slot = &state->slots[i];

			app->dmx.data[i] = slot_get_val(slot);
		}

		// write DMX data
		if(_ftdi_xmit(app) != 0)
		{
			atomic_store(&done, true); // end xmit loop
		}

		// calculate next beat timestamp
		to.tv_nsec += step_ns;
		while(to.tv_nsec >= NSECS)
		{
			to.tv_sec += 1;
			to.tv_nsec -= NSECS;
		}
	}

	return NULL;
}

static int
_thread_init(app_t *app)
{
	if(pthread_create(&app->thread, NULL, _beat, app) != 0)
	{
		syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
		return -1;
	}

	return 0;
}

static void
_thread_deinit(app_t *app)
{
	pthread_join(app->thread, NULL);
}

static void
_sched_deinit(app_t *app)
{
	for(sched_t *elmnt = app->list; elmnt; elmnt = app->list)
	{
		app->list = elmnt->next;
		free(elmnt);
	}
}

static int
_loop(app_t *app)
{
	if(_osc_init(app) == -1)
	{
		return -1;
	}

	if(_ftdi_init(app) == -1)
	{
		_osc_deinit(app);
		return -1;
	}

	if(_thread_init(app) == -1)
	{
		_ftdi_deinit(app);
		_osc_deinit(app);
		return -1;
	}

	atomic_store(&done, false);

	_thread_priority(app->priority.inp);

	while(!atomic_load(&done))
	{
		const LV2_OSC_Enum status = lv2_osc_stream_pollin(&app->stream, 1000);

		if(status & LV2_OSC_ERR)
		{
			syslog(LOG_ERR, "[%s] '%s'", __func__, strerror(errno));
		}
	}

	_thread_deinit(app);
	_sched_deinit(app);
	_ftdi_deinit(app);
	_osc_deinit(app);

	return 0;
}

static void
_version(void)
{
	fprintf(stderr,
		"--------------------------------------------------------------------\n"
		"This is free software: you can redistribute it and/or modify\n"
		"it under the terms of the Artistic License 2.0 as published by\n"
		"The Perl Foundation.\n"
		"\n"
		"This source is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
		"Artistic License 2.0 for more details.\n"
		"\n"
		"You should have received a copy of the Artistic License 2.0\n"
		"along the source as a COPYING file. If not, obtain it from\n"
		"http://www.perlfoundation.org/artistic_license_2_0.\n\n");
}

static void
_usage(char **argv, app_t *app)
{
	fprintf(stderr,
		"--------------------------------------------------------------------\n"
		"USAGE\n"
		"   %s [OPTIONS]\n"
		"\n"
		"OPTIONS\n"
		"   [-v]                     print version information\n"
		"   [-h]                     print usage information\n"
		"   [-d]                     enable verbose logging\n"
		"   [-A]                     enable auto-reconnect (disabled)\n"
		"   [-V] VID                 USB vendor ID (0x%04"PRIx16")\n"
		"   [-P] PID                 USB product ID (0x%04"PRIx16")\n"
		"   [-D] DESCRIPTION         USB product name (%s)\n"
		"   [-S] SERIAL              USB serial ID (%s)\n"
		"   [-F] FPS                 Frame rate (%"PRIu32")\n"
		"   [-U] URI                 OSC URI (%s)\n"
		"   [-I] PRIORITY            Input (OSC) realtime thread priority (%i)\n"
		"   [-O] PRIORITY            Output (DMX) realtime thread priority(%i)\n\n"
		, argv[0], app->vid, app->pid, app->des, app->sid, app->fps, app->url,
		app->priority.inp, app->priority.out);
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	static app_t app;
	int logp = LOG_INFO;

	app.vid = FTDI_VID;
	app.pid = FT232_PID;
	app.des = NULL;
	app.sid = NULL;
	app.fps = 30;
	app.url = "osc.udp://:6666";
	app.priority.inp = 0;
	app.priority.out = 0;


	fprintf(stderr,
		"%s "OSC2FTDIDMX_VERSION"\n"
		"Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n",
		argv[0]);

	int c;
	while( (c = getopt(argc, argv, "vhdAV:P:D:S:F:U:I:O:") ) != -1)
	{
		switch(c)
		{
			case 'v':
			{
				_version();
			}	return 0;
			case 'h':
			{
				_usage(argv, &app);
			}	return 0;
			case 'd':
			{
				logp = LOG_DEBUG;
			}	break;
			case 'A':
			{
				atomic_store(&reconnect, true);
			}	break;


			case 'V':
			{
				app.vid = strtol(optarg, NULL, 16);
			} break;
			case 'P':
			{
				app.pid = strtol(optarg, NULL, 16);
			} break;
			case 'D':
			{
				app.des = optarg;
			} break;
			case 'S':
			{
				app.sid = optarg;
			} break;
			case 'F':
			{
				app.fps = strtol(optarg, NULL, 10);;
			} break;
			case 'U':
			{
				app.url = optarg;
			} break;
			case 'I':
			{
				app.priority.inp = strtol(optarg, NULL, 10);
			} break;
			case 'O':
			{
				app.priority.out = strtol(optarg, NULL, 10);
			} break;

			case '?':
			{
				if(  (optopt == 'V') || (optopt == 'P') || (optopt == 'D')
					|| (optopt == 'S') || (optopt == 'F') || (optopt == 'U')
					|| (optopt == 'I') || (optopt == 'O') )
				{
					fprintf(stderr, "Option `-%c' requires an argument.\n", optopt);
				}
				else if(isprint(optopt))
				{
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				}
				else
				{
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
			}	return -1;
			default:
			{
				// nothing
			}	return -1;
		}
	}

	signal(SIGINT, _sig);
	signal(SIGTERM, _sig);
	signal(SIGQUIT, _sig);
	signal(SIGKILL, _sig);

	openlog(NULL, LOG_PERROR, LOG_DAEMON);
	setlogmask(LOG_UPTO(logp));

	int ret = _loop(&app);

	while(atomic_load(&reconnect))
	{
		syslog(LOG_NOTICE, "[%s] preparing to reconnect", __func__);
		sleep(1);

		ret = _loop(&app);
	}

	return ret;
}
