# Copyright 2021-2025 NXP
# NXP Proprietary. This software is owned or controlled by NXP and may only
# be used strictly in accordance with the applicable license terms. By expressly accepting
# such terms or by downloading, installing, activating and/or otherwise using
# the software, you are agreeing that you have read, and that you agree to
# comply with and are bound by, such license terms. If you do not agree to
# be bound by the applicable license terms, then you may not retain,
# install, activate or otherwise use the software.


SWIG ?= swig
SWIG_LIB ?= $(shell swig -swiglib)

TARGET= _rfif.so
MODULE = rfif
ifeq ($(MT3812), 1)
INCLUDE= -I../../interface -I../../rflib/ -I${MT3812_API_DIR} -I${MT3812_FIRMWARE_DIR} -I../../port -I${MT3812_LIB_DIR} -I../../interface/spi
else
INCLUDE= -I../../interface -I../../rflib/
endif

ifeq ($(PYTHON_INCLUDE),)
PY_INCLUDE= -I/usr/include/python3.8 -I../../pyincludes
else
PY_INCLUDE=-I../../pyincludes/aarch64-linux-gnu/python3.10 -I../../pyincludes/aarch64-linux-gnu  -I${PYTHON_INCLUDE} -I${PYTHON_INCLUDE}/../
endif

all : $(TARGET)

$(TARGET):
	$(SWIG) -python $(INCLUDE) $(EXTRA_CFLAGS) $(MODULE).i
	$(CC) -O2 -fPIC -c $(EXTRA_CFLAGS) $(MODULE)_wrap.c $(INCLUDE) $(PY_INCLUDE)
ifeq ($(MT3812), 1)
	$(CC) -shared $(MODULE)_wrap.o -L../../rflib/ -lrflib -L${MT3812_LIB_DIR} -ldiora-aarch64 -L../../interface/spi -lecspi -o $(TARGET)
else
	$(CC) -shared $(MODULE)_wrap.o -L../../rflib/ -lrflib -o $(TARGET)
endif

install:
	cp ${TARGET} $(RFCTRL_FR1_DIR)/
	cp $(MODULE).py $(RFCTRL_FR1_DIR)

clean:
	rm -f *_wrap* *~ .~* rfif.py
	rm -f core
	rm -f *.o *.so
