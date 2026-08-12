#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# ROMFS is the directory containing data to be added to RomFS, relative to the Makefile (Optional)
#
# NO_ICON: if set to anything, do not use icon.
# NO_NACP: if set to anything, no .nacp file is generated.
# APP_TITLE is the name of the app stored in the .nacp file (Optional)
# APP_AUTHOR is the author of the app stored in the .nacp file (Optional)
# APP_VERSION is the version of the app stored in the .nacp file (Optional)
# APP_TITLEID is the titleID of the app stored in the .nacp file (Optional)
# ICON is the filename of the icon (.jpg), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.jpg
#     - icon.jpg
#     - <libnx folder>/default_icon.jpg
#
# CONFIG_JSON is the filename of the NPDM config file (.json), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.json
#     - config.json
#   If a JSON file is provided or autodetected, an ExeFS PFS0 (.nsp) is built instead
#   of a homebrew executable (.nro). This is intended to be used for sysmodules.
#   NACP building is skipped as well.
#---------------------------------------------------------------------------------
TARGET		:=	PKSE
BUILD		:=	build
SOURCES		:=	src src/Pokemon src/Encryption src/Enums src/UI src/UI/Panels src/UI/Dialogs src/UI/Modals src/Trainer src/Names src/Utils src/Save src/Legality src/Conversion nanovg
DATA		:=	data
INCLUDES	:=	include nanovg
APP_TITLE   :=  PKSE
APP_AUTHOR  :=  Kiasta
# THE version, in two spellings. Both are set here and nothing downstream needs editing.
#
# APP_VERSION is the .nacp one -- the home menu and hbmenu read it. The name is not ours to choose:
# libnx's switch_rules passes $(APP_VERSION) straight to `nacptool --create`. Its display_version
# field is 16 bytes and nacptool truncates to fit WITHOUT complaining (exit 0, no warning), so this
# must stay at 15 characters or fewer -- "1.1.2-pre-release-debug" was silently becoming
# "1.1.2-pre-relea" in the .nro.
#
# APP_VERSION_FULL is the one the app prints about itself: -DPKSE_VERSION below feeds it to
# Globals.h's VERSION_STRING, which has no length limit. Long pre-release tags belong here.
#
# Keep the short one an ABBREVIATION of the long one. The split exists so the .nacp can hold less of
# the version, not a different version.
#
# NOTE: no trailing comment on either assignment line. Make keeps trailing whitespace in a value, so
# "0.0.3 \t\t# ..." would have baked spaces into the .nacp version and the -D define.
APP_VERSION :=	1.1.2
APP_VERSION_FULL :=	1.1.2

# Mirrors what switch_rules does for APP_VERSION: an unset long form falls back to the short one
# rather than compiling in an empty version string.
ifeq ($(strip $(APP_VERSION_FULL)),)
APP_VERSION_FULL := $(APP_VERSION)
endif
ROMFS		:=	romfs
ICON		:=  assets/icon.jpg

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

# NanoVG: disable its stb_image (we feed RGBA buffers via nvgCreateImageRGBA, and SpriteManager
# already owns the STB_IMAGE_IMPLEMENTATION) — avoids duplicate symbols. Keeps fontstash for text.
DEFINES	:=	-DNVG_NO_STB -DPKSE_VERSION='"$(APP_VERSION_FULL)"'

#---------------------------------------------------------------------------------
# Production builds:  make clean && make all prod
#
# 'prod' is a MODIFIER, not something that builds -- it is picked out of MAKECMDGOALS so it can
# flip a flag for whatever else is on the command line, which is what makes 'make all prod' work
# ('all' does the building, 'prod' only changes how). 'make PROD=1 all' is equivalent.
#
# What it does: -DPKSE_PROD, which compiles every SD-card log sink out of src/Utils/Logger.cpp --
# the dated debug logs under sdmc:/PKSE/logs and the sdmc:/PKSE/test-trace.log trace. A release
# build should not drop a new log file on the user's card every time they open the app.
#
# The export matters. DEFINES is evaluated ABOVE the top-level/build-dir split, so the sub-make
# that actually compiles re-derives it from this same file -- and that sub-make is invoked with
# the goal 'all', not 'prod', so the MAKECMDGOALS test below is FALSE there. Exporting PROD puts
# it in the sub-make's environment, where it is picked up as a make variable and the -D survives.
# Without the export this silently builds a normal logging binary that merely looks like a
# release, which is the worst possible failure for a flag whose whole job is what NOT to ship.
#
# Objects do not depend on this flag, so switching modes needs a 'make clean' first. The build
# prints which mode it is in (see the $(BUILD) rule) rather than leaving that to be assumed.
#---------------------------------------------------------------------------------
ifneq (,$(filter prod,$(MAKECMDGOALS)))
export PROD := 1
endif

ifeq ($(PROD),1)
DEFINES	+=	-DPKSE_PROD
BUILD_MODE	:=	production (SD-card logging compiled out)
else
BUILD_MODE	:=	debug (logging enabled)
endif

#---------------------------------------------------------------------------------
# SDL2 provides the window, GL context and input ONLY -- rendering is NanoVG on GL,
# PNG decoding is stb_image and text is NanoVG's own font atlas, so SDL2_image and
# SDL2_ttf are no longer linked. Resolve the exact include paths and static link chain
# from devkitPro's pkg-config so we don't hand-maintain the (long, order-sensitive)
# dependency list.
#---------------------------------------------------------------------------------
SDL_PKGCONFIG	:=	$(DEVKITPRO)/portlibs/switch/bin/aarch64-none-elf-pkg-config
SDL_CFLAGS	:=	$(shell $(SDL_PKGCONFIG) --cflags sdl2)
SDL_LIBS	:=	$(shell $(SDL_PKGCONFIG) --libs --static sdl2)

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__ $(SDL_CFLAGS)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

CXXFLAGS	+=	-std=c++20

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# SDL_LIBS already pulls in -lnx -lz -lm and the EGL/mesa/freetype/png/jpeg/webp
# chain; we only need to add the project's own extra libs (lz4).
# NanoVG renders through OpenGL 4.3 core loaded by switch-glad. -lglad must come BEFORE the
# EGL/mesa chain (which SDL_LIBS ends with: -lEGL -lglapi -ldrm_nouveau -lnx) so the static
# linker resolves glad's eglGetProcAddress reference.
LIBS	:= -lglad $(SDL_LIBS) -llz4 -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(LIBNX)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(CONFIG_JSON)),)
	jsons := $(wildcard *.json)
	ifneq (,$(findstring $(TARGET).json,$(jsons)))
		export APP_JSON := $(TOPDIR)/$(TARGET).json
	else
		ifneq (,$(findstring config.json,$(jsons)))
			export APP_JSON := $(TOPDIR)/config.json
		endif
	endif
else
	export APP_JSON := $(TOPDIR)/$(CONFIG_JSON)
endif

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.jpg)
	ifneq (,$(findstring $(TARGET).jpg,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).jpg
	else
		ifneq (,$(findstring icon.jpg,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.jpg
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_ICON)),)
	export NROFLAGS += --icon=$(APP_ICON)
endif

ifeq ($(strip $(NO_NACP)),)
	export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp
endif

ifneq ($(APP_TITLEID),)
	export NACPFLAGS += --titleid=$(APP_TITLEID)
endif

ifneq ($(ROMFS),)
	export NROFLAGS += --romfsdir=$(CURDIR)/$(ROMFS)
endif

# Default target when you just run 'make'. Only builds.
default: $(BUILD)

# Target when you run 'make all'. Downloads type icons + fonts, THEN build (HD sprites: tools/gen_hdsprites.py)
all: types fonts $(BUILD)

# 'prod' builds nothing -- it is a modifier that sets -DPKSE_PROD for the rest of the command line
# (see DEFINES near the top). It exists as a target so 'make all prod' has something to resolve.
# Combine it: 'make clean && make all prod'. On its own it only prints what it would have changed.
prod:
	@printf "production mode: SD-card logging compiled out (-DPKSE_PROD)\n"
	@printf "  combine with a build target, e.g. 'make clean && make all prod'\n"

.PHONY: prod

#---------------------------------------------------------------------------------
# Sprite and icon download integration
#---------------------------------------------------------------------------------
TYPE_DIR     := romfs/sprites/types
TYPE_BASE_URL ?= https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/types/generation-ix/scarlet-violet
MAX_JOBS     := 20        # increase the value if you want it to run faster
DOWNLOAD_RETRIES ?= 5
DOWNLOAD_RETRY_DELAY ?= 2
DOWNLOAD_CONNECT_TIMEOUT ?= 15
DOWNLOAD_MAX_TIME ?= 60

export DOWNLOAD_RETRIES DOWNLOAD_RETRY_DELAY DOWNLOAD_CONNECT_TIMEOUT DOWNLOAD_MAX_TIME

#---------------------------------------------------------------------------------
# Type sprite download (generation-ix scarlet-violet style)
# PokeAPI type IDs: 1=Normal, 2=Fighting, ... 18=Fairy
# We save as 0-17 to match our internal MoveType enum
#---------------------------------------------------------------------------------
types:
	@printf "Checking and downloading missing type sprites...\n"
	@mkdir -p "$(TYPE_DIR)"
	@missing_list=""; \
	for i in $$(seq 1 18); do \
		local_id=$$((i - 1)); \
		[ -s "$(TYPE_DIR)/$$local_id.png" ] || missing_list="$$missing_list $$i"; \
	done; \
	if [ -z "$$missing_list" ]; then \
		printf "All type sprites already present — nothing to download.\n"; \
		exit 0; \
	fi; \
	count=0; \
	for i in $$missing_list; do count=$$((count + 1)); done; \
	printf "Downloading %d missing type sprite(s) in parallel...\n" $$count; \
	\
	printf "$$missing_list" | tr ' ' '\n' | \
	xargs -P $(MAX_JOBS) -I{} sh -c '\
		api_id="{}"; \
		local_id=$$((api_id - 1)); \
		dir="$(TYPE_DIR)"; \
		outfile="$$dir/$$local_id.png"; \
		printf "Downloading type sprite #%d (saving as %d)...\n" "$$api_id" "$$local_id"; \
		sh tools/download_with_retry.sh "$(TYPE_BASE_URL)/$$api_id.png" "$$outfile" \
			|| { printf "Failed type #%s after retries\n" "$$api_id"; exit 1; }'; \
	ret=$$?; \
	if [ $$ret -ne 0 ]; then exit $$ret; fi

.PHONY: types

#---------------------------------------------------------------------------------
# UI font download (Nunito, SIL Open Font License — free to bundle/redistribute)
#---------------------------------------------------------------------------------
FONT_DIR         := romfs/fonts
FONT_FILE        := $(FONT_DIR)/Nunito.ttf
FONT_URL         := https://github.com/google/fonts/raw/main/ofl/nunito/Nunito%5Bwght%5D.ttf
# Fallback fonts for glyphs Nunito lacks. Noto Sans SC covers Simplified Chinese,
# Symbols covers gender ♂/♀ + star ★, and Symbols2 covers the card-suit heart ♥.
CJK_FONT_FILE     := $(FONT_DIR)/NotoSansSC.ttf
CJK_FONT_URL      := https://github.com/google/fonts/raw/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf
SYMBOL_FONT_FILE  := $(FONT_DIR)/NotoSansSymbols.ttf
SYMBOL_FONT_URL   := https://github.com/google/fonts/raw/main/ofl/notosanssymbols/NotoSansSymbols%5Bwght%5D.ttf
SYMBOL2_FONT_FILE := $(FONT_DIR)/NotoSansSymbols2.ttf
SYMBOL2_FONT_URL  := https://github.com/google/fonts/raw/main/ofl/notosanssymbols2/NotoSansSymbols2-Regular.ttf

# Download $(2) to $(1) if missing. $(3) = human label.
define fetch_font
	@if [ -f "$(1)" ] && [ -s "$(1)" ]; then \
		printf "%s already present.\n" "$(3)"; \
	else \
		printf "Downloading %s (SIL OFL)...\n" "$(3)"; \
		if command -v curl >/dev/null 2>&1; then \
			curl -fsSL -g "$(2)" -o "$(1)"; \
		else \
			wget -q "$(2)" -O "$(1)"; \
		fi || { printf "Failed to download %s\n" "$(3)"; exit 1; }; \
	fi
endef

fonts:
	@printf "Checking UI fonts...\n"
	@mkdir -p "$(FONT_DIR)"
	$(call fetch_font,$(FONT_FILE),$(FONT_URL),Nunito)
	$(call fetch_font,$(CJK_FONT_FILE),$(CJK_FONT_URL),Noto Sans SC)
	$(call fetch_font,$(SYMBOL_FONT_FILE),$(SYMBOL_FONT_URL),Noto Sans Symbols)
	$(call fetch_font,$(SYMBOL2_FONT_FILE),$(SYMBOL2_FONT_URL),Noto Sans Symbols 2)

.PHONY: fonts

# HD Pokemon sprites are NOT downloaded here -- fetch + downscale them from PokeAPI HOME
# renders with 'python tools/gen_hdsprites.py' (needs Pillow) into romfs/sprites/pokemon_hd/.
# Run it once, and after bumping its pinned PokeAPI ref; make / make all assume the sprites
# are already present (like the font).

#---------------------------------------------------------------------------------
.PHONY: $(BUILD) clean all

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@printf "build mode: %s\n" "$(BUILD_MODE)"
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile all

#---------------------------------------------------------------------------------
clean:
	@printf "clean ...\n"
ifeq ($(strip $(APP_JSON)),)
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf $(TARGET).lst
else
	@rm -fr $(BUILD) $(TARGET).nsp $(TARGET).nso $(TARGET).npdm $(TARGET).elf $(TARGET).lst
endif

#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
ifeq ($(strip $(APP_JSON)),)

all	:	$(OUTPUT).nro

ifeq ($(strip $(NO_NACP)),)
$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp
else
$(OUTPUT).nro	:	$(OUTPUT).elf
endif

else

all	:	$(OUTPUT).nsp

$(OUTPUT).nsp	:	$(OUTPUT).nso $(OUTPUT).npdm

$(OUTPUT).nso	:	$(OUTPUT).elf

endif

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@printf $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
