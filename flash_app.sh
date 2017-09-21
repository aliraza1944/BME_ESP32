#!/bin/sh

python /c/esp/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port COM3 --baud 1152000 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x1000 /c/Users/Ali/OneDrive/UPWORK/Bahan_Sadegh/Env_Sensor/Test/BME280/BME280_1/build/bootloader/bootloader.bin 0x10000 /c/Users/Ali/OneDrive/UPWORK/Bahan_Sadegh/Env_Sensor/Test/BME280/BME280_1/build/app-template.bin 0x8000 /c/Users/Ali/OneDrive/UPWORK/Bahan_Sadegh/Env_Sensor/Test/BME280/BME280_1/build/partitions.bin



make monitor
