printf "
ARG1=${PWD}/presets/HARQ_doubleradio_reedsolomon
ARG2=${PWD}/presets/secondary
ARG3=${PWD}/presets/nodisplay
ARG4=${PWD}/presets/nocsv
" > .progconf

printf "[Unit]
Description=rf24tunlink

[Service]
WorkingDirectory=${PWD}
EnvironmentFile=${PWD}/.progconf
ExecStart=${PWD}/rf24tunlink \${ARG1} \${ARG2} \${ARG3} \${ARG4}
Restart=always

[Install]
WantedBy=multi-user.target
" > rf24tunlink.service

sudo cp rf24tunlink.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable rf24tunlink.service
