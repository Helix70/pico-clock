#pico-zephyr-project
#-------------------------------------------------------------------------------
# Zephyr Example Application
#
# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
#-------------------------------------------------------------------------------
# NOTE: Please do not remove the #pico-zephyr-project header, it is used by
# the Raspberry Pi Pico VS Code extension to identify the project type.
#-------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pico-clock LANGUAGES C)

target_sources(app PRIVATE
    src/main.c
    src/aip33628.c
    src/buttons.c
    src/ds1302.c
    src/hu058.c
    src/lightsensor.c
    src/settings.c
    src/speaker.c
    src/thermistor.c
)

target_sources_ifdef(CONFIG_WIFI app PRIVATE
    src/ntp.c
    src/wifista.c
    src/wifisetup.c
    src/httpserver.c
    src/webconfig.c
)
