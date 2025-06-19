# Copyright 2021-2024 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only
# be used strictly in accordance with the applicable license terms. By expressly accepting
# such terms or by downloading, installing, activating and/or otherwise using
# the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms. If you do not agree to
# be bound by the applicable license terms, then you may not retain,
# install, activate or otherwise use the software.

.ONESHELL:

CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

ifndef LA9310_COMMON_HEADERS
$(error LA9310_COMMON_HEADERS is not set)
endif

DEST_DIR ?= ${PWD}/install
BIN_DIR ?= ${DEST_DIR}/usr/bin
LIB_DIR ?= ${DEST_DIR}/usr/lib
HOME_DIR ?= ${DEST_DIR}/home/root
RFCTRL_FR1_DIR ?= ${DEST_DIR}/home/root/rf-ctrl/fr1
RFAPI_FR1_DIR ?= ${DEST_DIR}/home/root/rf-ctrl/rfapi
RFCTRL_UTILS_FR1_DIR ?= ${DEST_DIR}/home/root/rf-ctrl/utils/fr1
RFCTRL_UTILS_SCRIPTS_DIR ?= ${DEST_DIR}/home/root/rf-ctrl/utils/scripts
INTERFACE_DIR := ${PWD}/interface
PORT_DIR := ${PWD}/port
RFLIB_DIR := ${PWD}/rflib
MT3812_DIR := ${PWD}/ext/diora-sdk-open-source
MT3812_LIB_DIR := ${MT3812_DIR}/lib
MT3812_API_DIR := ${MT3812_DIR}/api
MT3812_FIRMWARE_DIR := ${MT3812_DIR}/firmware
FR1_SPI_INTERFACE_DIR := ${PWD}/interface/spi
PYTHON_PACKAGES_DIR ?= ${DEST_DIR}/usr/lib/python3.6/site-packages
INCLUDES += -I${PWD}/rflib/

#CFLAGS += -DDEBUG
CFLAGS += -Wall -O0 -g -D__RFIC

ifeq ($(MT3812), 1)
EXTRA_CFLAGS += -DMT3812
endif

CFLAGS += $(EXTRA_CFLAGS)
export CC BIN_DIR LIB_DIR CONFIG_DIR RFCTRL_FR1_DIR RFCTRL_UTILS_FR1_DIR RFCTRL_UTILS_SCRIPTS_DIR RFAPI_FR1_DIR
export INTERFACE_DIR KERNEL_DIR PORT_DIR RFLIB_DIR FR1_SPI_INTERFACE_DIR
export LA9310_COMMON_HEADERS MT3812_DIR MT3812_FIRMWARE_DIR MT3812_API_DIR MT3812_LIB_DIR
export INCLUDES CFLAGS ABERDEEN EXTRA_CFLAGS

ifeq ($(MT3812), 1)
DIRS := rflib python interface/rfapi interface/rfapiut
endif

all: ${DIRS}
ifeq ($(MT3812),1)
	mkdir -p $(FR1_SPI_INTERFACE_DIR)
	if test -f $(ECSPI_LIB_PATH)/libecspi.a; then\
		cp $(ECSPI_LIB_PATH)/libecspi.a $(FR1_SPI_INTERFACE_DIR);\
	else\
		echo "libecspi.a file not found";\
	fi
endif
	$(foreach b, $(DIRS), ${MAKE} -C ${b}  all;)

clean: ${DIRS}
	$(foreach b, $(DIRS), ${MAKE} -C ${b}  clean;)
	rm -rf ${DEST_DIR}
	rm -rf ${FR1_SPI_INTERFACE_DIR}

install: ${DIRS}
	mkdir -p ${BIN_DIR} ${LIB_DIR} ${CONFIG_DIR} ${PYTHON_PACKAGES_DIR} ${HOME_DIR} ${RFCTRL_FR1_DIR} $(RFCTRL_UTILS_FR1_DIR) $(RFCTRL_UTILS_SCRIPTS_DIR) ${RFAPI_FR1_DIR};
	$(foreach b, $(DIRS), ${MAKE} -C ${b}  install;)

release: ${DIRS}
