AS         = arm-none-eabi-as
LD         = arm-none-eabi-ld
OBJCOPY    = arm-none-eabi-objcopy

BIN        = melonDLDI

all:
	$(AS) $(BIN).s -o $(BIN).o
	$(LD) $(BIN).o -Ttext 0xBF800000 -e 0xBF800000 -o $(BIN).elf
	$(OBJCOPY) -O binary $(BIN).elf $(BIN).bin
	xxd -i -n $(BIN) -c 16 $(BIN).bin $(BIN).h

clean:
	rm -f $(BIN).h $(BIN).bin $(BIN).elf $(BIN).o

