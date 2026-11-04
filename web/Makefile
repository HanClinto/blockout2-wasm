# The web build of BlockOut II (Emscripten). Run through web/build.sh, which runs it in the emscripten/emsdk container;
# or, with emsdk on the PATH, from the top of the repository: make -f web/Makefile -j8
#
# Output in build/web/: index.html (the page: web/shell.html), blockout.js (the page's code), scores.js (score
# services), and the game: bl2.js, bl2.wasm, bl2.data (its art and sounds).

EMCC     ?= emcc
OUT      := build/web
OBJDIR   := build/obj

GAME_SRC := $(addprefix BlockOut/, \
	BlockOut.cpp PageChooseSetup.cpp PageStartGame.cpp EditControl.cpp PageControls.cpp Pit.cpp \
	Game.cpp PageCredits.cpp PolyCube.cpp Http.cpp PageHallOfFame.cpp SetupManager.cpp \
	InitPolyCube.cpp PageHallOfFameOnLine.cpp SoundManager.cpp Menu.cpp PageHttp.cpp Sprites.cpp \
	MenuGraphics.cpp PageMainMenu.cpp Utils.cpp MenuGrid.cpp PageOptions.cpp PageChangeSetup.cpp \
	PageScoreDetails.cpp PageGSOptions.cpp BotPlayer.cpp BotPlayerAI.cpp BotMatrix.cpp \
	GLApp/GLApp.cpp GLApp/GLFont.cpp GLApp/GLMatrix.cpp GLApp/GLSprite.cpp)

IMG_SRC  := $(addprefix ImageLib/src/, \
	gif/gif.cpp CImage.cpp png/zlib/adler32.c png/zlib/compress.c \
	png/zlib/crc32.c png/zlib/deflate.c png/zlib/gzio.c png/zlib/infback.c \
	png/zlib/inffast.c png/zlib/inflate.c png/zlib/inftrees.c png/zlib/trees.c \
	png/zlib/uncompr.c png/zlib/zutil.c png/png/png.c png/png/pngerror.c \
	png/png/pnggccrd.c png/png/pngget.c png/png/pngmem.c png/png/pngpread.c \
	png/png/pngread.c png/png/pngrio.c png/png/pngrtran.c png/png/pngrutil.c \
	png/png/pngset.c png/png/pngtrans.c png/png/pngvcrd.c png/png/pngwio.c \
	png/png/pngwrite.c png/png/pngwtran.c png/png/pngwutil.c png/hpng.c \
	jpg/H2v2.cpp jpg/idct.cpp jpg/jpegdecoder.cpp jpg/jpegdecodermain.cpp)

WEB_SRC  := web/glcompat.cpp web/webhost.cpp

SDL_INC  := $(shell dirname $$(which $(EMCC)))/system/include/SDL
COMMON   := -O2 -Dlinux -w
CXXSTD   := -std=gnu++14
GAME_FLAGS := $(COMMON) $(CXXSTD) -I$(SDL_INC) -IImageLib/src -IBlockOut -include web/glcompat.h
IMG_FLAGS  := $(COMMON) -IImageLib/src

LDFLAGS  := -O2 -sLEGACY_GL_EMULATION=1 -sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -lidbfs.js \
	-sEXPORTED_RUNTIME_METHODS=FS,ENV,addRunDependency,removeRunDependency,ccall,stringToNewUTF8,UTF8ToString \
	-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_web_pause \
	--pre-js web/pre.js \
	--preload-file images@/bl2/images --preload-file sounds@/bl2/sounds \
	--shell-file web/shell.html

obj = $(OBJDIR)/$(basename $(1)).o
GAME_OBJ := $(foreach f,$(GAME_SRC),$(call obj,$(f)))
IMG_OBJ  := $(foreach f,$(IMG_SRC),$(call obj,$(f)))
WEB_OBJ  := $(foreach f,$(WEB_SRC),$(call obj,$(f)))

all: $(OUT)/index.html $(OUT)/blockout.js $(OUT)/scores.js

$(OUT)/index.html: $(GAME_OBJ) $(IMG_OBJ) $(WEB_OBJ) web/pre.js web/shell.html $(wildcard images/* sounds/*)
	@mkdir -p $(OUT)
	$(EMCC) $(GAME_OBJ) $(IMG_OBJ) $(WEB_OBJ) $(LDFLAGS) -o $(OUT)/bl2.html
	mv $(OUT)/bl2.html $@

$(OUT)/%.js: web/%.js
	@mkdir -p $(OUT)
	cp $< $@

$(OBJDIR)/BlockOut/%.o: BlockOut/%.cpp web/glcompat.h
	@mkdir -p $(dir $@)
	$(EMCC) $(GAME_FLAGS) -c $< -o $@

$(OBJDIR)/web/%.o: web/%.cpp web/glcompat.h web/webhost.h
	@mkdir -p $(dir $@)
	$(EMCC) $(COMMON) -I$(SDL_INC) -c $< -o $@

$(OBJDIR)/ImageLib/%.o: ImageLib/%.cpp
	@mkdir -p $(dir $@)
	$(EMCC) $(IMG_FLAGS) $(CXXSTD) -c $< -o $@

$(OBJDIR)/ImageLib/%.o: ImageLib/%.c
	@mkdir -p $(dir $@)
	$(EMCC) $(IMG_FLAGS) -c $< -o $@

clean:
	rm -rf build

.PHONY: all clean
