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

##### **/dmx**

* **i** channel offset (0-511)
* **i** value at channel offset +0 (0-255)
* **i** value at channel offset +1 (0-255)
* ...
* **i** value at channel offset +n (0-255)

For simple channel setting, send your OSC messages to path **/dmx** with the
first **i**nteger argument being the channel offset, and any following
**i**nteger argument(s) being subsequent channel values.

	# set channel 0 to value 255
	oscsend osc.udp://localhost:6666 /dmx ii 0 255

	# set channel 12 to value 127
	oscsend osc.udp://localhost:6666 /dmx ii 12 127

	# set channels 23,24,25,26 to values 1,2,3,4
	oscsend osc.udp://localhost:6666 /dmx iiiii 23 1 2 3 4

##### **/dmx/push**

Each channel has 32 associated priority values. Path **/dmx** pushes a value
to lowest priority channel=0. If you have higher priority values that should
precedence, you can push them onto the stack of values via path **/dmx/push**
with the first **i**nteger argument being the priority, second **i**nteger
argument being channel offset, and any following **i**nteger argument(s)
being subsequent channel values.

* **i** priority (0-31)
* **i** channel offset (0-511)
* **i** value at channel offset +0 (0-255)
* **i** value at channel offset +1 (0-255)
* ...
* **i** value at channel offset +n (0-255)

##### **/dmx/pop**

To remove a previously pushed priority value(s) via **/dmx/push** or **/dmx**,
you can remove values from the priorit stack via **/dmx/pop** with first
**i**nteger argument being the priority, and any subsequent integer argument(s)
being channel numbers.

* **i** priority (0-31)
* **i** channel number (0-511)
* **i** channel number (0-511)
* ...
* **i** channel number (0-511)

##### **/dmx/clear**

To remove all previously pushed priority value(s) via **/dmx/push** or **/dmx**,
you can remove all of from from the priorit stack via **/dmx/clear** with
subsequent **i**nteger argument(s) being channel numbers.

* **i** channel number (0-511)
* **i** channel number (0-511)
* ...
* **i** channel number (0-511)

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
