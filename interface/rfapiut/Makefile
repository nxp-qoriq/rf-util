# Copyright 2024 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only
# be used strictly in accordance with the applicable license terms. By expressly accepting
# such terms or by downloading, installing, activating and/or otherwise using
# the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms. If you do not agree to
# be bound by the applicable license terms, then you may not retain,
# install, activate or otherwise use the software.

CFLAGS  += -Wall

LIB_DIR1 ?= ${PWD}/interface/rfapi
LIB_DIR2 ?= ${PWD}/rflib
LIB_DIR3 ?= ${PWD}/ext/diora-sdk-open-source/lib
LIB_DIR4 ?= ${PWD}/interface/spi/

INCLUDES += -I${PWD}/interface/rfapi/
INCLUDES += -I${PWD}/ext/diora-sdk-open-source/api

SRCS_TEST := rf_api_unittest.c
OBJS_TEST := $(SRCS_TEST:.c =.o)
BIN_TEST := rfapiut

all: $(BIN_TEST)

$(BIN_TEST): ${OBJS_TEST}
	        ${CC} ${CFLAGS} -o $(BIN_TEST) ${OBJS_TEST} ${LIB_DIR1}/librfapi.a ${LIB_DIR2}/librflib.a ${LIB_DIR3}/libdiora-aarch64.a ${LIB_DIR4}/libecspi.a $(INCLUDES) ${LIB_DIR2}/libphallib.a -lm

%.o: %.c
	        ${CC} -c ${CFLAGS} ${INCLUDES}  $< -o $@

clean:
	         rm -rf *.o $(BIN_TEST)

install:
		mkdir ${PWD}/install/home/root/rf-ctrl/rfapiut/
		cp rfapiut ${PWD}/install/home/root/rf-ctrl/rfapiut/.
