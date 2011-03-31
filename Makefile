# obsidian Makefile

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

INCLUDES = -I. -Itextscreen -Iopl -Isrc -Ipcsound -Ienet/include $(shell sdl-config --cflags)
LIBS = $(shell sdl-config --libs)

ifeq ($(strip $(win32)), true)
LIBS += -mwindows -lws2_32 -lwinmm
endif

default: all
all: textscreen opl pcsound obsidian setup

# Textscreen stuffs
TXT_SOURCES = $(wildcard textscreen/*.c)
TXT_OBJS = $(patsubst textscreen/%.c,$(OBJ_D)/textscreen/%.o,$(TXT_SOURCES))

$(OBJ_D)/textscreen/%.o: textscreen/%.c
	@$(MKDIR) $(dir $@)
	$(CC) $(INCLUDES) -c $< -o $@

textscreen: $(TXT_OBJS)
	ar cru textscreen/libtextscreen.a $(TXT_OBJS)
	ranlib textscreen/libtextscreen.a

# OPL stuffs
OPL_SOURCES = $(wildcard opl/*.c)
OPL_OBJS = $(patsubst opl/%.c,$(OBJ_D)/opl/%.o,$(OPL_SOURCES))

$(OBJ_D)/opl/%.o: opl/%.c
	@$(MKDIR) $(dir $@)
	$(CC) $(INCLUDES) -c $< -o $@

opl: $(OPL_OBJS)
	ar cru opl/libopl.a $(OPL_OBJS)
	ranlib opl/libopl.a

# pcsound stuffs
PCSOUND_SOURCES = $(wildcard pcsound/*.c)
PCSOUND_OBJS = $(patsubst pcsound/%.c,$(OBJ_D)/pcsound/%.o,$(PCSOUND_SOURCES))

$(OBJ_D)/pcsound/%.o: pcsound/%.c
	@$(MKDIR) $(dir $@)
	$(CC) $(INCLUDES) -c $< -o $@

pcsound: $(PCSOUND_OBJS)
	ar cru pcsound/libpcsound.a $(PCSOUND_OBJS)
	ranlib pcsound/libpcsound.a

# engine stuff

OBS_SOURCES = $(wildcard src/*.c)
OBS_OBJS = $(patsubst src/%.c,$(OBJ_D)/%.o,$(OBS_SOURCES))

$(OBJ_D)/%.o: src/%.c
	$(CC) $(INCLUDES) -c $< -o $@

obsidian: $(OBS_OBJS)
	$(CC) $(INCLUDES) $(LIBS) -lSDL_mixer -lm -lsamplerate -o src/obsidian $(OBS_OBJS) enet/.libs/libenet.a textscreen/libtextscreen.a opl/libopl.a pcsound/libpcsound.a

clean:
	rm -rf obj/
