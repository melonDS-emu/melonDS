
default: all

all: test-gdb

CFLAGS += -Werror=implicit-function-declaration -Werror=int-conversion \
    -Werror=return-type -Werror=uninitialized \
    -I../ -Og -g -Wall \
    -Wno-switch -Wno-pointer-sign

obj/:
	@mkdir -vp "$@"

test-gdb: obj/gdbproto.o obj/gdbstub.o obj/gdbcmds.o obj/main.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o "$@" $^

obj/gdb%.o: ../gdb%.c obj/
	$(CC) $(CFLAGS) -c -o "$@" "$<"

obj/main.o: main.c obj/
	$(CC) $(CFLAGS) -c -o "$@" "$<"

clean:
	@$(RM) -rv obj/ test-gdb

