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
SOURCES		:=	src src/Pokemon src/Encryption src/Enums src/UI src/UI/Panels src/UI/Dialogs src/UI/Modals src/Trainer src/Names src/Utils src/Save
DATA		:=	data
INCLUDES	:=	include
APP_TITLE   :=  PKSE
APP_AUTHOR  :=  Kiasta
APP_VERSION :=  0.0.3 		# TODO: We need to create a better way to update the version, probably setup a github action to automate releases
ROMFS		:=	romfs
ICON		:=  icon.jpg

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CFLAGS	:=	-g -Wall -O2 -ffunction-sections \
			$(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE) -D__SWITCH__ `$(PREFIX)pkg-config --cflags freetype2`

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions

CXXFLAGS	+=	-std=c++20

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lnx `$(PREFIX)pkg-config --libs freetype2` -lz -llz4

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

# Target when you run 'make all'. Downloads sprites, types, forms then builds
all: sprites types forms $(BUILD)

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
