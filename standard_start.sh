#!/usr/bin/bash
if [ -z "$1" ]
  then
    echo "No argument supplied, you must supply the primary/secondary (1/2) as the first argument"
    exit;
fi

if [ $1 == 1 ]
  then
    sudo ./rf24tunlink presets/tunlink presets/primary
    exit;
fi

if [ $1 == 2 ]
  then
    sudo ./rf24tunlink presets/tunlink presets/secondary
fi