RC_SRC=../src
RC_CHEEVOS_SRC=$(RC_SRC)/rcheevos
RC_HASH_SRC=$(RC_SRC)/rhash
RC_API_SRC=$(RC_SRC)/rapi

OBJ=$(RC_CHEEVOS_SRC)/alloc.o $(RC_CHEEVOS_SRC)/condition.o $(RC_CHEEVOS_SRC)/condset.o \
    $(RC_CHEEVOS_SRC)/consoleinfo.o $(RC_CHEEVOS_SRC)/format.o $(RC_CHEEVOS_SRC)/lboard.o \
    $(RC_CHEEVOS_SRC)/memref.o $(RC_CHEEVOS_SRC)/operand.o $(RC_CHEEVOS_SRC)/rc_validate.o \
    $(RC_CHEEVOS_SRC)/richpresence.o $(RC_CHEEVOS_SRC)/runtime.o $(RC_CHEEVOS_SRC)/trigger.o \
    $(RC_CHEEVOS_SRC)/value.o \
    $(RC_SRC)/rc_compat.o $(RC_SRC)/rc_util.o \
    $(RC_HASH_SRC)/md5.o \
    $(RC_API_SRC)/rc_api_common.o $(RC_API_SRC)/rc_api_runtime.o
    validator.o

all: validator

%.o: %.c
	gcc -Wall -O0 -g -std=c89 -ansi -Wno-long-long -I../include -I$(RC_CHEEVOS_SRC) -c $< -o $@

validator: $(OBJ)
	gcc -o $@ $+ -lm

clean:
	rm -f test $(OBJ)
