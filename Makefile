.PHONY : clean showversion

ARCH            := arm-none-linux-gnueabi-
CC		:= ${ARCH}gcc
CXX		:= ${ARCH}g++
LD		:= ${ARCH}g++
AR		:= ${ARCH}ar
RANLIB		:= ${ARCH}ranlib
STRIP		:= ${ARCH}strip
CXXFLAGS	?= -I/tftpboot/ltib/usr/include -I${HOME}/linux/include -L/tftpboot/ltib/usr/lib
VERSION := $(shell ./makeVersion.sh)

showversion:
	echo "building version ${VERSION} here"

INCS		:= -I/tftpboot/linux-bd/include

LIBRARY_SRCS	:= camera.cpp cameraParams.cpp fb2_overlay.cpp fourcc.cpp imx_vpu.cpp imx_mjpeg_encoder.cpp physMem.cpp hexDump.cpp imx_h264_encoder.cpp v4l_display.cpp
LIBRARY_OBJS	:= $(addsuffix .o,$(basename ${LIBRARY_SRCS}))
LIBRARY		:= libimx-camera.a
LIBRARY_REF	:= -L./ -limx-camera

${LIBRARY}: ${LIBRARY_OBJS} 
	@$(AR) r $(LIBRARY) $(LIBRARY_OBJS)
	@$(RANLIB) $(LIBRARY)

camera_to_fb2: camera_to_fb2.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -lvpu -lpthread -o $@

camera_to_v4l: camera_to_v4l.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -lvpu -lpthread -o $@

devregs: devregs.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

fb2_overlay: fb2_overlay.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} -DOVERLAY_MODULETEST ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

ipu_bufs_mx53: ipu_bufs.cpp ${LIBRARY}
	${CXX} ${CXXFLAGS} -DMX53 ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

ipu_bufs_mx51: ipu_bufs.cpp ${LIBRARY}
	${CXX} ${CXXFLAGS} -DMX51 ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

battery_test: battery_test.c
	${CC} ${CFLAGS} ${INCS} ${DEFS} $< -o $@ -lm

pmic: pmic.cpp
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< -o $@

imx_h264_encoder: imx_h264_encoder.cpp ${LIBRARY}
	${CXX} ${CXXFLAGS} -DMODULETEST=1 ${INCS} ${DEFS} $< -lvpu ${LIBRARY_REF} -lavformat -lavcodec -lavutil -lz -lbz2 -lpthread -o $@

imx_mpeg4_encoder: imx_mpeg4_encoder.cpp ${LIBRARY}
	${CXX} ${CXXFLAGS} -DMODULETEST=1 ${INCS} ${DEFS} $< -lvpu ${LIBRARY_REF} -lavformat -lavcodec -lavutil -lz -lbz2 -lpthread -o $@

EXES		:= camera_to_fb2 camera_to_v4l devregs

%.o : %.cpp
	@echo "=== compiling:" $@ ${OPT} ${CXXFLAGS} 
	@${CXX} -c ${CXXFLAGS} ${INCS} ${DEFS} $< -o $@

all: ${LIBRARY} ${EXES}

clean:
	rm -f ${LIBRARY} ${EXES} *.o

