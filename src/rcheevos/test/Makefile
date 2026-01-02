# supported parameters
#  ARCH                  architecture - "x86" or "x64" [detected if not set]
#  BUILD                 use flags for specified upstream consumer - "c89" or "retroarch" [default to "c89"]
#  DEBUG                 if set to anything, builds with DEBUG symbols
#  HAVE_HASH             if set to 0, excludes all hash functionality
#  HAVE_HASH_ROM         if set to 0, excludes rom hash generation
#  HAVE_HASH_DISC        if set to 0, excludes disc hash generation
#  HAVE_HASH_ZIP         if set to 0, excludes zip hash generation
#  HAVE_HASH_ENCRYPTED   if set to 0, excludes encrypted hash generation

RC_SRC=../src
RC_CHEEVOS_SRC=$(RC_SRC)/rcheevos
RC_HASH_SRC=$(RC_SRC)/rhash
RC_API_SRC=$(RC_SRC)/rapi

# default parameter values
ifeq ($(ARCH),)
    UNAME := $(shell uname -s)
    ifeq ($(findstring MINGW64, $(UNAME)), MINGW64)
        ARCH=x64
    else ifeq ($(findstring MINGW32, $(UNAME)), MINGW32)
        ARCH=x86
    else
        $(error Could not determine ARCH)
    endif
endif

ifeq ($(BUILD),)
    BUILD=c89
endif

# OS specific stuff
ifeq ($(OS),Windows_NT)
    EXE=.exe
else ifeq ($(findstring mingw, $(CC)), mingw)
    EXE=.exe
else
    EXE=
endif

# source files
OBJ=$(RC_SRC)/rc_compat.o \
    $(RC_SRC)/rc_client.o \
    $(RC_SRC)/rc_util.o \
    $(RC_SRC)/rc_version.o \
    $(RC_CHEEVOS_SRC)/alloc.o \
    $(RC_CHEEVOS_SRC)/condition.o \
    $(RC_CHEEVOS_SRC)/condset.o \
    $(RC_CHEEVOS_SRC)/consoleinfo.o \
    $(RC_CHEEVOS_SRC)/format.o \
    $(RC_CHEEVOS_SRC)/lboard.o \
    $(RC_CHEEVOS_SRC)/memref.o \
    $(RC_CHEEVOS_SRC)/operand.o \
    $(RC_CHEEVOS_SRC)/rc_validate.o \
    $(RC_CHEEVOS_SRC)/richpresence.o \
    $(RC_CHEEVOS_SRC)/runtime.o \
    $(RC_CHEEVOS_SRC)/runtime_progress.o \
    $(RC_CHEEVOS_SRC)/trigger.o \
    $(RC_CHEEVOS_SRC)/value.o \
    $(RC_HASH_SRC)/md5.o \
    $(RC_API_SRC)/rc_api_common.o \
    $(RC_API_SRC)/rc_api_editor.o \
    $(RC_API_SRC)/rc_api_info.o \
    $(RC_API_SRC)/rc_api_runtime.o \
    $(RC_API_SRC)/rc_api_user.o \
    rcheevos/test_condition.o \
    rcheevos/test_condset.o \
    rcheevos/test_consoleinfo.o \
    rcheevos/test_format.o \
    rcheevos/test_lboard.o \
    rcheevos/test_memref.o \
    rcheevos/test_operand.o \
    rcheevos/test_rc_validate.o \
    rcheevos/test_richpresence.o \
    rcheevos/test_runtime.o \
    rcheevos/test_runtime_progress.o \
    rcheevos/test_timing.o \
    rcheevos/test_trigger.o \
    rcheevos/test_value.o \
    rapi/test_rc_api_common.o \
    rapi/test_rc_api_editor.o \
    rapi/test_rc_api_info.o \
    rapi/test_rc_api_runtime.o \
    rapi/test_rc_api_user.o \
    test_rc_client.o \
    test.o

# compile flags
CFLAGS=-Wall -Werror -Wno-long-long -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
INCLUDES=-I../include -I$(RC_CHEEVOS_SRC) -I.

ifeq ($(ARCH), x86)
    CFLAGS += -m32
    LDFLAGS += -m32
else ifeq ($(ARCH), x64)
    CFLAGS += -m64
    LDFLAGS += -m64
else
    $(error unknown ARCH "$(ARCH)")
endif

EXTRA=
ifdef DEBUG
    CFLAGS += -O0 -g
    EXTRA +=  DEBUG
else
    CFLAGS += -O3
endif

# more strict validation for source files to eliminate warnings/errors in upstream consumers
# -Wextra includes -Wsign-compare -Wtype-limits -Wimplicit-fallthrough=3 -Wunused-parameter and more
# -pedantic issues errors when using GNU extensions or other things that are not strictly ISO C
SRC_CFLAGS=-Wextra -Wshadow -pedantic
# 3DS build (retroarch) doesn't support signed char
SRC_CFLAGS += -fno-signed-char

ifeq ($(BUILD), c89)
    CFLAGS += -std=c89
else ifeq ($(BUILD), c99)
    CFLAGS += -std=c99
else ifeq ($(BUILD), retroarch)
    # RetroArch builds with gcc8 and gnu99 which adds some extra warning validations
    SRC_CFLAGS += -std=gnu99 -D_GNU_SOURCE -Wall -Warray-bounds=2 -Wincompatible-pointer-types
    # Also include the RC_CLIENT_SUPPORTS_EXTERNAL flag to ensure we do the full level of validation on the
    # most code. The c89 build will ensure things still build without the RC_CLIENT_SUPPORTS_EXTERNAL flag.
    CFLAGS += -DRC_CLIENT_SUPPORTS_EXTERNAL
    OBJ += $(RC_SRC)/rc_client_external.o test_rc_client_external.o
else
    $(error unknown BUILD "$(BUILD)")
endif

# support for building without hash subdirectory
ifeq ($(HAVE_HASH), 0)
    EXTRA += |NO_HASH
else
    CFLAGS += -DRC_CLIENT_SUPPORTS_HASH
    OBJ += $(RC_HASH_SRC)/hash.o \
           $(RC_SRC)/rc_libretro.o \
           rhash/data.o \
           rhash/mock_filereader.o \
           rhash/test_hash.o \
           test_rc_libretro.o

    ifeq ($(HAVE_HASH_ROM), 0)
        EXTRA += |NO_HASH_ROM
        CFLAGS += -DRC_HASH_NO_ROM
    else
        OBJ += $(RC_HASH_SRC)/hash_rom.o \
               rhash/test_hash_rom.o
    endif

    ifeq ($(HAVE_HASH_DISC), 0)
        EXTRA += |NO_HASH_DISC
        CFLAGS += -DRC_HASH_NO_DISC
    else
        OBJ += $(RC_HASH_SRC)/cdreader.o \
               $(RC_HASH_SRC)/hash_disc.o \
               rhash/test_cdreader.o \
               rhash/test_hash_disc.o
    endif

    ifeq ($(HAVE_HASH_ZIP), 0)
        EXTRA += |NO_HASH_ZIP
        CFLAGS += -DRC_HASH_NO_ZIP
    else
        OBJ += $(RC_HASH_SRC)/hash_zip.o \
               rhash/test_hash_zip.o
    endif

    ifeq ($(HAVE_HASH_ENCRYPTED), 0)
        EXTRA += |NO_HASH_ENCRYPTED
        CFLAGS += -DRC_HASH_NO_ENCRYPTED
    else
        OBJ += $(RC_HASH_SRC)/hash_encrypted.o \
               $(RC_HASH_SRC)/aes.o
    endif
endif

# recipes
$(info ==== rcheevos test [$(BUILD)/$(ARCH)$(EXTRA)] ====)

all: test

$(RC_SRC)/%.o: $(RC_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_CHEEVOS_SRC)/%.o: $(RC_CHEEVOS_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_HASH_SRC)/%.o: $(RC_HASH_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

$(RC_API_SRC)/%.o: $(RC_API_SRC)/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

test: $(OBJ)
	$(CC) $(LDFLAGS) -o $@$(EXE) $+ -lm
	@echo ---------------------------------

check_ctype:
	@if grep -rnI "isalpha([^(u]" ../src/*; \
	then echo "*** Error: isalpha without unsigned char cast" && false; \
	fi
	@if grep -rnI "isalnum([^(u]" ../src/*; \
	then echo "*** Error: isalnum without unsigned char cast" && false; \
	fi
	@if grep -rnI "isdigit([^(u]" ../src/*; \
	then echo "*** Error: isdigit without unsigned char cast" && false; \
	fi
	@if grep -rnI "isspace([^(u]" ../src/*; \
	then echo "*** Error: isspace without unsigned char cast" && false; \
	fi

runtests: test
	@./test$(EXE)

valgrind: test
	@valgrind --leak-check=full --error-exitcode=1 ./test$(EXE)

clean:
	rm -f test$(EXE) $(OBJ)
