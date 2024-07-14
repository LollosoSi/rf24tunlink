if [ -z "$1" ]
  then
    echo "You must specify the power level as the first argument (Accepted values: 0 - MIN, 1 - LOW, 2 - HIGH, 3 - MAX)"
    echo "Current config: "
    echo "$(<presets/radio_pb_config )"
    exit;
fi

if [ -z "$2" ]
  then
    echo "You must specify the bitrate as the second argument (Accepted values: 0 - 1Mbps, 1 - 2Mbps, 2 - 250kbps )"
    echo "Current config: "
    echo "$(<presets/radio_pb_config )"
    exit;
fi


printf "#	The modulation speed of the radios (Accepted values: 0 - 1Mbps, 1 - 2Mbps, 2 - 250kbps )
#	Type:	uint8
data_rate=$2

#	Power setting for all radios (Accepted values: 0 - MIN, 1 - LOW, 2 - HIGH, 3 - MAX)
#	Type:	uint8
radio_power=$1
" > presets/radio_pb_config

./stopservice.sh && ./startservice.sh
