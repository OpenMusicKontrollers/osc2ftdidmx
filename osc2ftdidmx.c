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
#include <semaphore.h>
#include <time.h>
#include <signal.h>

#ifdef HAVE_LIBFTDI1
#	include <libftdi1/ftdi.h>
#else
#	include <ftdi.h>
#endif // HAVE_LIBFTDI1

#include <osc.lv2/stream.h>
#include <osc.lv2/reader.h>

#include <varchunk.h>

#define FTDI_VID   0x0403
#define FT232_PID  0x6001
#define FT4232_PID 0x6011
#define SEC        1000000000

typedef struct _app_t app_t;

struct _app_t {
	uint16_t vid;
	uint16_t pid;
	const char *sid;
	const char *nid;
	uint32_t fps;
	const char *url;

	LV2_OSC_Stream stream;

	struct ftdi_context ftdi;

	struct {
		varchunk_t *rx;
		varchunk_t *tx;
	} rb;

	struct {
		uint8_t start_code [1];
		uint8_t data [512];
	} __attribute__((packed)) dmx;
};

static sem_t sem;
static sig_atomic_t done = 0;

static void
_sig(int num __attribute__((unused)))
{
	done = 1;
	sem_post(&sem);
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
_handle_osc_packet(app_t *app, const uint8_t *buf, size_t len);

static void
_handle_osc_message(app_t *app, LV2_OSC_Reader *reader, size_t len)
{
	const char *path = "/dmx";
	unsigned pos = 0;
	unsigned channel = 0;

	OSC_READER_MESSAGE_FOREACH(reader, arg, len)
	{
		if(path && strcmp(arg->path, path))
		{
			return;
		}
		path = NULL;

		switch(arg->type[0])
		{
			case LV2_OSC_INT32:
			{
				switch(pos++)
				{
					case 0:
					{
						channel = arg->i & 0x1ff;
					} break;
					default:
					{
						app->dmx.data[channel++] = arg->i & 0xff;
					} break;
				}
			} break;
			default:
			{
				// ignore other types
			} break;
		}
	}
}

static void
_handle_osc_bundle(app_t *app, LV2_OSC_Reader *reader, size_t len)
{
	OSC_READER_BUNDLE_FOREACH(reader, itm, len)
	{
		_handle_osc_packet(app, itm->body, itm->size);
	}
}

static void
_handle_osc_packet(app_t *app, const uint8_t *buf, size_t len)
{
	LV2_OSC_Reader reader;
	lv2_osc_reader_initialize(&reader, buf, len);

	if(lv2_osc_reader_is_bundle(&reader))
	{
		_handle_osc_bundle(app, &reader, len);
	}
	else if(lv2_osc_reader_is_message(&reader))
	{
		_handle_osc_message(app, &reader, len);
	}
}

static int
_ftdi_xmit(app_t *app)
{
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

	return 0;

failure:
	fprintf(stderr, "[%s] '%s'\n", __func__, strerror(errno));

	return -1;
}

static int
_ftdi_init(app_t *app)
{
	app->ftdi.module_detach_mode = AUTO_DETACH_SIO_MODULE;

	if(ftdi_init(&app->ftdi) != 0)
	{
		goto failure;
	}

	if(ftdi_set_interface(&app->ftdi, INTERFACE_ANY) != 0)
	{
		goto failure;
	}

	if(app->nid && app->sid)
	{
		if(ftdi_usb_open_desc(&app->ftdi, app->vid, app->pid,
			app->nid, app->sid) != 0)
		{
			goto failure;
		}
	}
	else
	{
		if(ftdi_usb_open(&app->ftdi, app->vid, app->pid) != 0)
		{
			goto failure;
		}
	}

	if(ftdi_usb_reset(&app->ftdi) != 0)
	{
		goto failure;
	}

	if(ftdi_set_baudrate(&app->ftdi, 250000) != 0)
	{
		goto failure;
	}

	if(ftdi_set_line_property2(&app->ftdi, BITS_8, STOP_BIT_2, NONE,
		BREAK_ON) != 0)
	{
		goto failure;
	}

	if(ftdi_usb_purge_buffers(&app->ftdi) != 0)
	{
		goto failure;
	}

	if(ftdi_setflowctrl(&app->ftdi, SIO_DISABLE_FLOW_CTRL) != 0)
	{
		goto failure;
	}

	if(ftdi_setrts(&app->ftdi, 0) != 0)
	{
		goto failure;
	}

	return 0;

failure:
	fprintf(stderr, "[%s] '%s'\n", __func__, strerror(errno));

	return -1;
}

static void
_ftdi_deinit(app_t *app)
{
	if(ftdi_usb_close(&app->ftdi) != 0)
	{
		//FIXME
	}

	ftdi_deinit(&app->ftdi);
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
		"   [-V] VID                 USB vendor ID (0x%04"PRIx16")\n"
		"   [-P] PID                 USB product ID (0x%04"PRIx16")\n"
		"   [-N] NAME                USB product name (%s)\n"
		"   [-S] SID                 USB serial ID (%s)\n"
		"   [-F] FPS                 Frame rate (%"PRIu32")\n"
		"   [-U] URI                 OSC URI (%s)\n\n"
		, argv[0], app->vid, app->pid, app->nid, app->sid, app->fps, app->url);
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	static app_t app;

	app.vid = FTDI_VID;
	app.pid = FT232_PID;
	app.nid = "KMtronic DMX Interface";
	app.sid = NULL;
	app.fps = 25;
	app.url = "osc.udp://:6666";

	fprintf(stderr,
		"%s "OSC2FTDIDMX_VERSION"\n"
		"Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n",
		argv[0]);

	int c;
	while( (c = getopt(argc, argv, "vhdV:P:N:S:F:U:") ) != -1)
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
				//FIXME
			}	break;

			case 'V':
			{
				app.vid = strtol(optarg, NULL, 16);
			} break;
			case 'P':
			{
				app.pid = strtol(optarg, NULL, 16);
			} break;
			case 'N':
			{
				app.nid = optarg;
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

			case '?':
			{
				if(  (optopt == 'V') || (optopt == 'P') || (optopt == 'N')
					|| (optopt == 'S') || (optopt == 'F') || (optopt == 'U') )
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

	if(sem_init(&sem, 0, 0) == -1)
	{
		goto failure;
	}

	app.rb.rx = varchunk_new(8192, false);
	if(!app.rb.rx)
	{
		sem_destroy(&sem);
		goto failure;
	}

	app.rb.tx = varchunk_new(8192, false);
	if(!app.rb.tx)
	{
		varchunk_free(app.rb.rx);
		sem_destroy(&sem);
		goto failure;
	}

	if(_ftdi_init(&app) == -1)
	{
		varchunk_free(app.rb.tx);
		varchunk_free(app.rb.rx);
		sem_destroy(&sem);
		goto failure;
	}

	lv2_osc_stream_init(&app.stream, "osc.udp://:6666", &driver, &app);

	struct timespec t0;
	clock_gettime(CLOCK_REALTIME, &t0);
	struct timespec t1 = t0;

	const uint64_t step_poll = SEC / 1000;
	const uint64_t step_fps = SEC / app.fps;

	while(!done)
	{
		if(sem_timedwait(&sem, &t1) == -1)
		{
			if(errno != ETIMEDOUT)
			{
				continue;
			}
		}

		const LV2_OSC_Enum status = lv2_osc_stream_run(&app.stream);

		if(status & LV2_OSC_SEND)
		{
			fprintf(stdout, "[%s:%i] sent\n", __func__, __LINE__);
			//FIXME
		}

		if(status & LV2_OSC_RECV)
		{
			const uint8_t *buf;
			size_t len;
			while( (buf = varchunk_read_request(app.rb.rx, &len)) )
			{
				_handle_osc_packet(&app, buf, len);

				varchunk_read_advance(app.rb.rx);
			}
		}

		if(status & LV2_OSC_CONN)
		{
			fprintf(stdout, "[%s:%i] connected\n", __func__, __LINE__);
			//FIXME
		}

		if(status & LV2_OSC_ERR)
		{
			fprintf(stderr, "[%s:%i] %s\n", __func__, __LINE__, strerror(errno));
		}

		t1.tv_nsec += step_poll;
		while(t1.tv_nsec >= SEC)
		{
			t1.tv_sec += 1;
			t1.tv_nsec -= SEC;
		}

		const uint64_t diff = (t1.tv_sec - t0.tv_sec)*SEC + t1.tv_nsec - t0.tv_nsec;
		if(diff < step_fps)
		{
			continue;
		}

		if(_ftdi_xmit(&app) == -1)
		{
			//FIXME
		}

		t0 = t1;
	}

	sem_destroy(&sem);

	lv2_osc_stream_deinit(&app.stream);

	_ftdi_deinit(&app);

	varchunk_free(app.rb.rx);
	varchunk_free(app.rb.tx);

	return 0;

failure:
	return -1;
}
