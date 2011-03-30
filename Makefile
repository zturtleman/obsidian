# obsidian Makefile
# my first Makefile :(

SHELL=/bin/sh
CC=gcc
MKDIR=mkdir -p

OBJ_D=obj
OPT_LEVEL=0
DGB_LEVEL=3

# Windows build? (make sure to set CC to the mingw compiler)
win32 := false
# Linux, FreeBSD, etc. (default)
nix := true

# GNU make?
ifeq ($(strip $(shell make -v |grep "GNU")),)
 $(warning WARNING: You are using non-gnu make, try "gmake" if build fails)
endif

INCLUDES = -Itextscreen -Iopl -Isrc -Ipcsound -Ienet/include `sdl-config --cflags`
LIBS = `sdl-config --libs`
OBS_LIBS = $(LIBS) enet/.libs/libenet.a -lSDL_mixer

ifeq ($(strip $(win32)), true)
LIBS += -mwindows -lws2_32 -lwinmm
endif

# Textscreen stuffs
TXT_SOURCES = $(wildcard textscreen/*.c)
TXT_OBJS = $(patsubst textscreen/%.c,$(OBJ_D)/textscreen/%.o,$(TXT_SOURCES))

default: all
all: textscreen

$(TXT_OBJS): $(TXT_SOURCES)
	@$(MKDIR) $(dir $@)
	$(CC) $(INCLUDES) -c $< -o $@

textscreen: $(TXT_OBJS)
	$(CC) $(INCLUDES) $(LIBS) -o libtextscreen.a $(TXT_OBJS)
