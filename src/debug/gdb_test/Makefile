
default: all

all: test-gdb

CPPFLAGS += -Werror=implicit-function-declaration -Werror=int-conversion \
    -Werror=return-type -Werror=uninitialized \
    -I../ -I../../ -Og -g -Wall \
    -Wno-switch -Wno-pointer-sign

obj/:
	@mkdir -vp "$@"

test-gdb: obj/GdbProto.o obj/GdbStub.o obj/GdbCmds.o obj/main.o obj/CRC32.o
	$(CXX) $(CPPFLAGS) $(LDFLAGS) -o "$@" $^

obj/Gdb%.o: ../Gdb%.cpp obj/
	$(CXX) $(CPPFLAGS) -c -o "$@" "$<"

obj/main.o: main.cpp obj/
	$(CXX) $(CPPFLAGS) -c -o "$@" "$<"

obj/CRC32.o: ../../CRC32.cpp obj/
	$(CXX) $(CPPFLAGS) -c -o "$@" "$<"

clean:
	@$(RM) -rv obj/ test-gdb

