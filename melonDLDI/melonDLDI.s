.arm
.text

.align 2

_start:
.word 0xBF8DA5ED
.string " Chishm"
.byte 1
.byte 9 @ size
.byte 0
.byte 0
.string "melonDS DLDI driver"
.align 6, 0
.word _start, melon_end
.word 0, 0
.word 0, 0
.word 0, 0
.ascii "MELN"
.word 0x23
.word melon_startup
.word melon_isInserted
.word melon_readSectors
.word melon_writeSectors
.word melon_clearStatus
.word melon_shutdown


.align 2

melon_startup:
	mov r0, #1
	bx lr
	
	
melon_isInserted:
	mov r0, #1
	bx lr
	
	
@ r0=cmd r1=sector r2=out (0=none)
_sendcmd:
	mov r12, #0x04000000
	add r12, r12, #0x1A0
	@ init
	mov r3, #0x8000
	strh r3, [r12]
	@ set cmd
	strb r0, [r12, #0x8]
	strb r1, [r12, #0xC]
	mov r1, r1, lsr #8
	strb r1, [r12, #0xB]
	mov r1, r1, lsr #8
	strb r1, [r12, #0xA]
	mov r1, r1, lsr #8
	strb r1, [r12, #0x9]
	mov r1, r1, lsr #8
	strb r1, [r12, #0xD]
	strh r1, [r12, #0xE]
	@ send
	mov r3, #0xA0000000
	orr r3, r3, r0, lsl #30
	cmp r2, #0
	orrne r3, r3, #0x01000000 @ block size
	orr r3, r3, #0x00400000 @ KEY2
	str r3, [r12, #0x4]
	mov r3, #0x04100000
	tst r0, #0x01
	bne __send_write
	@ receive data
__read_busyloop:
	ldr r0, [r12, #0x4]
	tst r0, #0x00800000
	ldrne r1, [r3, #0x10] @ load data
	cmpne r2, #0
	strne r1, [r2], #4
	tst r0, #0x80000000
	bne __read_busyloop
	bx lr
	@ send data
__send_write:
	mov r1, #0
__write_busyloop:
	ldr r0, [r12, #0x4]
	tst r0, #0x00800000
	cmpne r2, #0
	ldrne r1, [r2], #4
	strne r1, [r3, #0x10] @ store data
	tst r0, #0x80000000
	bne __write_busyloop
	bx lr
	
	
@ r0=sector r1=numsectors r2=out
melon_readSectors:
	tst r2, #0x3
	movne r0, #0
	bxne lr
	stmdb sp!, {r3-r6, lr}
	mov r4, r0
	mov r5, r1
	mov r6, #0
_readloop:
	mov r0, #0xC0
	add r1, r4, r6
	bl _sendcmd
	add r6, r6, #1
	cmp r6, r5
	bcc _readloop
	ldmia sp!, {r3-r6, lr}
	mov r0, #1
	bx lr
	
	
@ r0=sector r1=numsectors r2=out
melon_writeSectors:
	tst r2, #0x3
	movne r0, #0
	bxne lr
	stmdb sp!, {r3-r6, lr}
	mov r4, r0
	mov r5, r1
	mov r6, #0
_writeloop:
	mov r0, #0xC1
	add r1, r4, r6
	bl _sendcmd
	add r6, r6, #1
	cmp r6, r5
	bcc _writeloop
	ldmia sp!, {r3-r6, lr}
	mov r0, #1
	bx lr
	
	
melon_clearStatus:
	mov r0, #1
	bx lr
	
	
melon_shutdown:
	mov r0, #1
	bx lr
	
	
melon_end:
