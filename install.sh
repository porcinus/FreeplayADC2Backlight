#!/bin/bash
cp nns-freeplay-adc-backlight-daemon.service /lib/systemd/system/nns-freeplay-adc-backlight-daemon.service
systemctl enable nns-freeplay-adc-backlight-daemon.service
systemctl start nns-freeplay-adc-backlight-daemon.service