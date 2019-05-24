# osc2ftdidmx

## OSC to FTDI DMX bridge

### Build status

[![build status](https://gitlab.com/OpenMusicKontrollers/osc2ftdidmx/badges/master/build.svg)](https://gitlab.com/OpenMusicKontrollers/osc2ftdidmx/commits/master)

### Dependencies

* [LV2](http://lv2plug.in/) (LV2 Plugin Standard)
* [libftdi](https://www.intra2net.com/en/developer/libftdi/index.php) (Library to talk to FTDI chips)

### Build / install

	git clone https://git.open-music-kontrollers.ch/lad/osc2ftdidmx
	cd osc2ftdidmx
	meson build
	cd build
	ninja
	sudo ninja install

### Usage

	osc2ftdidmx -V 0x0403 -P 0x6001 -N 'KMtronic DMX Interface' -S ABCXYZ -F 30 -U osc.udp://:6666

### License

Copyright (c) 2019 Hanspeter Portner (dev@open-music-kontrollers.ch)

This is free software: you can redistribute it and/or modify
it under the terms of the Artistic License 2.0 as published by
The Perl Foundation.

This source is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
Artistic License 2.0 for more details.

You should have received a copy of the Artistic License 2.0
along the source as a COPYING file. If not, obtain it from
<http://www.perlfoundation.org/artistic_license_2_0>.
