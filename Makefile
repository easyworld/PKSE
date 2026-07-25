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
# THE version. Globals.h derives VERSION_STRING from this via -DPKSE_VERSION below, so this is the
# single source of truth -- the two can no longer drift.
# NOTE: no trailing comment on the assignment line. Make keeps trailing whitespace in a value, so
# "0.0.3 \t\t# ..." would have baked spaces into the .nacp version and the -D define.
APP_VERSION :=	1.0
ROMFS		:=	romfs
ICON		:=  icon.jpg

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

# NanoVG: disable its stb_image (we feed RGBA buffers via nvgCreateImageRGBA, and SpriteManager
# already owns the STB_IMAGE_IMPLEMENTATION) — avoids duplicate symbols. Keeps fontstash for text.
DEFINES	:=	-DNVG_NO_STB -DPKSE_VERSION='"$(APP_VERSION)"'

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

# Target when you run 'make all'. Downloads sprites, types, forms, HD sprites, fonts then builds
all: sprites types forms hdsprites hdforms fonts $(BUILD)

#---------------------------------------------------------------------------------
# Sprite and icon download integration
#---------------------------------------------------------------------------------
SPRITE_DIR   := romfs/sprites/pokemon
TYPE_DIR     := romfs/sprites/types
SPRITE_START := 0
SPRITE_END   := 1025
MAX_JOBS     := 20        # increase the value if you want it to run faster

# Pokemon form sprite IDs to download (static/permanent forms only)
# Regional variants and special forms that have different appearances
# Note: Some IDs don't have sprites on PokeAPI and are omitted
FORM_SPRITE_IDS := \
	10001 10002 10003 \
	10004 10005 \
	10006 10007 \
	10008 10009 10010 10011 10012 \
	10016 \
	10019 10020 10021 \
	10022 10023 \
	10024 10025 \
	10027 10028 10029 10030 10031 10032 10033 10034 10035 10036 10037 10038 10039 10040 10041 \
	10086 10087 10088 10089 10090 \
	10091 10092 10093 10094 10095 10096 10097 10098 10099 10100 10101 10102 10103 10104 10105 \
	10106 10107 10108 10109 10110 10111 10112 10113 10114 10115 \
	10116 10117 10118 10119 10120 \
	10123 10124 10125 10126 \
	10127 10130 10131 10132 10133 10134 \
	10152 \
	10155 10156 10157 \
	10161 10162 10163 10164 10165 10166 10167 10168 10169 10170 \
	10171 10172 10173 10174 10175 10176 10177 10178 10179 10180 \
	10184 10185 10186 \
	10188 10189 \
	10191 10192 10193 10194 \
	10229 10230 10231 10232 10233 10234 10235 10236 10237 10238 10239 \
	10240 10241 10242 10243 10244 10245 10246 10247 10248 10249 \
	10250 10251 10252 10253 10254 10255 10256 10257 10258 10259 \
	10260 10261 10262 10263 \
	10272 10273 10274 10275 10276 10277

sprites:
	@printf "Checking and downloading missing Pokemon sprites...\n"
	@mkdir -p "$(SPRITE_DIR)"
	@missing_list=""; \
	for i in $$(seq $(SPRITE_START) $(SPRITE_END)); do \
		[ -f "$(SPRITE_DIR)/$$i.png" ] && [ -f "$(SPRITE_DIR)/$${i}s.png" ] || missing_list="$$missing_list $$i"; \
	done; \
	if [ -z "$$missing_list" ]; then \
		printf "All sprites already present — nothing to download.\n"; \
		exit 0; \
	fi; \
	count=0; \
	for i in $$missing_list; do count=$$((count + 1)); done; \
	printf "Downloading %d missing sprite(s) in parallel...\n" $$count; \
	\
	printf "$$missing_list" | tr ' ' '\n' | \
	xargs -n 1 -P $(MAX_JOBS) -I{} sh -c '\
		id="{}"; \
		dir="$(SPRITE_DIR)"; \
		normal="$$dir/$$id.png"; \
		shiny="$$dir/$${id}s.png"; \
		if [ ! -f "$$normal" ]; then \
			printf "Downloading normal sprite #%d...\n" "$$id"; \
			if command -v curl >/dev/null 2>&1; then \
				curl -fsSL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/$$id.png" -o "$$normal"; \
			else \
				wget -q "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/$$id.png" -O "$$normal"; \
			fi || { printf "Failed normal #$$id"; exit 1; } \
		fi; \
		if [ ! -f "$$shiny" ]; then \
			printf "Downloading shiny sprite #%d...\n" "$$id"; \
			if command -v curl >/dev/null 2>&1; then \
				curl -fsSL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/shiny/$$id.png" -o "$$shiny"; \
			else \
				wget -q "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/shiny/$$id.png" -O "$$shiny"; \
			fi || { echo "Failed shiny #$$id"; exit 1; } \
		fi'; \
	ret=$$?; \
	if [ $$ret -ne 0 ]; then exit $$ret; fi

.PHONY: sprites

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
		[ -f "$(TYPE_DIR)/$$local_id.png" ] || missing_list="$$missing_list $$i"; \
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
	xargs -n 1 -P $(MAX_JOBS) -I{} sh -c '\
		api_id="{}"; \
		local_id=$$((api_id - 1)); \
		dir="$(TYPE_DIR)"; \
		outfile="$$dir/$$local_id.png"; \
		printf "Downloading type sprite #%d (saving as %d)...\n" "$$api_id" "$$local_id"; \
		if command -v curl >/dev/null 2>&1; then \
			curl -fsSL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/types/generation-ix/scarlet-violet/$$api_id.png" -o "$$outfile"; \
		else \
			wget -q "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/types/generation-ix/scarlet-violet/$$api_id.png" -O "$$outfile"; \
		fi || { printf "Failed type #$$api_id\n"; exit 1; }'; \
	ret=$$?; \
	if [ $$ret -ne 0 ]; then exit $$ret; fi

.PHONY: types

#---------------------------------------------------------------------------------
# Form sprite download (regional variants, special forms)
# Downloads sprites for Pokemon with alternate permanent forms
#---------------------------------------------------------------------------------
forms:
	@printf "Checking and downloading missing form sprites...\n"
	@mkdir -p "$(SPRITE_DIR)"
	@missing_list=""; \
	for id in $(FORM_SPRITE_IDS); do \
		[ -f "$(SPRITE_DIR)/$$id.png" ] || missing_list="$$missing_list $$id"; \
	done; \
	if [ -z "$$missing_list" ]; then \
		printf "All form sprites already present — nothing to download.\n"; \
		exit 0; \
	fi; \
	count=0; \
	for i in $$missing_list; do count=$$((count + 1)); done; \
	printf "Downloading %d missing form sprite(s) in parallel...\n" $$count; \
	\
	printf "$$missing_list" | tr ' ' '\n' | \
	xargs -n 1 -P $(MAX_JOBS) -I{} sh -c '\
		id="{}"; \
		dir="$(SPRITE_DIR)"; \
		normal="$$dir/$$id.png"; \
		shiny="$$dir/$${id}s.png"; \
		if [ ! -f "$$normal" ]; then \
			printf "Downloading form sprite #%d...\n" "$$id"; \
			if command -v curl >/dev/null 2>&1; then \
				curl -fsSL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/$$id.png" -o "$$normal" 2>/dev/null; \
			else \
				wget -q "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/$$id.png" -O "$$normal" 2>/dev/null; \
			fi; \
			if [ ! -f "$$normal" ] || [ ! -s "$$normal" ]; then \
				printf "Form sprite #%d not available, skipping...\n" "$$id"; \
				rm -f "$$normal" 2>/dev/null; \
			fi \
		fi; \
		if [ ! -f "$$shiny" ]; then \
			if command -v curl >/dev/null 2>&1; then \
				curl -fsSL "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/shiny/$$id.png" -o "$$shiny" 2>/dev/null; \
			else \
				wget -q "https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/shiny/$$id.png" -O "$$shiny" 2>/dev/null; \
			fi; \
			if [ ! -f "$$shiny" ] || [ ! -s "$$shiny" ]; then \
				rm -f "$$shiny" 2>/dev/null; \
			fi \
		fi'

.PHONY: forms

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

#---------------------------------------------------------------------------------
# HD sprite download — Pokemon HOME renders (transparent 256px PNGs) from pokemondb.
# Fetched BY DEX NUMBER using the name map in tools/hd_sprite_names.txt (line N = dex N),
# saved as <id>.png / <id>s.png. 256px so the big editor/summary renders stay crisp (drawn up to
# ~196px). Preferred over the 96px PokeAPI sprites at runtime; any that 404 fall back to the 96px.
#---------------------------------------------------------------------------------
HD_SPRITE_DIR := romfs/sprites/pokemon_hd
HD_NAMES      := tools/hd_sprite_names.txt
HD_BASE       := https://img.pokemondb.net/sprites/home

hdsprites:
	@printf "Checking and downloading missing HD Pokemon sprites...\n"
	@mkdir -p "$(HD_SPRITE_DIR)"
	@if [ ! -f "$(HD_NAMES)" ]; then printf "Missing $(HD_NAMES) -- cannot fetch HD sprites\n"; exit 1; fi
	@awk '{ print NR "|" $$0 }' "$(HD_NAMES)" | \
	xargs -P $(MAX_JOBS) -I{} sh -c '\
		pair="{}"; id="$${pair%%|*}"; name="$${pair#*|}"; \
		dir="$(HD_SPRITE_DIR)"; base="$(HD_BASE)"; \
		normal="$$dir/$$id.png"; shiny="$$dir/$${id}s.png"; \
		if [ ! -f "$$normal" ]; then \
			curl -fsSL "$$base/normal/$$name.png" -o "$$normal" 2>/dev/null; \
			[ -s "$$normal" ] || rm -f "$$normal" 2>/dev/null; \
		fi; \
		if [ ! -f "$$shiny" ]; then \
			curl -fsSL "$$base/shiny/$$name.png" -o "$$shiny" 2>/dev/null; \
			[ -s "$$shiny" ] || rm -f "$$shiny" 2>/dev/null; \
		fi'
	@printf "HD sprites present: %s files.\n" "$$(ls -1 $(HD_SPRITE_DIR) 2>/dev/null | wc -l)"

.PHONY: hdsprites

#---------------------------------------------------------------------------------
# HD form sprites -- regional variants and alternate forms (task #13).
#
# Same source and size as the base-species set above: pokemondb HOME renders at 256px.
# PokeAPI would have been simpler (it keys by the same numeric ids as FORM_SPRITE_IDS,
# so no name map at all) but only serves 512px -- 4x the bytes and 4x the DECODED memory
# for no visible gain, since the UI draws at most ~196px and the sprite cache does not
# evict. So forms use pokemondb too, via a generated id -> name map.
#
# tools/gen_hdform_names.py builds that map from PokeAPI's own form names and FETCHES
# every candidate URL before accepting it -- a trimmed name very often exists but is a
# different Pokemon ('tauros-paldea-combat-breed' -> 'tauros' is a valid URL and the
# wrong sprite), so it refuses to emit a partial or region-dropping map.
#
# Presence is judged on the NORMAL sprite alone: a few forms (the Pikachu cap forms)
# have no shiny render, so requiring both would re-attempt permanent 404s every run.
#
# SpriteManager already prefers sprites/pokemon_hd/<id>.png and falls back to the 96px
# copy, so these need no code change: dropping the files in IS the feature.
#---------------------------------------------------------------------------------
HD_FORM_NAMES := tools/hd_form_names.txt
HD_FORM_BASE  := https://img.pokemondb.net/sprites/home

hdforms:
	@printf "Checking and downloading missing HD form sprites...\n"
	@mkdir -p "$(HD_SPRITE_DIR)"
	@if [ ! -f "$(HD_FORM_NAMES)" ]; then printf "Missing $(HD_FORM_NAMES) - run: python tools/gen_hdform_names.py\n"; exit 1; fi
	@grep -v '^#' "$(HD_FORM_NAMES)" | \
	xargs -P $(MAX_JOBS) -I{} sh -c '\
		pair="{}"; id="$${pair%%|*}"; name="$${pair#*|}"; \
		dir="$(HD_SPRITE_DIR)"; base="$(HD_FORM_BASE)"; \
		normal="$$dir/$$id.png"; shiny="$$dir/$${id}s.png"; \
		if [ ! -f "$$normal" ]; then \
			curl -fsSL "$$base/normal/$$name.png" -o "$$normal" 2>/dev/null; \
			[ -s "$$normal" ] || rm -f "$$normal" 2>/dev/null; \
		fi; \
		if [ ! -f "$$shiny" ]; then \
			curl -fsSL "$$base/shiny/$$name.png" -o "$$shiny" 2>/dev/null; \
			[ -s "$$shiny" ] || rm -f "$$shiny" 2>/dev/null; \
		fi'
	@printf "HD sprite dir now holds %s files.\n" "$$(ls -1 $(HD_SPRITE_DIR) 2>/dev/null | wc -l)"

.PHONY: hdforms

#---------------------------------------------------------------------------------
.PHONY: $(BUILD) clean all

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
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
