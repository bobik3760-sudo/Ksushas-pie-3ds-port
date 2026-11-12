#---------------------------------------------------------------------------------
# Ksusha cooks a pie 3  --  Nintendo 3DS homebrew (devkitARM + libctru + citro2d)
#   make        -> KsushaPie3DS.3dsx (+ .smdh with icon.png)   [run from Homebrew Launcher]
#   make cia    -> KsushaPie3DS.cia  (needs bannertool + makerom, uses banner.png/icon.png)
#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := KsushaPie3DS
BUILD           := build
SOURCES         := .
DATA            :=
INCLUDES        := .
ROMFS           := romfs

APP_TITLE       := Ksusha cooks a pie 3
APP_DESCRIPTION := Help Ksusha cook a pie - Pocket Code game
APP_AUTHOR      := Vova games studio
ICON            := icon.png
BANNER          := banner.png
BANNER_AUDIO    := banner.wav

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS   += $(INCLUDE) -D__3DS__ -std=gnu11
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
ASFLAGS  := -g $(ARCH)
LDFLAGS   = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS     := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS  := $(PORTLIBS) $(CTRULIB)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
export LD := $(CC)
export OFILES := $(CFILES:.c=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# assets live in one zip so the repo has no folders: unpack it to romfs/ on first build
ifeq ("$(wildcard $(ROMFS))","")
  $(shell unzip -q -o $(CURDIR)/assets.zip)
endif

export _3DSXDEPS := $(OUTPUT).smdh
export APP_ICON := $(TOPDIR)/$(ICON)
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)

.PHONY: $(BUILD) clean all cia

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

cia: all
	bannertool makebanner -i $(BANNER) -a $(BANNER_AUDIO) -o $(BUILD)/banner.bnr
	bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i $(ICON) -o $(BUILD)/icon.icn
	makerom -f cia -o $(TARGET).cia -elf $(TARGET).elf -rsf app.rsf -target t -exefslogo \
	        -banner $(BUILD)/banner.bnr -icon $(BUILD)/icon.icn \
	        -DAPP_TITLE="$(APP_TITLE)" -DAPP_ROMFS="$(CURDIR)/$(ROMFS)"

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cia

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(OUTPUT).smdh
$(OUTPUT).elf  : $(OFILES)

-include $(DEPENDS)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
