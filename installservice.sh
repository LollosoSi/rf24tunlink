printf "
ARG1=${PWD}/presets/radio_config.txt
ARG2=${PWD}/presets/tunlink_config.txt
ARG3=${PWD}/presets/harq_config.txt
ARG4=${PWD}/presets/primary
" > .progconf

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
