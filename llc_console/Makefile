FRAMEBUFFER		?= 0
ExtraIncDir		?=
ExtraLibDir		?=
LLCExcludeSrc	?= llc_process_broker.cpp llc_test_core.cpp
IncDir			:= ../../llc/llc ../../llc/llc_engine $(ExtraIncDir)
Libs			:= llc.a
GccLibs			:= xcb xcb-image xkbcommon xkbcommon-x11 pthread
Platform		:= Linux.x64
ProjectName		:= llc_console
Configuration	:= Debug
ProjectDir		?= .
SolutionDir		?= $(ProjectDir)/..

C_DEFS			:= -DNOMINMAX -DLLC_XCB -DDEBUG
OPT				:= 0

ifeq ($(FRAMEBUFFER), 1)
	Platform		= LinuxFB.x64
	GccLibs			= :libxkbcommon.so.0 pthread
	C_DEFS			= -DNOMINMAX -DLLC_FRAMEBUFFER -DDEBUG
	LLCExcludeSrc	= llc_process_broker.cpp llc_test_core.cpp
endif

ifeq ($(DEBUG), 0)
	Configuration	:= Release
	OPT				:= 3
	C_DEFS			:= -DNOMINMAX -DLLC_XCB
ifeq ($(FRAMEBUFFER), 1)
	C_DEFS			= -DNOMINMAX -DLLC_FRAMEBUFFER
endif
endif

OutDir		:= $(SolutionDir)/../$(Platform).$(Configuration)
IntDir		:= $(SolutionDir)/../obj/$(Platform).$(Configuration)/$(ProjectName)

LIBS		:= $(patsubst %, $(OutDir)/%, $(Libs))
GCCLIBS		:= $(patsubst %, -l%, $(GccLibs))
INCDIR		:= $(patsubst %,-I%,$(IncDir))
LIBDIR		:= $(patsubst %,-L%,$(ExtraLibDir))

CFLAGS		:= -std=c++20 -Wall -fpermissive -Wno-changes-meaning -O$(OPT) -include ../../llc/llc/llc_particle.h $(C_DEFS) $(INCDIR)
LLCFLAGS	:= -std=c++20 -Wall -fpermissive -Wno-changes-meaning -Wno-invalid-constexpr -O$(OPT) -I../../zlib $(C_DEFS) -include stdio.h -include stdlib.h
CC			:= g++

OBJ			:= $(IntDir)/llc_console.o

$(IntDir)/%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

$(OutDir)/llc.a:
	$(MAKE) -C ../../llc/llc DEBUG=$(DEBUG) FRAMEBUFFER=$(FRAMEBUFFER) ExtraIncDir="$(ExtraIncDir)" ExcludeSrc="$(LLCExcludeSrc)" CFLAGS="$(LLCFLAGS)" build

build: $(IntDir) $(OBJ) $(LIBS)
	mkdir -p $(OutDir)
	$(CC) $(OBJ) $(CFLAGS) -o $(OutDir)/$(ProjectName).bin $(LIBS) $(LIBDIR) $(GCCLIBS)

$(IntDir):
	mkdir -p $@

.PHONY: build clean

clean:
	rm -rf $(IntDir)
