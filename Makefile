.PHONY : clean showversion

ARCH            := arm-none-linux-gnueabi-
CC		:= ${ARCH}gcc
CXX		:= ${ARCH}g++
LD		:= ${ARCH}g++
AR		:= ${ARCH}ar
RANLIB		:= ${ARCH}ranlib
STRIP		:= ${ARCH}strip
CXXFLAGS	?= -I${HOME}/ltib/rootfs/usr/include -L${HOME}/ltib/rootfs/usr/lib
VERSION := $(shell ./makeVersion.sh)

showversion:
	echo "building version ${VERSION} here"

INCS		:= -I${HOME}/linux-bd/include

LIBRARY_SRCS	:= camera.cpp cameraParams.cpp fb2_overlay.cpp fourcc.cpp imx_vpu.cpp imx_mjpeg_encoder.cpp physMem.cpp
LIBRARY_OBJS	:= $(addsuffix .o,$(basename ${LIBRARY_SRCS}))
LIBRARY		:= libimx-camera.a
LIBRARY_REF	:= -L./ -limx-camera

${LIBRARY}: ${LIBRARY_OBJS} 
	@$(AR) r $(LIBRARY) $(LIBRARY_OBJS)
	@$(RANLIB) $(LIBRARY)

camera_to_fb2: camera_to_fb2.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -lvpu -lpthread -o $@

devregs: devregs.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

fb2_overlay: fb2_overlay.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} -DOVERLAY_MODULETEST ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

ipu_bufs: ipu_bufs.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} -DOVERLAY_MODULETEST ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

battery_test: battery_test.c
	${CC} ${CFLAGS} ${INCS} ${DEFS} $< -o $@ -lm

pmic: pmic.cpp
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< -o $@


EXES		:= camera_to_fb2 devregs

%.o : %.cpp
	@echo "=== compiling:" $@ ${OPT} ${CXXFLAGS} 
	@${CXX} -c ${CXXFLAGS} ${INCS} ${DEFS} $< -o $@

all: ${LIBRARY} ${EXES}

clean:
	rm -f ${LIBRARY} ${EXES} *.o

