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

### Compatibility

Should be compatible with any FTDI-based USB-DMX adapter, e.g

* [ENTTEC OPEN DMX USB](https://www.enttec.co.uk/en/product/controls/dmx-usb-interfaces/open-dmx-usb/)
* [KMtronic DMX Adapter](https://info.kmtronic.com/kmtronic-dmx-adapter.html)
* [SOH Module USB - DMX 512](http://eshop.soh.cz/en/light-control/i110-module-usb-dmx-512)
* [KWMATIK USB dmx 512 ](https://kwmatik.blogspot.com/2013/06/jak-podaczac-urzadzenia-dmx512-do-czego.html)

### Usage

#### Discover your type of FTDI-DMX device via e.g. dmesg when plugging it in

	[user@machine ~] dmesg
	[  132.718684] usb 1-1.4: New USB device found, idVendor=0403, idProduct=6001, bcdDevice= 6.00
	[  132.718689] usb 1-1.4: New USB device strings: Mfr=1, Product=2, SerialNumber=3
	[  132.718691] usb 1-1.4: Product: KMtronic DMX Interface
	[  132.718693] usb 1-1.4: Manufacturer: KMtronic
	[  132.718695] usb 1-1.4: SerialNumber: ABCXYZ

#### Run osc2ftdidmx with the information gathered above

	osc2ftdidmx \
		-V 0x0403 \                   # USB idVendor
		-P 0x6001 \                   # USB idProduct
		-D 'KMtronic DMX Interface' \ # USB product description
		-S ABCXYZ \                   # USB product serial number
		-F 30 \                       # update rate in frames per second
		-U osc.udp://:6666            # OSC server URI

#### Control osc2ftdidmx with your favorite OSC client

osc2ftdidmx supports 32 priority levels per channel. If priority level 0 on a
given channel is set, and now priority level 1 gets set, the latter takes
precedence. When priority level 1 is cleared, value is taken from priority
level 0 again. If no priority level is set on a given channel, the channel's
value is assumed to be 0.

##### **/dmx/[0-511]/[0-31] {i}+ [0-255]+**

To set channels, send your OSC messages to given OSC path with
**i**nteger argument(s) being subsequent channel/priority values.

	# set channel 0, priority 0 to value 255
	oscsend osc.udp://localhost:6666 /dmx/0/0 i 255

	# set channel 12, priority 31 to value 127
	oscsend osc.udp://localhost:6666 /dmx/12/31 i 127

	# set channels 23,24,25,26, priority 1 to values 1,2,3,4
	oscsend osc.udp://localhost:6666 /dmx/{23,24,25,26}/1 iiii 1 2 3 4

	# set all channels, priority 3 to value 0
	oscsend osc.udp://localhost:6666 /dmx/*/3 i 0

	# set channel 0, priorities 0,1 to values 1, 2
	# set channel 1, priorities 0,1 to values 3, 4
	oscsend osc.udp://localhost:6666 /dmx/[0-1]/[0-1] iiii 1 2 3 4

	# set channel 0, priorities 0,1 to values 1, 2
	# set channel 1, priorities 0,1 to values 3, 3
	oscsend osc.udp://localhost:6666 /dmx/[0-1]/[0-1] iii 1 2 3

	# set channel 0, priorities 0,1 to values 1, 2
	# set channel 1, priorities 0,1 to values 2, 2
	oscsend osc.udp://localhost:6666 /dmx/[0-1]/[0-1] ii 1 2

	# set channel 0, priorities 0,1 to values 1, 1
	# set channel 1, priorities 0,1 to values 1, 1
	oscsend osc.udp://localhost:6666 /dmx/[0-1]/[0-1] i 1

##### **/dmx/[0-511]/[0-31]

To clear values, send your oSC messages to given OSC path without any arguments.

	# clear all channels, priorities
	oscsend osc.udp://localhost:6666 /dmx/*/*

	# clear channel 0-1, priorities 0-1
	oscsend osc.udp://localhost:6666 /dmx/[0-1]/[0-1]

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
