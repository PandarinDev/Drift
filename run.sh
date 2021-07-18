#!/bin/bash
SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
Xephyr :1 -screen 1600x900 &
sleep 1
DISPLAY=:1 $SCRIPTPATH/build/Drift &
sleep 1
DISPLAY=:1 alacritty