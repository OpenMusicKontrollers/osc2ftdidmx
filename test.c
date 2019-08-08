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

#include <assert.h>
#include <string.h>

#include <osc2ftdidmx.h>

static void
_test_priorities()
{
	slot_t slot;

	// empty
	memset(&slot, 0x0, sizeof(slot));
	assert(slot_has_val(&slot) == false);
	assert(slot_get_val(&slot) == 0x0);

	// prio 0
	memset(&slot, 0x0, sizeof(slot));
	slot_set_val(&slot, 0, 0x1);
	assert(slot_has_val(&slot) == true);
	assert(slot_get_val(&slot) == 0x1);
	slot_clr_val(&slot, 0);
	assert(slot_has_val(&slot) == false);
	assert(slot_get_val(&slot) == 0x0);

	// prio 0, 1
	memset(&slot, 0x0, sizeof(slot));
	slot_set_val(&slot, 0, 0x1);
	slot_set_val(&slot, 1, 0x2);
	assert(slot_has_val(&slot) == true);
	assert(slot_get_val(&slot) == 0x2);
	slot_clr_val(&slot, 1);
	assert(slot_get_val(&slot) == 0x1);
	slot_clr_val(&slot, 0);
	assert(slot_has_val(&slot) == false);
	assert(slot_get_val(&slot) == 0x0);

	// prio 1, 2, 3
	memset(&slot, 0x0, sizeof(slot));
	slot_set_val(&slot, 1, 0x1);
	slot_set_val(&slot, 2, 0x2);
	slot_set_val(&slot, 3, 0x3);
	assert(slot_has_val(&slot) == true);
	assert(slot_get_val(&slot) == 0x3);
	slot_clr_val(&slot, 1);
	assert(slot_get_val(&slot) == 0x3);
	slot_clr_val(&slot, 3);
	assert(slot_has_val(&slot) == true);
	assert(slot_get_val(&slot) == 0x2);
	slot_clr_val(&slot, 2);
	assert(slot_has_val(&slot) == false);
	assert(slot_get_val(&slot) == 0x0);
}

static void
_test_parse()
{
	state_t state;
	LV2_OSC_Reader reader;

	{
		const uint8_t msg [] = {
			'/', 'd', 'm', 'x',
			'/', '*', '/', '*',
			0x0, 0x0, 0x0, 0x0,
			',', 0x0, 0x0, 0x0
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg, sizeof(msg));

		memset(&state, 0x0, sizeof(state));
		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg));
		lv2_osc_reader_match(&reader, sizeof(msg), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			assert(slot_has_val(slot) == false);
			assert(slot_get_val(slot) == 0x0);
		}
	}

	{
		const uint8_t msg [] = {
			'/', 'd', 'm', 'x',
			'/', '*', '/', '*',
			0x0, 0x0, 0x0, 0x0,
			',', 'i', 0x0, 0x0,
			0x0, 0x0, 0x0, 0x1
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg, sizeof(msg));

		memset(&state, 0x0, sizeof(state));
		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg));
		lv2_osc_reader_match(&reader, sizeof(msg), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			assert(slot_has_val(slot) == true);
			assert(slot_get_val(slot) == 0x1);
		}
	}

	{
		const uint8_t msg1 [] = {
			'/', 'd', 'm', 'x',
			'/', '2', '/', '2',
			0x0, 0x0, 0x0, 0x0,
			',', 'i', 0x0, 0x0,
			0x0, 0x0, 0x0, 0x2
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg1, sizeof(msg1));

		memset(&state, 0x0, sizeof(state));
		state.cur_channel = 0;
		state.cur_value = 0;
		state.cur_set = false;

		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg1));
		lv2_osc_reader_match(&reader, sizeof(msg1), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			if(channel == 0x2)
			{
				assert(slot_has_val(slot) == true);
				assert(slot_get_val(slot) == 0x2);
			}
			else
			{
				assert(slot_has_val(slot) == false);
				assert(slot_get_val(slot) == 0x0);
			}
		}

		const uint8_t msg2 [] = {
			'/', 'd', 'm', 'x',
			'/', '2', '/', '3',
			0x0, 0x0, 0x0, 0x0,
			',', 'i', 0x0, 0x0,
			0x0, 0x0, 0x0, 0x3
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg2, sizeof(msg2));

		//memset(&state, 0x0, sizeof(state));
		state.cur_channel = 0;
		state.cur_value = 0;
		state.cur_set = false;

		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg2));
		lv2_osc_reader_match(&reader, sizeof(msg2), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			if(channel == 0x2)
			{
				assert(slot_has_val(slot) == true);
				assert(slot_get_val(slot) == 0x3);
			}
			else
			{
				assert(slot_has_val(slot) == false);
				assert(slot_get_val(slot) == 0x0);
			}
		}

		const uint8_t msg3 [] = {
			'/', 'd', 'm', 'x',
			'/', '2', '/', '2',
			0x0, 0x0, 0x0, 0x0,
			',', 0x0, 0x0, 0x0
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg3, sizeof(msg3));

		//memset(&state, 0x0, sizeof(state));
		state.cur_channel = 0;
		state.cur_value = 0;
		state.cur_set = false;

		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg3));
		lv2_osc_reader_match(&reader, sizeof(msg3), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			if(channel == 0x2)
			{
				assert(slot_has_val(slot) == true);
				assert(slot_get_val(slot) == 0x3);
			}
			else
			{
				assert(slot_has_val(slot) == false);
				assert(slot_get_val(slot) == 0x0);
			}
		}

		const uint8_t msg4 [] = {
			'/', 'd', 'm', 'x',
			'/', '2', '/', '3',
			0x0, 0x0, 0x0, 0x0,
			',', 0x0, 0x0, 0x0
		};

		memset(&reader, 0x0, sizeof(reader));
		lv2_osc_reader_initialize(&reader, msg4, sizeof(msg4));

		//memset(&state, 0x0, sizeof(state));
		state.cur_channel = 0;
		state.cur_value = 0;
		state.cur_set = false;

		state.cur_reader = reader;
		state.cur_arg = OSC_READER_MESSAGE_BEGIN(&state.cur_reader, sizeof(msg4));
		lv2_osc_reader_match(&reader, sizeof(msg4), tree_root, &state);

		for(unsigned channel = 0; channel < 512; channel++)
		{
			slot_t *slot = &state.slots[channel];

			if(channel == 0x2)
			{
				assert(slot_has_val(slot) == false);
				assert(slot_get_val(slot) == 0x0);
			}
			else
			{
				assert(slot_has_val(slot) == false);
				assert(slot_get_val(slot) == 0x0);
			}
		}

		//FIXME clear 3 and test
	}
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	_test_priorities();
	_test_parse();

	return 0;
}
