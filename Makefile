#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif
export LIBOGC_INC	:=	$(DEVKITPRO)/libogc/include
export LIBOGC_LIB	:=	$(DEVKITPRO)/libogc/lib/wii
export PORTLIBS		:=	$(DEVKITPRO)/portlibs/ppc

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET		:=	disc2app
BUILD		:=	build
SOURCES		:=	src libfat src/polarssl
DATA		:=  
INCLUDES	:=  src include libfat/include payload
DEFS        :=  
#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
CFLAGS	:=	-std=gnu2x -g -Wall -Ofast -ffunction-sections \
			$(MACHDEP) $(INCLUDE) -D__WIIU__ -D__WUT__

CXXFLAGS	:= -std=gnu++20 -g -Wall -Wno-int-in-bool-context -Wno-format-overflow -Ofast -fpermissive -ffunction-sections \
			$(MACHDEP) $(INCLUDE) -D__WIIU__ -D__WUT__

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lfat -lwut -liosuhax

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:= $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr $(CURDIR)/libfat

#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
DEFFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.def)))
BINFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#-------------------------------------------------------------------------------
	export LD	:=	$(CC)
#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

export OFILES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
					$(sFILES:.s=.o) $(SFILES:.S=.o) \
					$(PNGFILES:.png=.png.o) $(addsuffix .o,$(BINFILES))
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)  \
			-I/opt/devkitpro/portlibs/ppc/include/freetype2/

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(CURDIR)/libfat/lib


.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
$(BUILD): $(CURDIR)/payload/arm_kernel_bin.h
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(CURDIR)/payload/arm_kernel_bin.h:  $(CURDIR)/payload/wupserver_bin.h $(CURDIR)/payload/arm_user_bin.h $(CURDIR)/payload/odmhook_bin.h
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_kernel -f  $(CURDIR)/arm_kernel/Makefile
	@-mkdir -p $(CURDIR)/payload
	@cp -p $(CURDIR)/arm_kernel/arm_kernel_bin.h $@

$(CURDIR)/payload/wupserver_bin.h:
	@$(MAKE) --no-print-directory -C $(CURDIR)/wupserver -f  $(CURDIR)/wupserver/Makefile
	@-mkdir -p $(CURDIR)/payload
	@cp -p $(CURDIR)/wupserver/wupserver_bin.h $@

$(CURDIR)/payload/odmhook_bin.h:
	@$(MAKE) --no-print-directory -C $(CURDIR)/odmhook -f  $(CURDIR)/odmhook/Makefile
	@-mkdir -p $(CURDIR)/payload
	@cp -p $(CURDIR)/odmhook/odmhook_bin.h $@

$(CURDIR)/payload/arm_user_bin.h:
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_user -f  $(CURDIR)/arm_user/Makefile
	@-mkdir -p $(CURDIR)/payload
	@cp -p $(CURDIR)/arm_user/arm_user_bin.h $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).bin $(BUILD_DBG).elf $(CURDIR)/payload
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_user -f  $(CURDIR)/arm_user/Makefile clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/wupserver -f  $(CURDIR)/wupserver/Makefile clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/odmhook -f  $(CURDIR)/odmhook/Makefile clean
	@$(MAKE) --no-print-directory -C $(CURDIR)/arm_kernel -f  $(CURDIR)/arm_kernel/Makefile clean


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all	:	$(OUTPUT).rpx

$(OUTPUT).rpx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(DEFFILES:.def=.o) $(HFILES_BIN)

#---------------------------------------------------------------------------------
%.a:
#---------------------------------------------------------------------------------
	@echo $(notdir $@)
	@rm -f $@
	@$(AR) -rc $@ $^

#---------------------------------------------------------------------------------
%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
%.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
%.o: %.S
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@ $(ERROR_FILTER)

#---------------------------------------------------------------------------------
%.png.o : %.png
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.jpg.o : %.jpg
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.ttf.o : %.ttf
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.bin.o : %.bin
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.wav.o : %.wav
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.mp3.o : %.mp3
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.ogg.o : %.ogg
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)

#---------------------------------------------------------------------------------
%.zip.o : %.zip
	@echo $(notdir $<)
	@bin2s -a 32 $< | $(AS) -o $(@)
#---------------------------------------------------------------------------------
%.o: %.def
	$(SILENTMSG) $(notdir $<)
	$(SILENTCMD)rplexportgen $< $*.s $(ERROR_FILTER)
	$(SILENTCMD)$(CC) -x assembler-with-cpp $(ASFLAGS) -c $*.s -o $@ $(ERROR_FILTER)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
