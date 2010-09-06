.PHONY : clean showversion

ARCH            := arm-none-linux-gnueabi-
CC		:= ${ARCH}gcc
CXX		:= ${ARCH}g++
LD		:= ${ARCH}g++
AR		:= ${ARCH}ar
RANLIB		:= ${ARCH}ranlib
STRIP		:= ${ARCH}strip

VERSION := $(shell ./makeVersion.sh)

showversion:
	echo "building version ${VERSION} here"

INCS		:= -I${HOME}/linux-bd/include

LIBRARY_SRCS	:= camera.cpp cameraParams.cpp fb2_overlay.cpp fourcc.cpp
LIBRARY_OBJS	:= $(addsuffix .o,$(basename ${LIBRARY_SRCS}))
LIBRARY		:= libimx-camera.a
LIBRARY_REF	:= -L./ -limx-camera

${LIBRARY}: ${LIBRARY_OBJS} 
	@$(AR) r $(LIBRARY) $(LIBRARY_OBJS)
	@$(RANLIB) $(LIBRARY)

camera_to_fb2: camera_to_fb2.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

devregs: devregs.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

fb2_overlay: fb2_overlay.cpp ${LIBRARY} 
	${CXX} ${CXXFLAGS} -DOVERLAY_MODULETEST ${INCS} ${DEFS} $< ${LIBRARY_REF} -o $@

EXES		:= camera_to_fb2 devregs

%.o : %.cpp
	@echo "=== compiling:" $@ ${OPT} ${CXXFLAGS} 
	@${CXX} -c ${CXXFLAGS} ${INCS} ${DEFS} $< -o $@

all: ${LIBRARY} ${EXES}

clean:
	rm -f ${LIBRARY} ${EXES} *.o

