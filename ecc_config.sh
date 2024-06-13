if [ -z "$1" ]
  then
    echo "No argument supplied, you must specify the ECC bytes"
    echo "Current config: "
    echo "$(<presets/rs_config )"
    exit;
fi

if [ -z "$2" ]
  then
    pl_sz=32
  else
    pl_sz=$2
fi

val=$(expr $pl_sz - $1)

printf "#	The data bytes for the Reed-Solomon codec
#	Type:	uint8
data_bytes=$val

#	The ECC bytes for the Reed-Solomon codec (note, one byte might be used as CRC, this element must be >0)
#	Type:	uint8
ecc_bytes=$1

payload_size=$pl_sz
" > presets/rs_config

./stopservice.sh && ./startservice.sh
