if [ -z "$1" ]
  then
    echo "No argument supplied, you must specify whether to install as \"primary\" or \"secondary\""
    exit;
fi

if [ "$1" == "primary" ]
then
printf "
ARG1=${PWD}/presets/radio_config.txt
ARG2=${PWD}/presets/tunlink_config.txt
ARG3=${PWD}/presets/harq_config.txt
ARG4=${PWD}/presets/primary
" > .progconf
elif [ "$1" == "secondary" ]
then
printf "
ARG1=${PWD}/presets/radio_config.txt
ARG2=${PWD}/presets/tunlink_config.txt
ARG3=${PWD}/presets/harq_config.txt
ARG4=${PWD}/presets/secondary
" > .progconf
else
 echo "Bad argument supplied, you must specify whether to install as \"primary\" or \"secondary\""
 exit;
fi

printf "[Unit]
Description=rf24tunlink2

[Service]
WorkingDirectory=${PWD}
EnvironmentFile=${PWD}/.progconf
ExecStart=${PWD}/rf24tunlink2 \${ARG1} \${ARG2} \${ARG3} \${ARG4}
Restart=always

[Install]
WantedBy=multi-user.target
" > rf24tunlink2.service

sudo cp rf24tunlink2.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable rf24tunlink2.service
