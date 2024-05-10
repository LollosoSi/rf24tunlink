#!/usr/bin/bash
if [ -z "$1" ]
  then
    echo "No argument supplied, you must supply the primary/secondary (1/2) as the first argument"
    exit;
fi
screen -dmS tunlink sudo -i -u andrea sudo ./rf24tunlink/rf24_watchdog.sh -daemon $1
