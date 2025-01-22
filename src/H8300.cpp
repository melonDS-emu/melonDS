/*
	This file is copyright Barret Klics and maybe melonDS team I guess?
   */


#include "H8300.h"
#include <string.h>
#include "types.h"
H8300::H8300(){
	printf("H8/300 has been created");
}

H8300::~H8300(){

	printf("H8/300 has been killed");

}

void H8300::speak(){
	printf("H8 CHIP IS SPEAKING");
}


//A botched implementation
u8 H8300::handleSPI(u8& IRCmd, u8& val, u32&pos, bool& last){

    printf("IRCmd: %d		val:%d   pos: %ld   last: %d\n", IRCmd, val, pos, last);
    switch (IRCmd)
    {
    case 0x00: // pass-through
	printf("CRITICAL ERROR IN H8300 emu, should not have recieved 0x00 command!");
        return 0xFF;


    case 0x01:
	u8 rtnval;
	if (pos == 1){
		rtnval = 0x01;
	}
	if (pos == 2){
		rtnval = 0x56;
	}
	printf("	returning: 0x%02x   val: %d   pos: %ld   last: %d\n", rtnval, val, pos, last);
	return rtnval;


    case 0x08: // ID
        return 0xAA;
    }

    printf("Unhandled: %d		val:%d   pos: %ld   last: %d\n", IRCmd, val, pos, last);
    return 0x00;
}
