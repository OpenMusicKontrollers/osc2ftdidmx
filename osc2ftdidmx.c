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
#include <syslog.h>
#include <stdatomic.h>

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
#define NSECS      1000000000
#define JAN_1970   2208988800ULL

//#define FTDI_SKIP

typedef struct _sched_t sched_t;
typedef struct _slot_t slot_t;
typedef struct _app_t app_t;

struct _sched_t {
	sched_t *next;
	struct timespec to;
	size_t len;
	uint8_t buf [];
};

struct _slot_t {
	uint32_t mask;
	uint8_t data [32];
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

	uint16_t cur_channel;
	uint8_t cur_priority;
	uint8_t cur_value;
	bool cur_set;
	LV2_OSC_Reader cur_reader;
	LV2_OSC_Arg *cur_arg;

	slot_t slots [512];

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

static const LV2_OSC_Tree tree_priority [32+1];
static const LV2_OSC_Tree tree_channel [512+1];

static void
_priority (LV2_OSC_Reader *reader __attribute__((unused)),
	LV2_OSC_Arg *arg __attribute__((unused)), const LV2_OSC_Tree *tree, void *data)
{
	app_t *app = data;

	app->cur_priority = tree - tree_priority;

	if(app->cur_arg && !lv2_osc_reader_arg_is_end(&app->cur_reader, app->cur_arg))
	{
		switch(app->cur_arg->type[0])
		{
			case LV2_OSC_INT32:
			{
				app->cur_value = app->cur_arg->i & 0xff;
				app->cur_set = true; // change into setting mode
			} break;
			default:
			{
				// ignore other types
			} break;
		}

		app->cur_arg = lv2_osc_reader_arg_next(&app->cur_reader, app->cur_arg);
	}

	slot_t *slot = &app->slots[app->cur_channel];
	slot->data[app->cur_priority] = app->cur_value;

	if(app->cur_set)
	{
		slot->mask |= (1 << app->cur_priority);

		syslog(LOG_DEBUG, "[%s] SET chan: %"PRIu16" prio: %"PRIu8" val: %"PRIu8,
			__func__, app->cur_channel, app->cur_priority, app->cur_value);
	}
	else
	{
		slot->mask &= ~(1 << app->cur_priority);

		syslog(LOG_DEBUG, "[%s] CLEAR chan: %"PRIu16" prio: %"PRIu8,
			__func__, app->cur_channel, app->cur_priority);
	}
}

static void
_channel( LV2_OSC_Reader *reader __attribute__((unused)),
	LV2_OSC_Arg *arg __attribute__((unused)), const LV2_OSC_Tree *tree, void *data)
{
	app_t *app = data;

	app->cur_channel = tree - tree_channel;
}

static const LV2_OSC_Tree tree_priority [32+1] = {
	{ .name =  "0", .branch = _priority },
	{ .name =  "1", .branch = _priority },
	{ .name =  "2", .branch = _priority },
	{ .name =  "3", .branch = _priority },
	{ .name =  "4", .branch = _priority },
	{ .name =  "5", .branch = _priority },
	{ .name =  "6", .branch = _priority },
	{ .name =  "7", .branch = _priority },
	{ .name =  "8", .branch = _priority },
	{ .name =  "9", .branch = _priority },
	{ .name = "10", .branch = _priority },
	{ .name = "11", .branch = _priority },
	{ .name = "12", .branch = _priority },
	{ .name = "13", .branch = _priority },
	{ .name = "14", .branch = _priority },
	{ .name = "15", .branch = _priority },
	{ .name = "16", .branch = _priority },
	{ .name = "17", .branch = _priority },
	{ .name = "18", .branch = _priority },
	{ .name = "19", .branch = _priority },
	{ .name = "20", .branch = _priority },
	{ .name = "21", .branch = _priority },
	{ .name = "22", .branch = _priority },
	{ .name = "23", .branch = _priority },
	{ .name = "24", .branch = _priority },
	{ .name = "25", .branch = _priority },
	{ .name = "26", .branch = _priority },
	{ .name = "27", .branch = _priority },
	{ .name = "28", .branch = _priority },
	{ .name = "29", .branch = _priority },
	{ .name = "30", .branch = _priority },
	{ .name = "31", .branch = _priority },
	{ .name = NULL }
};

static const LV2_OSC_Tree tree_channel [512+1] = {
	{ .name =   "0", .branch = _channel, .trees = tree_priority },
	{ .name =   "1", .branch = _channel, .trees = tree_priority },
	{ .name =   "2", .branch = _channel, .trees = tree_priority },
	{ .name =   "3", .branch = _channel, .trees = tree_priority },
	{ .name =   "4", .branch = _channel, .trees = tree_priority },
	{ .name =   "5", .branch = _channel, .trees = tree_priority },
	{ .name =   "6", .branch = _channel, .trees = tree_priority },
	{ .name =   "7", .branch = _channel, .trees = tree_priority },
	{ .name =   "8", .branch = _channel, .trees = tree_priority },
	{ .name =   "9", .branch = _channel, .trees = tree_priority },
	{ .name =  "10", .branch = _channel, .trees = tree_priority },
	{ .name =  "11", .branch = _channel, .trees = tree_priority },
	{ .name =  "12", .branch = _channel, .trees = tree_priority },
	{ .name =  "13", .branch = _channel, .trees = tree_priority },
	{ .name =  "14", .branch = _channel, .trees = tree_priority },
	{ .name =  "15", .branch = _channel, .trees = tree_priority },
	{ .name =  "16", .branch = _channel, .trees = tree_priority },
	{ .name =  "17", .branch = _channel, .trees = tree_priority },
	{ .name =  "18", .branch = _channel, .trees = tree_priority },
	{ .name =  "19", .branch = _channel, .trees = tree_priority },
	{ .name =  "20", .branch = _channel, .trees = tree_priority },
	{ .name =  "21", .branch = _channel, .trees = tree_priority },
	{ .name =  "22", .branch = _channel, .trees = tree_priority },
	{ .name =  "23", .branch = _channel, .trees = tree_priority },
	{ .name =  "24", .branch = _channel, .trees = tree_priority },
	{ .name =  "25", .branch = _channel, .trees = tree_priority },
	{ .name =  "26", .branch = _channel, .trees = tree_priority },
	{ .name =  "27", .branch = _channel, .trees = tree_priority },
	{ .name =  "28", .branch = _channel, .trees = tree_priority },
	{ .name =  "29", .branch = _channel, .trees = tree_priority },
	{ .name =  "30", .branch = _channel, .trees = tree_priority },
	{ .name =  "31", .branch = _channel, .trees = tree_priority },
	{ .name =  "32", .branch = _channel, .trees = tree_priority },
	{ .name =  "33", .branch = _channel, .trees = tree_priority },
	{ .name =  "34", .branch = _channel, .trees = tree_priority },
	{ .name =  "35", .branch = _channel, .trees = tree_priority },
	{ .name =  "36", .branch = _channel, .trees = tree_priority },
	{ .name =  "37", .branch = _channel, .trees = tree_priority },
	{ .name =  "38", .branch = _channel, .trees = tree_priority },
	{ .name =  "39", .branch = _channel, .trees = tree_priority },
	{ .name =  "40", .branch = _channel, .trees = tree_priority },
	{ .name =  "41", .branch = _channel, .trees = tree_priority },
	{ .name =  "42", .branch = _channel, .trees = tree_priority },
	{ .name =  "43", .branch = _channel, .trees = tree_priority },
	{ .name =  "44", .branch = _channel, .trees = tree_priority },
	{ .name =  "45", .branch = _channel, .trees = tree_priority },
	{ .name =  "46", .branch = _channel, .trees = tree_priority },
	{ .name =  "47", .branch = _channel, .trees = tree_priority },
	{ .name =  "48", .branch = _channel, .trees = tree_priority },
	{ .name =  "49", .branch = _channel, .trees = tree_priority },
	{ .name =  "50", .branch = _channel, .trees = tree_priority },
	{ .name =  "51", .branch = _channel, .trees = tree_priority },
	{ .name =  "52", .branch = _channel, .trees = tree_priority },
	{ .name =  "53", .branch = _channel, .trees = tree_priority },
	{ .name =  "54", .branch = _channel, .trees = tree_priority },
	{ .name =  "55", .branch = _channel, .trees = tree_priority },
	{ .name =  "56", .branch = _channel, .trees = tree_priority },
	{ .name =  "57", .branch = _channel, .trees = tree_priority },
	{ .name =  "58", .branch = _channel, .trees = tree_priority },
	{ .name =  "59", .branch = _channel, .trees = tree_priority },
	{ .name =  "60", .branch = _channel, .trees = tree_priority },
	{ .name =  "61", .branch = _channel, .trees = tree_priority },
	{ .name =  "62", .branch = _channel, .trees = tree_priority },
	{ .name =  "63", .branch = _channel, .trees = tree_priority },
	{ .name =  "64", .branch = _channel, .trees = tree_priority },
	{ .name =  "65", .branch = _channel, .trees = tree_priority },
	{ .name =  "66", .branch = _channel, .trees = tree_priority },
	{ .name =  "67", .branch = _channel, .trees = tree_priority },
	{ .name =  "68", .branch = _channel, .trees = tree_priority },
	{ .name =  "69", .branch = _channel, .trees = tree_priority },
	{ .name =  "70", .branch = _channel, .trees = tree_priority },
	{ .name =  "71", .branch = _channel, .trees = tree_priority },
	{ .name =  "72", .branch = _channel, .trees = tree_priority },
	{ .name =  "73", .branch = _channel, .trees = tree_priority },
	{ .name =  "74", .branch = _channel, .trees = tree_priority },
	{ .name =  "75", .branch = _channel, .trees = tree_priority },
	{ .name =  "76", .branch = _channel, .trees = tree_priority },
	{ .name =  "77", .branch = _channel, .trees = tree_priority },
	{ .name =  "78", .branch = _channel, .trees = tree_priority },
	{ .name =  "79", .branch = _channel, .trees = tree_priority },
	{ .name =  "80", .branch = _channel, .trees = tree_priority },
	{ .name =  "81", .branch = _channel, .trees = tree_priority },
	{ .name =  "82", .branch = _channel, .trees = tree_priority },
	{ .name =  "83", .branch = _channel, .trees = tree_priority },
	{ .name =  "84", .branch = _channel, .trees = tree_priority },
	{ .name =  "85", .branch = _channel, .trees = tree_priority },
	{ .name =  "86", .branch = _channel, .trees = tree_priority },
	{ .name =  "87", .branch = _channel, .trees = tree_priority },
	{ .name =  "88", .branch = _channel, .trees = tree_priority },
	{ .name =  "89", .branch = _channel, .trees = tree_priority },
	{ .name =  "90", .branch = _channel, .trees = tree_priority },
	{ .name =  "91", .branch = _channel, .trees = tree_priority },
	{ .name =  "92", .branch = _channel, .trees = tree_priority },
	{ .name =  "93", .branch = _channel, .trees = tree_priority },
	{ .name =  "94", .branch = _channel, .trees = tree_priority },
	{ .name =  "95", .branch = _channel, .trees = tree_priority },
	{ .name =  "96", .branch = _channel, .trees = tree_priority },
	{ .name =  "97", .branch = _channel, .trees = tree_priority },
	{ .name =  "98", .branch = _channel, .trees = tree_priority },
	{ .name =  "99", .branch = _channel, .trees = tree_priority },
	{ .name = "100", .branch = _channel, .trees = tree_priority },
	{ .name = "101", .branch = _channel, .trees = tree_priority },
	{ .name = "102", .branch = _channel, .trees = tree_priority },
	{ .name = "103", .branch = _channel, .trees = tree_priority },
	{ .name = "104", .branch = _channel, .trees = tree_priority },
	{ .name = "105", .branch = _channel, .trees = tree_priority },
	{ .name = "106", .branch = _channel, .trees = tree_priority },
	{ .name = "107", .branch = _channel, .trees = tree_priority },
	{ .name = "108", .branch = _channel, .trees = tree_priority },
	{ .name = "109", .branch = _channel, .trees = tree_priority },
	{ .name = "110", .branch = _channel, .trees = tree_priority },
	{ .name = "111", .branch = _channel, .trees = tree_priority },
	{ .name = "112", .branch = _channel, .trees = tree_priority },
	{ .name = "113", .branch = _channel, .trees = tree_priority },
	{ .name = "114", .branch = _channel, .trees = tree_priority },
	{ .name = "115", .branch = _channel, .trees = tree_priority },
	{ .name = "116", .branch = _channel, .trees = tree_priority },
	{ .name = "117", .branch = _channel, .trees = tree_priority },
	{ .name = "118", .branch = _channel, .trees = tree_priority },
	{ .name = "119", .branch = _channel, .trees = tree_priority },
	{ .name = "120", .branch = _channel, .trees = tree_priority },
	{ .name = "121", .branch = _channel, .trees = tree_priority },
	{ .name = "122", .branch = _channel, .trees = tree_priority },
	{ .name = "123", .branch = _channel, .trees = tree_priority },
	{ .name = "124", .branch = _channel, .trees = tree_priority },
	{ .name = "125", .branch = _channel, .trees = tree_priority },
	{ .name = "126", .branch = _channel, .trees = tree_priority },
	{ .name = "127", .branch = _channel, .trees = tree_priority },
	{ .name = "128", .branch = _channel, .trees = tree_priority },
	{ .name = "129", .branch = _channel, .trees = tree_priority },
	{ .name = "130", .branch = _channel, .trees = tree_priority },
	{ .name = "131", .branch = _channel, .trees = tree_priority },
	{ .name = "132", .branch = _channel, .trees = tree_priority },
	{ .name = "133", .branch = _channel, .trees = tree_priority },
	{ .name = "134", .branch = _channel, .trees = tree_priority },
	{ .name = "135", .branch = _channel, .trees = tree_priority },
	{ .name = "136", .branch = _channel, .trees = tree_priority },
	{ .name = "137", .branch = _channel, .trees = tree_priority },
	{ .name = "138", .branch = _channel, .trees = tree_priority },
	{ .name = "139", .branch = _channel, .trees = tree_priority },
	{ .name = "140", .branch = _channel, .trees = tree_priority },
	{ .name = "141", .branch = _channel, .trees = tree_priority },
	{ .name = "142", .branch = _channel, .trees = tree_priority },
	{ .name = "143", .branch = _channel, .trees = tree_priority },
	{ .name = "144", .branch = _channel, .trees = tree_priority },
	{ .name = "145", .branch = _channel, .trees = tree_priority },
	{ .name = "146", .branch = _channel, .trees = tree_priority },
	{ .name = "147", .branch = _channel, .trees = tree_priority },
	{ .name = "148", .branch = _channel, .trees = tree_priority },
	{ .name = "149", .branch = _channel, .trees = tree_priority },
	{ .name = "150", .branch = _channel, .trees = tree_priority },
	{ .name = "151", .branch = _channel, .trees = tree_priority },
	{ .name = "152", .branch = _channel, .trees = tree_priority },
	{ .name = "153", .branch = _channel, .trees = tree_priority },
	{ .name = "154", .branch = _channel, .trees = tree_priority },
	{ .name = "155", .branch = _channel, .trees = tree_priority },
	{ .name = "156", .branch = _channel, .trees = tree_priority },
	{ .name = "157", .branch = _channel, .trees = tree_priority },
	{ .name = "158", .branch = _channel, .trees = tree_priority },
	{ .name = "159", .branch = _channel, .trees = tree_priority },
	{ .name = "160", .branch = _channel, .trees = tree_priority },
	{ .name = "161", .branch = _channel, .trees = tree_priority },
	{ .name = "162", .branch = _channel, .trees = tree_priority },
	{ .name = "163", .branch = _channel, .trees = tree_priority },
	{ .name = "164", .branch = _channel, .trees = tree_priority },
	{ .name = "165", .branch = _channel, .trees = tree_priority },
	{ .name = "166", .branch = _channel, .trees = tree_priority },
	{ .name = "167", .branch = _channel, .trees = tree_priority },
	{ .name = "168", .branch = _channel, .trees = tree_priority },
	{ .name = "169", .branch = _channel, .trees = tree_priority },
	{ .name = "170", .branch = _channel, .trees = tree_priority },
	{ .name = "171", .branch = _channel, .trees = tree_priority },
	{ .name = "172", .branch = _channel, .trees = tree_priority },
	{ .name = "173", .branch = _channel, .trees = tree_priority },
	{ .name = "174", .branch = _channel, .trees = tree_priority },
	{ .name = "175", .branch = _channel, .trees = tree_priority },
	{ .name = "176", .branch = _channel, .trees = tree_priority },
	{ .name = "177", .branch = _channel, .trees = tree_priority },
	{ .name = "178", .branch = _channel, .trees = tree_priority },
	{ .name = "179", .branch = _channel, .trees = tree_priority },
	{ .name = "180", .branch = _channel, .trees = tree_priority },
	{ .name = "181", .branch = _channel, .trees = tree_priority },
	{ .name = "182", .branch = _channel, .trees = tree_priority },
	{ .name = "183", .branch = _channel, .trees = tree_priority },
	{ .name = "184", .branch = _channel, .trees = tree_priority },
	{ .name = "185", .branch = _channel, .trees = tree_priority },
	{ .name = "186", .branch = _channel, .trees = tree_priority },
	{ .name = "187", .branch = _channel, .trees = tree_priority },
	{ .name = "188", .branch = _channel, .trees = tree_priority },
	{ .name = "189", .branch = _channel, .trees = tree_priority },
	{ .name = "190", .branch = _channel, .trees = tree_priority },
	{ .name = "191", .branch = _channel, .trees = tree_priority },
	{ .name = "192", .branch = _channel, .trees = tree_priority },
	{ .name = "193", .branch = _channel, .trees = tree_priority },
	{ .name = "194", .branch = _channel, .trees = tree_priority },
	{ .name = "195", .branch = _channel, .trees = tree_priority },
	{ .name = "196", .branch = _channel, .trees = tree_priority },
	{ .name = "197", .branch = _channel, .trees = tree_priority },
	{ .name = "198", .branch = _channel, .trees = tree_priority },
	{ .name = "199", .branch = _channel, .trees = tree_priority },
	{ .name = "200", .branch = _channel, .trees = tree_priority },
	{ .name = "201", .branch = _channel, .trees = tree_priority },
	{ .name = "202", .branch = _channel, .trees = tree_priority },
	{ .name = "203", .branch = _channel, .trees = tree_priority },
	{ .name = "204", .branch = _channel, .trees = tree_priority },
	{ .name = "205", .branch = _channel, .trees = tree_priority },
	{ .name = "206", .branch = _channel, .trees = tree_priority },
	{ .name = "207", .branch = _channel, .trees = tree_priority },
	{ .name = "208", .branch = _channel, .trees = tree_priority },
	{ .name = "209", .branch = _channel, .trees = tree_priority },
	{ .name = "210", .branch = _channel, .trees = tree_priority },
	{ .name = "211", .branch = _channel, .trees = tree_priority },
	{ .name = "212", .branch = _channel, .trees = tree_priority },
	{ .name = "213", .branch = _channel, .trees = tree_priority },
	{ .name = "214", .branch = _channel, .trees = tree_priority },
	{ .name = "215", .branch = _channel, .trees = tree_priority },
	{ .name = "216", .branch = _channel, .trees = tree_priority },
	{ .name = "217", .branch = _channel, .trees = tree_priority },
	{ .name = "218", .branch = _channel, .trees = tree_priority },
	{ .name = "219", .branch = _channel, .trees = tree_priority },
	{ .name = "220", .branch = _channel, .trees = tree_priority },
	{ .name = "221", .branch = _channel, .trees = tree_priority },
	{ .name = "222", .branch = _channel, .trees = tree_priority },
	{ .name = "223", .branch = _channel, .trees = tree_priority },
	{ .name = "224", .branch = _channel, .trees = tree_priority },
	{ .name = "225", .branch = _channel, .trees = tree_priority },
	{ .name = "226", .branch = _channel, .trees = tree_priority },
	{ .name = "227", .branch = _channel, .trees = tree_priority },
	{ .name = "228", .branch = _channel, .trees = tree_priority },
	{ .name = "229", .branch = _channel, .trees = tree_priority },
	{ .name = "230", .branch = _channel, .trees = tree_priority },
	{ .name = "231", .branch = _channel, .trees = tree_priority },
	{ .name = "232", .branch = _channel, .trees = tree_priority },
	{ .name = "233", .branch = _channel, .trees = tree_priority },
	{ .name = "234", .branch = _channel, .trees = tree_priority },
	{ .name = "235", .branch = _channel, .trees = tree_priority },
	{ .name = "236", .branch = _channel, .trees = tree_priority },
	{ .name = "237", .branch = _channel, .trees = tree_priority },
	{ .name = "238", .branch = _channel, .trees = tree_priority },
	{ .name = "239", .branch = _channel, .trees = tree_priority },
	{ .name = "240", .branch = _channel, .trees = tree_priority },
	{ .name = "241", .branch = _channel, .trees = tree_priority },
	{ .name = "242", .branch = _channel, .trees = tree_priority },
	{ .name = "243", .branch = _channel, .trees = tree_priority },
	{ .name = "244", .branch = _channel, .trees = tree_priority },
	{ .name = "245", .branch = _channel, .trees = tree_priority },
	{ .name = "246", .branch = _channel, .trees = tree_priority },
	{ .name = "247", .branch = _channel, .trees = tree_priority },
	{ .name = "248", .branch = _channel, .trees = tree_priority },
	{ .name = "249", .branch = _channel, .trees = tree_priority },
	{ .name = "250", .branch = _channel, .trees = tree_priority },
	{ .name = "251", .branch = _channel, .trees = tree_priority },
	{ .name = "252", .branch = _channel, .trees = tree_priority },
	{ .name = "253", .branch = _channel, .trees = tree_priority },
	{ .name = "254", .branch = _channel, .trees = tree_priority },
	{ .name = "255", .branch = _channel, .trees = tree_priority },
	{ .name = "256", .branch = _channel, .trees = tree_priority },
	{ .name = "257", .branch = _channel, .trees = tree_priority },
	{ .name = "258", .branch = _channel, .trees = tree_priority },
	{ .name = "259", .branch = _channel, .trees = tree_priority },
	{ .name = "260", .branch = _channel, .trees = tree_priority },
	{ .name = "261", .branch = _channel, .trees = tree_priority },
	{ .name = "262", .branch = _channel, .trees = tree_priority },
	{ .name = "263", .branch = _channel, .trees = tree_priority },
	{ .name = "264", .branch = _channel, .trees = tree_priority },
	{ .name = "265", .branch = _channel, .trees = tree_priority },
	{ .name = "266", .branch = _channel, .trees = tree_priority },
	{ .name = "267", .branch = _channel, .trees = tree_priority },
	{ .name = "268", .branch = _channel, .trees = tree_priority },
	{ .name = "269", .branch = _channel, .trees = tree_priority },
	{ .name = "270", .branch = _channel, .trees = tree_priority },
	{ .name = "271", .branch = _channel, .trees = tree_priority },
	{ .name = "272", .branch = _channel, .trees = tree_priority },
	{ .name = "273", .branch = _channel, .trees = tree_priority },
	{ .name = "274", .branch = _channel, .trees = tree_priority },
	{ .name = "275", .branch = _channel, .trees = tree_priority },
	{ .name = "276", .branch = _channel, .trees = tree_priority },
	{ .name = "277", .branch = _channel, .trees = tree_priority },
	{ .name = "278", .branch = _channel, .trees = tree_priority },
	{ .name = "279", .branch = _channel, .trees = tree_priority },
	{ .name = "280", .branch = _channel, .trees = tree_priority },
	{ .name = "281", .branch = _channel, .trees = tree_priority },
	{ .name = "282", .branch = _channel, .trees = tree_priority },
	{ .name = "283", .branch = _channel, .trees = tree_priority },
	{ .name = "284", .branch = _channel, .trees = tree_priority },
	{ .name = "285", .branch = _channel, .trees = tree_priority },
	{ .name = "286", .branch = _channel, .trees = tree_priority },
	{ .name = "287", .branch = _channel, .trees = tree_priority },
	{ .name = "288", .branch = _channel, .trees = tree_priority },
	{ .name = "289", .branch = _channel, .trees = tree_priority },
	{ .name = "290", .branch = _channel, .trees = tree_priority },
	{ .name = "291", .branch = _channel, .trees = tree_priority },
	{ .name = "292", .branch = _channel, .trees = tree_priority },
	{ .name = "293", .branch = _channel, .trees = tree_priority },
	{ .name = "294", .branch = _channel, .trees = tree_priority },
	{ .name = "295", .branch = _channel, .trees = tree_priority },
	{ .name = "296", .branch = _channel, .trees = tree_priority },
	{ .name = "297", .branch = _channel, .trees = tree_priority },
	{ .name = "298", .branch = _channel, .trees = tree_priority },
	{ .name = "299", .branch = _channel, .trees = tree_priority },
	{ .name = "300", .branch = _channel, .trees = tree_priority },
	{ .name = "301", .branch = _channel, .trees = tree_priority },
	{ .name = "302", .branch = _channel, .trees = tree_priority },
	{ .name = "303", .branch = _channel, .trees = tree_priority },
	{ .name = "304", .branch = _channel, .trees = tree_priority },
	{ .name = "305", .branch = _channel, .trees = tree_priority },
	{ .name = "306", .branch = _channel, .trees = tree_priority },
	{ .name = "307", .branch = _channel, .trees = tree_priority },
	{ .name = "308", .branch = _channel, .trees = tree_priority },
	{ .name = "309", .branch = _channel, .trees = tree_priority },
	{ .name = "310", .branch = _channel, .trees = tree_priority },
	{ .name = "311", .branch = _channel, .trees = tree_priority },
	{ .name = "312", .branch = _channel, .trees = tree_priority },
	{ .name = "313", .branch = _channel, .trees = tree_priority },
	{ .name = "314", .branch = _channel, .trees = tree_priority },
	{ .name = "315", .branch = _channel, .trees = tree_priority },
	{ .name = "316", .branch = _channel, .trees = tree_priority },
	{ .name = "317", .branch = _channel, .trees = tree_priority },
	{ .name = "318", .branch = _channel, .trees = tree_priority },
	{ .name = "319", .branch = _channel, .trees = tree_priority },
	{ .name = "320", .branch = _channel, .trees = tree_priority },
	{ .name = "321", .branch = _channel, .trees = tree_priority },
	{ .name = "322", .branch = _channel, .trees = tree_priority },
	{ .name = "323", .branch = _channel, .trees = tree_priority },
	{ .name = "324", .branch = _channel, .trees = tree_priority },
	{ .name = "325", .branch = _channel, .trees = tree_priority },
	{ .name = "326", .branch = _channel, .trees = tree_priority },
	{ .name = "327", .branch = _channel, .trees = tree_priority },
	{ .name = "328", .branch = _channel, .trees = tree_priority },
	{ .name = "329", .branch = _channel, .trees = tree_priority },
	{ .name = "330", .branch = _channel, .trees = tree_priority },
	{ .name = "331", .branch = _channel, .trees = tree_priority },
	{ .name = "332", .branch = _channel, .trees = tree_priority },
	{ .name = "333", .branch = _channel, .trees = tree_priority },
	{ .name = "334", .branch = _channel, .trees = tree_priority },
	{ .name = "335", .branch = _channel, .trees = tree_priority },
	{ .name = "336", .branch = _channel, .trees = tree_priority },
	{ .name = "337", .branch = _channel, .trees = tree_priority },
	{ .name = "338", .branch = _channel, .trees = tree_priority },
	{ .name = "339", .branch = _channel, .trees = tree_priority },
	{ .name = "340", .branch = _channel, .trees = tree_priority },
	{ .name = "341", .branch = _channel, .trees = tree_priority },
	{ .name = "342", .branch = _channel, .trees = tree_priority },
	{ .name = "343", .branch = _channel, .trees = tree_priority },
	{ .name = "344", .branch = _channel, .trees = tree_priority },
	{ .name = "345", .branch = _channel, .trees = tree_priority },
	{ .name = "346", .branch = _channel, .trees = tree_priority },
	{ .name = "347", .branch = _channel, .trees = tree_priority },
	{ .name = "348", .branch = _channel, .trees = tree_priority },
	{ .name = "349", .branch = _channel, .trees = tree_priority },
	{ .name = "350", .branch = _channel, .trees = tree_priority },
	{ .name = "351", .branch = _channel, .trees = tree_priority },
	{ .name = "352", .branch = _channel, .trees = tree_priority },
	{ .name = "353", .branch = _channel, .trees = tree_priority },
	{ .name = "354", .branch = _channel, .trees = tree_priority },
	{ .name = "355", .branch = _channel, .trees = tree_priority },
	{ .name = "356", .branch = _channel, .trees = tree_priority },
	{ .name = "357", .branch = _channel, .trees = tree_priority },
	{ .name = "358", .branch = _channel, .trees = tree_priority },
	{ .name = "359", .branch = _channel, .trees = tree_priority },
	{ .name = "360", .branch = _channel, .trees = tree_priority },
	{ .name = "361", .branch = _channel, .trees = tree_priority },
	{ .name = "362", .branch = _channel, .trees = tree_priority },
	{ .name = "363", .branch = _channel, .trees = tree_priority },
	{ .name = "364", .branch = _channel, .trees = tree_priority },
	{ .name = "365", .branch = _channel, .trees = tree_priority },
	{ .name = "366", .branch = _channel, .trees = tree_priority },
	{ .name = "367", .branch = _channel, .trees = tree_priority },
	{ .name = "368", .branch = _channel, .trees = tree_priority },
	{ .name = "369", .branch = _channel, .trees = tree_priority },
	{ .name = "370", .branch = _channel, .trees = tree_priority },
	{ .name = "371", .branch = _channel, .trees = tree_priority },
	{ .name = "372", .branch = _channel, .trees = tree_priority },
	{ .name = "373", .branch = _channel, .trees = tree_priority },
	{ .name = "374", .branch = _channel, .trees = tree_priority },
	{ .name = "375", .branch = _channel, .trees = tree_priority },
	{ .name = "376", .branch = _channel, .trees = tree_priority },
	{ .name = "377", .branch = _channel, .trees = tree_priority },
	{ .name = "378", .branch = _channel, .trees = tree_priority },
	{ .name = "379", .branch = _channel, .trees = tree_priority },
	{ .name = "380", .branch = _channel, .trees = tree_priority },
	{ .name = "381", .branch = _channel, .trees = tree_priority },
	{ .name = "382", .branch = _channel, .trees = tree_priority },
	{ .name = "383", .branch = _channel, .trees = tree_priority },
	{ .name = "384", .branch = _channel, .trees = tree_priority },
	{ .name = "385", .branch = _channel, .trees = tree_priority },
	{ .name = "386", .branch = _channel, .trees = tree_priority },
	{ .name = "387", .branch = _channel, .trees = tree_priority },
	{ .name = "388", .branch = _channel, .trees = tree_priority },
	{ .name = "389", .branch = _channel, .trees = tree_priority },
	{ .name = "390", .branch = _channel, .trees = tree_priority },
	{ .name = "391", .branch = _channel, .trees = tree_priority },
	{ .name = "392", .branch = _channel, .trees = tree_priority },
	{ .name = "393", .branch = _channel, .trees = tree_priority },
	{ .name = "394", .branch = _channel, .trees = tree_priority },
	{ .name = "395", .branch = _channel, .trees = tree_priority },
	{ .name = "396", .branch = _channel, .trees = tree_priority },
	{ .name = "397", .branch = _channel, .trees = tree_priority },
	{ .name = "398", .branch = _channel, .trees = tree_priority },
	{ .name = "399", .branch = _channel, .trees = tree_priority },
	{ .name = "400", .branch = _channel, .trees = tree_priority },
	{ .name = "401", .branch = _channel, .trees = tree_priority },
	{ .name = "402", .branch = _channel, .trees = tree_priority },
	{ .name = "403", .branch = _channel, .trees = tree_priority },
	{ .name = "404", .branch = _channel, .trees = tree_priority },
	{ .name = "405", .branch = _channel, .trees = tree_priority },
	{ .name = "406", .branch = _channel, .trees = tree_priority },
	{ .name = "407", .branch = _channel, .trees = tree_priority },
	{ .name = "408", .branch = _channel, .trees = tree_priority },
	{ .name = "409", .branch = _channel, .trees = tree_priority },
	{ .name = "410", .branch = _channel, .trees = tree_priority },
	{ .name = "411", .branch = _channel, .trees = tree_priority },
	{ .name = "412", .branch = _channel, .trees = tree_priority },
	{ .name = "413", .branch = _channel, .trees = tree_priority },
	{ .name = "414", .branch = _channel, .trees = tree_priority },
	{ .name = "415", .branch = _channel, .trees = tree_priority },
	{ .name = "416", .branch = _channel, .trees = tree_priority },
	{ .name = "417", .branch = _channel, .trees = tree_priority },
	{ .name = "418", .branch = _channel, .trees = tree_priority },
	{ .name = "419", .branch = _channel, .trees = tree_priority },
	{ .name = "420", .branch = _channel, .trees = tree_priority },
	{ .name = "421", .branch = _channel, .trees = tree_priority },
	{ .name = "422", .branch = _channel, .trees = tree_priority },
	{ .name = "423", .branch = _channel, .trees = tree_priority },
	{ .name = "424", .branch = _channel, .trees = tree_priority },
	{ .name = "425", .branch = _channel, .trees = tree_priority },
	{ .name = "426", .branch = _channel, .trees = tree_priority },
	{ .name = "427", .branch = _channel, .trees = tree_priority },
	{ .name = "428", .branch = _channel, .trees = tree_priority },
	{ .name = "429", .branch = _channel, .trees = tree_priority },
	{ .name = "430", .branch = _channel, .trees = tree_priority },
	{ .name = "431", .branch = _channel, .trees = tree_priority },
	{ .name = "432", .branch = _channel, .trees = tree_priority },
	{ .name = "433", .branch = _channel, .trees = tree_priority },
	{ .name = "434", .branch = _channel, .trees = tree_priority },
	{ .name = "435", .branch = _channel, .trees = tree_priority },
	{ .name = "436", .branch = _channel, .trees = tree_priority },
	{ .name = "437", .branch = _channel, .trees = tree_priority },
	{ .name = "438", .branch = _channel, .trees = tree_priority },
	{ .name = "439", .branch = _channel, .trees = tree_priority },
	{ .name = "440", .branch = _channel, .trees = tree_priority },
	{ .name = "441", .branch = _channel, .trees = tree_priority },
	{ .name = "442", .branch = _channel, .trees = tree_priority },
	{ .name = "443", .branch = _channel, .trees = tree_priority },
	{ .name = "444", .branch = _channel, .trees = tree_priority },
	{ .name = "445", .branch = _channel, .trees = tree_priority },
	{ .name = "446", .branch = _channel, .trees = tree_priority },
	{ .name = "447", .branch = _channel, .trees = tree_priority },
	{ .name = "448", .branch = _channel, .trees = tree_priority },
	{ .name = "449", .branch = _channel, .trees = tree_priority },
	{ .name = "450", .branch = _channel, .trees = tree_priority },
	{ .name = "451", .branch = _channel, .trees = tree_priority },
	{ .name = "452", .branch = _channel, .trees = tree_priority },
	{ .name = "453", .branch = _channel, .trees = tree_priority },
	{ .name = "454", .branch = _channel, .trees = tree_priority },
	{ .name = "455", .branch = _channel, .trees = tree_priority },
	{ .name = "456", .branch = _channel, .trees = tree_priority },
	{ .name = "457", .branch = _channel, .trees = tree_priority },
	{ .name = "458", .branch = _channel, .trees = tree_priority },
	{ .name = "459", .branch = _channel, .trees = tree_priority },
	{ .name = "460", .branch = _channel, .trees = tree_priority },
	{ .name = "461", .branch = _channel, .trees = tree_priority },
	{ .name = "462", .branch = _channel, .trees = tree_priority },
	{ .name = "463", .branch = _channel, .trees = tree_priority },
	{ .name = "464", .branch = _channel, .trees = tree_priority },
	{ .name = "465", .branch = _channel, .trees = tree_priority },
	{ .name = "466", .branch = _channel, .trees = tree_priority },
	{ .name = "467", .branch = _channel, .trees = tree_priority },
	{ .name = "468", .branch = _channel, .trees = tree_priority },
	{ .name = "469", .branch = _channel, .trees = tree_priority },
	{ .name = "470", .branch = _channel, .trees = tree_priority },
	{ .name = "471", .branch = _channel, .trees = tree_priority },
	{ .name = "472", .branch = _channel, .trees = tree_priority },
	{ .name = "473", .branch = _channel, .trees = tree_priority },
	{ .name = "474", .branch = _channel, .trees = tree_priority },
	{ .name = "475", .branch = _channel, .trees = tree_priority },
	{ .name = "476", .branch = _channel, .trees = tree_priority },
	{ .name = "477", .branch = _channel, .trees = tree_priority },
	{ .name = "478", .branch = _channel, .trees = tree_priority },
	{ .name = "479", .branch = _channel, .trees = tree_priority },
	{ .name = "480", .branch = _channel, .trees = tree_priority },
	{ .name = "481", .branch = _channel, .trees = tree_priority },
	{ .name = "482", .branch = _channel, .trees = tree_priority },
	{ .name = "483", .branch = _channel, .trees = tree_priority },
	{ .name = "484", .branch = _channel, .trees = tree_priority },
	{ .name = "485", .branch = _channel, .trees = tree_priority },
	{ .name = "486", .branch = _channel, .trees = tree_priority },
	{ .name = "487", .branch = _channel, .trees = tree_priority },
	{ .name = "488", .branch = _channel, .trees = tree_priority },
	{ .name = "489", .branch = _channel, .trees = tree_priority },
	{ .name = "490", .branch = _channel, .trees = tree_priority },
	{ .name = "491", .branch = _channel, .trees = tree_priority },
	{ .name = "492", .branch = _channel, .trees = tree_priority },
	{ .name = "493", .branch = _channel, .trees = tree_priority },
	{ .name = "494", .branch = _channel, .trees = tree_priority },
	{ .name = "495", .branch = _channel, .trees = tree_priority },
	{ .name = "496", .branch = _channel, .trees = tree_priority },
	{ .name = "497", .branch = _channel, .trees = tree_priority },
	{ .name = "498", .branch = _channel, .trees = tree_priority },
	{ .name = "499", .branch = _channel, .trees = tree_priority },
	{ .name = "500", .branch = _channel, .trees = tree_priority },
	{ .name = "501", .branch = _channel, .trees = tree_priority },
	{ .name = "502", .branch = _channel, .trees = tree_priority },
	{ .name = "503", .branch = _channel, .trees = tree_priority },
	{ .name = "504", .branch = _channel, .trees = tree_priority },
	{ .name = "505", .branch = _channel, .trees = tree_priority },
	{ .name = "506", .branch = _channel, .trees = tree_priority },
	{ .name = "507", .branch = _channel, .trees = tree_priority },
	{ .name = "508", .branch = _channel, .trees = tree_priority },
	{ .name = "509", .branch = _channel, .trees = tree_priority },
	{ .name = "510", .branch = _channel, .trees = tree_priority },
	{ .name = "511", .branch = _channel, .trees = tree_priority },
	{ .name = NULL }
};

static LV2_OSC_Tree tree_root [1+1] = {
	{ .name = "dmx", .trees = tree_channel },
	{ .name = NULL }
};

static void
_handle_osc_packet(app_t *app, uint64_t timetag, const uint8_t *buf, size_t len);

static void
_handle_osc_message(app_t *app __attribute__((unused)), LV2_OSC_Reader *reader,
	size_t len)
{
	(void)lv2_osc_hooks; //FIXME

	app->cur_channel = 0;
	app->cur_priority = 0;
	app->cur_value = 0;
	app->cur_set = false;

	app->cur_reader = *reader;
	app->cur_arg = OSC_READER_MESSAGE_BEGIN(&app->cur_reader, len);

	lv2_osc_reader_match(reader, len, tree_root, app);
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
			slot_t *slot = &app->slots[i];

			if(slot->mask == 0x0)
			{
				app->dmx.data[i] = 0x0;
				continue;
			}

			// find highest priority value
			for(uint32_t j = 0, mask = 0x80000000; j < 32; j++, mask >>= 1)
			{
				if(slot->mask & mask)
				{
					app->dmx.data[i] = slot->data[32-1-j];
					break;
				}
			}
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
