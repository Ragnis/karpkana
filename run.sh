#!/bin/bash

v4l2-ctl -d /dev/video0 -c brightness=48
v4l2-ctl -d /dev/video0 -c contrast=20
v4l2-ctl -d /dev/video0 -c saturation=28
v4l2-ctl -d /dev/video0 -c hue=-7
v4l2-ctl -d /dev/video0 -c white_balance_automatic=0
v4l2-ctl -d /dev/video0 -c exposure=89
v4l2-ctl -d /dev/video0 -c gain_automatic=0
v4l2-ctl -d /dev/video0 -c gain=5

./bin/main
