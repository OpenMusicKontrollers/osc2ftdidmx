#!/bin/bash

offset=4
red=$(( offset + 0 ))
blue=$(( offset + 1 ))
green=$(( offset + 2))
alpha=$(( offset + 3 ))

inc=8
dec=-8
slp=0.05

send()
{
	oscsend osc.udp://192.168.1.201:6666 /dmx ii $1 $2
}

send_rgb()
{
	local channel=$1
	local rgba=$2

	local r=$(( (rgba >> 24) & 0xff ))
	local g=$(( (rgba >> 16) & 0xff ))
	local b=$(( (rgba >>  8) & 0xff ))
	local a=$(( (rgba >>  9) & 0xff ))

	oscsend osc.udp://localhost:6666 /dmx iiiii $channel $r $g $b $a
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
	send $alpha 190
	ramp_up $green
	ramp_down $red
	ramp_up $blue
	ramp_down $green
	ramp_up $red
	ramp_down $blue
done

ramp_down $red
off
