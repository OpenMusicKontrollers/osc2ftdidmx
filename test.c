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
	assert(_has_prio(&slot) == false);
	assert(_get_prio(&slot) == 0x0);

	// prio 0
	memset(&slot, 0x0, sizeof(slot));
	_set_prio(&slot, 0, 0x1);
	assert(_has_prio(&slot) == true);
	assert(_get_prio(&slot) == 0x1);
	_clr_prio(&slot, 0);
	assert(_has_prio(&slot) == false);
	assert(_get_prio(&slot) == 0x0);

	// prio 0, 1
	memset(&slot, 0x0, sizeof(slot));
	_set_prio(&slot, 0, 0x1);
	_set_prio(&slot, 1, 0x2);
	assert(_has_prio(&slot) == true);
	assert(_get_prio(&slot) == 0x2);
	_clr_prio(&slot, 1);
	assert(_get_prio(&slot) == 0x1);
	_clr_prio(&slot, 0);
	assert(_has_prio(&slot) == false);
	assert(_get_prio(&slot) == 0x0);

	// prio 1, 2, 3
	memset(&slot, 0x0, sizeof(slot));
	_set_prio(&slot, 1, 0x1);
	_set_prio(&slot, 2, 0x2);
	_set_prio(&slot, 3, 0x3);
	assert(_has_prio(&slot) == true);
	assert(_get_prio(&slot) == 0x3);
	_clr_prio(&slot, 1);
	assert(_get_prio(&slot) == 0x3);
	_clr_prio(&slot, 3);
	assert(_has_prio(&slot) == true);
	assert(_get_prio(&slot) == 0x2);
	_clr_prio(&slot, 2);
	assert(_has_prio(&slot) == false);
	assert(_get_prio(&slot) == 0x0);
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	_test_priorities();

	return 0;
}
