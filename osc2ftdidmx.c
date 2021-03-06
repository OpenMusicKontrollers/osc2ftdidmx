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

#include <osc2ftdidmx.h>

static const LV2_OSC_Tree tree_priority [32+1];
static const LV2_OSC_Tree tree_channel [512+1];

void
slot_set_val(slot_t *slot, uint8_t prio, uint8_t val)
{
		slot->mask |= (1 << prio);
		slot->data[prio] = val;
}

void
slot_clr_val(slot_t *slot, uint8_t prio)
{
		slot->mask &= ~(1 << prio);
		slot->data[prio] = 0x0;
}

bool
slot_has_val(slot_t *slot)
{
	return (slot->mask != 0x0);
}

uint8_t
slot_get_val(slot_t *slot)
{
	if(slot_has_val(slot))
	{
		for(uint32_t j = 0, mask = 0x80000000; j < 32; j++, mask >>= 1)
		{
			if(slot->mask & mask)
			{
				const uint8_t prio = 32 - 1 - j;

				return slot->data[prio];
			}
		}
	}

	return 0x0;
}

static void
_priority (LV2_OSC_Reader *reader __attribute__((unused)),
	LV2_OSC_Arg *arg __attribute__((unused)), const LV2_OSC_Tree *tree, void *data)
{
	state_t *state = data;

	const uint8_t prio = tree - tree_priority;

	if(state->cur_arg && !lv2_osc_reader_arg_is_end(&state->cur_reader, state->cur_arg))
	{
		switch(state->cur_arg->type[0])
		{
			case LV2_OSC_INT32:
			{
				state->cur_value = state->cur_arg->i & 0xff;
				state->cur_set = true; // change into setting mode
			} break;
			default:
			{
				// ignore other types
			} break;
		}

		state->cur_arg = lv2_osc_reader_arg_next(&state->cur_reader, state->cur_arg);
	}

	slot_t *slot = &state->slots[state->cur_channel];

	fprintf(stderr, "%u %u\n", state->cur_channel, prio);

	if(state->cur_set)
	{
		slot_set_val(slot, prio, state->cur_value);

		syslog(LOG_DEBUG, "[%s] SET chan: %"PRIu16" prio: %"PRIu8" val: %"PRIu8,
			__func__, state->cur_channel, prio, state->cur_value);
	}
	else
	{
		slot_clr_val(slot, prio);

		syslog(LOG_DEBUG, "[%s] CLEAR chan: %"PRIu16" prio: %"PRIu8,
			__func__, state->cur_channel, prio);
	}
}

static void
_channel( LV2_OSC_Reader *reader __attribute__((unused)),
	LV2_OSC_Arg *arg __attribute__((unused)), const LV2_OSC_Tree *tree, void *data)
{
	state_t *state = data;

	state->cur_channel = tree - tree_channel;
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

const LV2_OSC_Tree tree_root [1+1] = {
	{ .name = "dmx", .trees = tree_channel },
	{ .name = NULL }
};
