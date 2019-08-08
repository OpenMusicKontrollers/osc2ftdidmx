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

#ifndef _OSC2FTDIDMX_H
#define _OSC2FTDIDMX_H

#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

#include <osc.lv2/reader.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _slot_t slot_t;
typedef struct _state_t state_t;

struct _slot_t {
	uint32_t mask;
	uint8_t data [32];
};

struct _state_t {
	uint16_t cur_channel;
	uint8_t cur_value;
	bool cur_set;
	LV2_OSC_Reader cur_reader;
	LV2_OSC_Arg *cur_arg;
	slot_t slots [512];
};

void
slot_set_val(slot_t *slot, uint8_t prio, uint8_t val);

void
slot_clr_val(slot_t *slot, uint8_t prio);

bool
slot_has_val(slot_t *slot);

uint8_t
slot_get_val(slot_t *slot);

extern const LV2_OSC_Tree tree_root [];

#ifdef __cplusplus
}
#endif

#endif //_OSC2FTDIDMX_H
