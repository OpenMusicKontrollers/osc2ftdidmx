#!/bin/bash

red=0
blue=1
green=2
alpha=3

inc=8
dec=-8
slp=0.05

send()
{
	oscsend osc.udp://localhost:6666 /dmx ii $1 $2
}

ramp_up()
{
	for i in $(seq 0 $inc 255); do
		send $1 $i
		sleep $slp
	done
}

ramp_down()
{
	for i in $(seq 255 $dec 0); do
		send $1 $i
		sleep $slp
	done
}

off()
{
	send $alpha 0
}

on()
{
	send $red 0
	send $green 0
	send $blue 0
	send $alpha 190
}

on
ramp_up $red

while true; do
	ramp_up $green
	ramp_down $red
	ramp_up $blue
	ramp_down $green
	ramp_up $red
	ramp_down $blue
done

ramp_down $red
off
