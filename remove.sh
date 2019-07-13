#!/bin/bash
systemctl stop nns-freeplay-adc-backlight-daemon.service
systemctl disable nns-freeplay-adc-backlight-daemon.service
rm /lib/systemd/system/nns-freeplay-adc-backlight-daemon.service