#---------------------------------------------------------------------------------
# Syobon Action U - Nintendo Wii U port of OpenSyobonAction
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitPro")
endif

#---------------------------------------------------------------------------------
# Application metadata.
# These MUST be defined before wut_rules is included: wut_rules expands
# WUHB_OPTIONS at parse time, so late definitions would be ignored.
#---------------------------------------------------------------------------------
APP_NAME	:=	Syobon Action U
APP_SHORTNAME	:=	SyobonActionU
APP_AUTHOR	:=	PortBREW

APP_ICON	:=	meta/icon.png
APP_TV_SPLASH	:=	meta/tv-splash.png
APP_DRC_SPLASH	:=	meta/drc-splash.png
APP_CONTENT	:=	build/content

include $(DEVKITPRO)/wut/share/wut_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
TARGET		:=	SyobonActionU
BUILD		:=	build
SOURCES		:=	.
DATA		:=	data
INCLUDES	:=	.

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-DESPRESSO -mcpu=750 -meabi -mhard-float

CFLAGS		:=	-g -O2 -Wall -Wno-unused-variable -Wno-write-strings \
			$(ARCH) $(DEFINES)

CFLAGS		+=	$(INCLUDE) -D__WIIU__ -D__WUT__

CXXFLAGS	:=	$(CFLAGS) -std=gnu++11 -fpermissive \
			-Wno-narrowing -Wno-deprecated-declarations

LDFLAGS		=	-g $(ARCH) -specs=$(DEVKITPRO)/wut/share/wut.specs
LIBS		:=	-lSDL2_mixer -lSDL2_ttf -lSDL2_image -lSDL2_gfx -lSDL2 \
			-lvorbisfile -lvorbis -logg -lmodplug -lmpg123 \
			-lfreetype -lharfbuzz -lpng16 -lwebp -ljpeg -lz -lbz2 \
			-lbrotlidec -lbrotlicommon -lwut -lm

#---------------------------------------------------------------------------------
# standard devkitPro library paths
#---------------------------------------------------------------------------------
LIBDIRS		:=	$(PORTLIBS) $(WUT_ROOT)

#---------------------------------------------------------------------------------
# no real need to edit anything past this point
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
export LD	:=	$(CXX)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES		:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: all clean wuhb

#---------------------------------------------------------------------------------
# all: compile + link in the build sub-directory, then package the .wuhb
# from the top-level directory so the relative artwork paths resolve.
#---------------------------------------------------------------------------------
all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@$(MAKE) --no-print-directory -f $(CURDIR)/Makefile wuhb

# Stage the game assets into one content tree for wuhbtool.  The assets
# in BGM/, SE/ and res/ are the canonical copies (the same files the game
# loads from the SD card); staging avoids keeping a second 9 MB duplicate
# of them in the repository.
$(BUILD)/content: $(BUILD)
	@echo staging ... content
	@rm -rf $@
	@mkdir -p $@
	@cp -r BGM SE res $@/

$(BUILD):
	@mkdir -p $@

#---------------------------------------------------------------------------------
wuhb: $(TARGET).wuhb

$(TARGET).wuhb: $(BUILD)/content

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).rpx $(TARGET).elf $(TARGET).wuhb $(TARGET).lst

#---------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# Link the ELF and convert it to .rpx.  The .wuhb packaging is driven by the
# top-level Makefile so that the artwork/content paths are relative to the
# project root.
#---------------------------------------------------------------------------------
all: $(OUTPUT).rpx

$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
#---------------------------------------------------------------------------------
