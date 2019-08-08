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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _slot_t slot_t;

struct _slot_t {
	uint32_t mask;
	uint8_t data [32];
};

static inline void
_set_prio(slot_t *slot, uint8_t prio, uint8_t val)
{
		slot->mask |= (1 << prio);
		slot->data[prio] = val;
}

static inline void
_clr_prio(slot_t *slot, uint8_t prio)
{
		slot->mask &= ~(1 << prio);
}

static inline bool
_has_prio(slot_t *slot)
{
	return (slot->mask != 0x0);
}

static inline uint8_t
_get_prio(slot_t *slot)
{
	if(_has_prio(slot))
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

#ifdef __cplusplus
}
#endif

#endif //_OSC2FTDIDMX_H
