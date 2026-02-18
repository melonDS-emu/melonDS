# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Adrian "asie" Siekierka, 2025

BIOS_FILE_NTR_ARM9 = bios_ntr_arm9.bin
BIOS_FILE_NTR_ARM7 = bios_ntr_arm7.bin
BIOS_FILE_TWL_ARM9 = bios_twl_arm9.bin
BIOS_FILE_TWL_ARM7 = bios_twl_arm7.bin

BIOS_FILES = \
	$(BIOS_FILE_NTR_ARM9) \
	$(BIOS_FILE_NTR_ARM7) \
	$(BIOS_FILE_TWL_ARM9) \
	$(BIOS_FILE_TWL_ARM7)

BIOS_HEADERS = \
	 $(patsubst %.bin,%.h,$(BIOS_FILES))

BIOS_HEADER_ALL = FreeBIOS_Data.h

.PHONY: all clean $(BIOS_FILES) $(BIOS_HEADER_ALL)

all: $(BIOS_FILES) $(BIOS_HEADER_ALL)

$(BIOS_HEADER_ALL): $(BIOS_FILES)
	cat $(BIOS_HEADERS) > $(BIOS_HEADER_ALL)

$(BIOS_FILE_NTR_ARM9):
	$(MAKE) -f Makefile.bios TARGET=arm9 PLATFORM=ntr OUTPUT=$(BIOS_FILE_NTR_ARM9)

$(BIOS_FILE_NTR_ARM7):
	$(MAKE) -f Makefile.bios TARGET=arm7 PLATFORM=ntr OUTPUT=$(BIOS_FILE_NTR_ARM7)

$(BIOS_FILE_TWL_ARM9):
	$(MAKE) -f Makefile.bios TARGET=arm9 PLATFORM=twl OUTPUT=$(BIOS_FILE_TWL_ARM9)

$(BIOS_FILE_TWL_ARM7):
	$(MAKE) -f Makefile.bios TARGET=arm7 PLATFORM=twl OUTPUT=$(BIOS_FILE_TWL_ARM7)

clean:
	rm -fr build || true
	rm -r $(BIOS_FILES) || true
	rm -r $(BIOS_HEADERS) || true
	rm -r $(BIOS_HEADER_ALL) || true
