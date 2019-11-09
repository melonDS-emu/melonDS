/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008-2017 DeSmuME team
	
    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

/* This file was taken from DeSmuME. The following modifications were made:
	#includes were removed/added as needed
	#defines were taken from other DeSmuME files and placed here
	All swi table methods were given a parameter (ARM* cpu); DeSmuME had a #define for cpu, which was removed
	cpu->frozen				-> cpu->Halted
	_MMU_read/write			-> cpu->DataRead/Write
	cp15.ctrl				-> (ARMv5*)cpu->CP15Control
	cp15.DTCMRegion			-> (ARMv5*)cpu->DTCMBase
	cpu->instruct_adr		-> cpu->NextInstr[0]
	cpu->next_instruction	-> cpu->NextInstr[1]
	isDebugger is always false; I don't see an equivalent feature in MelonDS, but I'm not familiar enough to know it isn't somewhere.

	Elsewhere in MelonDS:
	the ARM class was given new fields, u8 intrWaitARM_state and bool useHLE_bios
	NDS::Reset loads fake bios content, taken from DeSmuME's NDSSystem.cpp PrepareBiosARM7/9
	ARMInterpreter::A_SVC and T_SVC modified to conditionally use this HLE bios.
*/

//this file contains HLE for the arm9 and arm7 bios SWI routines.
//it renders the use of bios files generally unnecessary.
//it turns out that they're not too complex, although of course the timings will be all wrong here.

#include <math.h>

#include "types.h"
#include "ARM.h"

#define TEMPLATE template <int PROCNUM>

// defines from files included in the desmume version
#define ARMCPU_ARM7 1
#define ARMCPU_ARM9 0

#define CPU_FREEZE_NONE 0x00
#define CPU_FREEZE_WAIT_IRQ 0x01 //waiting for some IRQ to happen, any IRQ
#define CPU_FREEZE_IE_IF 0x02	 //waiting for IE&IF to signal something (probably edge triggered on IRQ too)
#define CPU_FREEZE_IRQ_IE_IF (CPU_FREEZE_WAIT_IRQ | CPU_FREEZE_IE_IF)

#define REG_POSTFLG 0x04000300
#define REG_HALTCNT 0x04000301

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

u32 tmp;

struct CompressionHeader
{
public:
	CompressionHeader(u32 _value) : value(_value) {}
	u32 DataSize() const { return value & 15; }
	u32 Type() const { return (value >> 4) & 15; }
	u32 DecompressedSize() const { return value >> 8; }

private:
	u32 value;
};

static const u16 getsinetbl[] = {
	0x0000, 0x0324, 0x0648, 0x096A, 0x0C8C, 0x0FAB, 0x12C8, 0x15E2,
	0x18F9, 0x1C0B, 0x1F1A, 0x2223, 0x2528, 0x2826, 0x2B1F, 0x2E11,
	0x30FB, 0x33DF, 0x36BA, 0x398C, 0x3C56, 0x3F17, 0x41CE, 0x447A,
	0x471C, 0x49B4, 0x4C3F, 0x4EBF, 0x5133, 0x539B, 0x55F5, 0x5842,
	0x5A82, 0x5CB3, 0x5ED7, 0x60EB, 0x62F1, 0x64E8, 0x66CF, 0x68A6,
	0x6A6D, 0x6C23, 0x6DC9, 0x6F5E, 0x70E2, 0x7254, 0x73B5, 0x7504,
	0x7641, 0x776B, 0x7884, 0x7989, 0x7A7C, 0x7B5C, 0x7C29, 0x7CE3,
	0x7D89, 0x7E1D, 0x7E9C, 0x7F09, 0x7F61, 0x7FA6, 0x7FD8, 0x7FF5};

static const u16 getpitchtbl[] = {
	0x0000, 0x003B, 0x0076, 0x00B2, 0x00ED, 0x0128, 0x0164, 0x019F,
	0x01DB, 0x0217, 0x0252, 0x028E, 0x02CA, 0x0305, 0x0341, 0x037D,
	0x03B9, 0x03F5, 0x0431, 0x046E, 0x04AA, 0x04E6, 0x0522, 0x055F,
	0x059B, 0x05D8, 0x0614, 0x0651, 0x068D, 0x06CA, 0x0707, 0x0743,
	0x0780, 0x07BD, 0x07FA, 0x0837, 0x0874, 0x08B1, 0x08EF, 0x092C,
	0x0969, 0x09A7, 0x09E4, 0x0A21, 0x0A5F, 0x0A9C, 0x0ADA, 0x0B18,
	0x0B56, 0x0B93, 0x0BD1, 0x0C0F, 0x0C4D, 0x0C8B, 0x0CC9, 0x0D07,
	0x0D45, 0x0D84, 0x0DC2, 0x0E00, 0x0E3F, 0x0E7D, 0x0EBC, 0x0EFA,
	0x0F39, 0x0F78, 0x0FB6, 0x0FF5, 0x1034, 0x1073, 0x10B2, 0x10F1,
	0x1130, 0x116F, 0x11AE, 0x11EE, 0x122D, 0x126C, 0x12AC, 0x12EB,
	0x132B, 0x136B, 0x13AA, 0x13EA, 0x142A, 0x146A, 0x14A9, 0x14E9,
	0x1529, 0x1569, 0x15AA, 0x15EA, 0x162A, 0x166A, 0x16AB, 0x16EB,
	0x172C, 0x176C, 0x17AD, 0x17ED, 0x182E, 0x186F, 0x18B0, 0x18F0,
	0x1931, 0x1972, 0x19B3, 0x19F5, 0x1A36, 0x1A77, 0x1AB8, 0x1AFA,
	0x1B3B, 0x1B7D, 0x1BBE, 0x1C00, 0x1C41, 0x1C83, 0x1CC5, 0x1D07,
	0x1D48, 0x1D8A, 0x1DCC, 0x1E0E, 0x1E51, 0x1E93, 0x1ED5, 0x1F17,
	0x1F5A, 0x1F9C, 0x1FDF, 0x2021, 0x2064, 0x20A6, 0x20E9, 0x212C,
	0x216F, 0x21B2, 0x21F5, 0x2238, 0x227B, 0x22BE, 0x2301, 0x2344,
	0x2388, 0x23CB, 0x240E, 0x2452, 0x2496, 0x24D9, 0x251D, 0x2561,
	0x25A4, 0x25E8, 0x262C, 0x2670, 0x26B4, 0x26F8, 0x273D, 0x2781,
	0x27C5, 0x280A, 0x284E, 0x2892, 0x28D7, 0x291C, 0x2960, 0x29A5,
	0x29EA, 0x2A2F, 0x2A74, 0x2AB9, 0x2AFE, 0x2B43, 0x2B88, 0x2BCD,
	0x2C13, 0x2C58, 0x2C9D, 0x2CE3, 0x2D28, 0x2D6E, 0x2DB4, 0x2DF9,
	0x2E3F, 0x2E85, 0x2ECB, 0x2F11, 0x2F57, 0x2F9D, 0x2FE3, 0x302A,
	0x3070, 0x30B6, 0x30FD, 0x3143, 0x318A, 0x31D0, 0x3217, 0x325E,
	0x32A5, 0x32EC, 0x3332, 0x3379, 0x33C1, 0x3408, 0x344F, 0x3496,
	0x34DD, 0x3525, 0x356C, 0x35B4, 0x35FB, 0x3643, 0x368B, 0x36D3,
	0x371A, 0x3762, 0x37AA, 0x37F2, 0x383A, 0x3883, 0x38CB, 0x3913,
	0x395C, 0x39A4, 0x39ED, 0x3A35, 0x3A7E, 0x3AC6, 0x3B0F, 0x3B58,
	0x3BA1, 0x3BEA, 0x3C33, 0x3C7C, 0x3CC5, 0x3D0E, 0x3D58, 0x3DA1,
	0x3DEA, 0x3E34, 0x3E7D, 0x3EC7, 0x3F11, 0x3F5A, 0x3FA4, 0x3FEE,
	0x4038, 0x4082, 0x40CC, 0x4116, 0x4161, 0x41AB, 0x41F5, 0x4240,
	0x428A, 0x42D5, 0x431F, 0x436A, 0x43B5, 0x4400, 0x444B, 0x4495,
	0x44E1, 0x452C, 0x4577, 0x45C2, 0x460D, 0x4659, 0x46A4, 0x46F0,
	0x473B, 0x4787, 0x47D3, 0x481E, 0x486A, 0x48B6, 0x4902, 0x494E,
	0x499A, 0x49E6, 0x4A33, 0x4A7F, 0x4ACB, 0x4B18, 0x4B64, 0x4BB1,
	0x4BFE, 0x4C4A, 0x4C97, 0x4CE4, 0x4D31, 0x4D7E, 0x4DCB, 0x4E18,
	0x4E66, 0x4EB3, 0x4F00, 0x4F4E, 0x4F9B, 0x4FE9, 0x5036, 0x5084,
	0x50D2, 0x5120, 0x516E, 0x51BC, 0x520A, 0x5258, 0x52A6, 0x52F4,
	0x5343, 0x5391, 0x53E0, 0x542E, 0x547D, 0x54CC, 0x551A, 0x5569,
	0x55B8, 0x5607, 0x5656, 0x56A5, 0x56F4, 0x5744, 0x5793, 0x57E2,
	0x5832, 0x5882, 0x58D1, 0x5921, 0x5971, 0x59C1, 0x5A10, 0x5A60,
	0x5AB0, 0x5B01, 0x5B51, 0x5BA1, 0x5BF1, 0x5C42, 0x5C92, 0x5CE3,
	0x5D34, 0x5D84, 0x5DD5, 0x5E26, 0x5E77, 0x5EC8, 0x5F19, 0x5F6A,
	0x5FBB, 0x600D, 0x605E, 0x60B0, 0x6101, 0x6153, 0x61A4, 0x61F6,
	0x6248, 0x629A, 0x62EC, 0x633E, 0x6390, 0x63E2, 0x6434, 0x6487,
	0x64D9, 0x652C, 0x657E, 0x65D1, 0x6624, 0x6676, 0x66C9, 0x671C,
	0x676F, 0x67C2, 0x6815, 0x6869, 0x68BC, 0x690F, 0x6963, 0x69B6,
	0x6A0A, 0x6A5E, 0x6AB1, 0x6B05, 0x6B59, 0x6BAD, 0x6C01, 0x6C55,
	0x6CAA, 0x6CFE, 0x6D52, 0x6DA7, 0x6DFB, 0x6E50, 0x6EA4, 0x6EF9,
	0x6F4E, 0x6FA3, 0x6FF8, 0x704D, 0x70A2, 0x70F7, 0x714D, 0x71A2,
	0x71F7, 0x724D, 0x72A2, 0x72F8, 0x734E, 0x73A4, 0x73FA, 0x7450,
	0x74A6, 0x74FC, 0x7552, 0x75A8, 0x75FF, 0x7655, 0x76AC, 0x7702,
	0x7759, 0x77B0, 0x7807, 0x785E, 0x78B4, 0x790C, 0x7963, 0x79BA,
	0x7A11, 0x7A69, 0x7AC0, 0x7B18, 0x7B6F, 0x7BC7, 0x7C1F, 0x7C77,
	0x7CCF, 0x7D27, 0x7D7F, 0x7DD7, 0x7E2F, 0x7E88, 0x7EE0, 0x7F38,
	0x7F91, 0x7FEA, 0x8042, 0x809B, 0x80F4, 0x814D, 0x81A6, 0x81FF,
	0x8259, 0x82B2, 0x830B, 0x8365, 0x83BE, 0x8418, 0x8472, 0x84CB,
	0x8525, 0x857F, 0x85D9, 0x8633, 0x868E, 0x86E8, 0x8742, 0x879D,
	0x87F7, 0x8852, 0x88AC, 0x8907, 0x8962, 0x89BD, 0x8A18, 0x8A73,
	0x8ACE, 0x8B2A, 0x8B85, 0x8BE0, 0x8C3C, 0x8C97, 0x8CF3, 0x8D4F,
	0x8DAB, 0x8E07, 0x8E63, 0x8EBF, 0x8F1B, 0x8F77, 0x8FD4, 0x9030,
	0x908C, 0x90E9, 0x9146, 0x91A2, 0x91FF, 0x925C, 0x92B9, 0x9316,
	0x9373, 0x93D1, 0x942E, 0x948C, 0x94E9, 0x9547, 0x95A4, 0x9602,
	0x9660, 0x96BE, 0x971C, 0x977A, 0x97D8, 0x9836, 0x9895, 0x98F3,
	0x9952, 0x99B0, 0x9A0F, 0x9A6E, 0x9ACD, 0x9B2C, 0x9B8B, 0x9BEA,
	0x9C49, 0x9CA8, 0x9D08, 0x9D67, 0x9DC7, 0x9E26, 0x9E86, 0x9EE6,
	0x9F46, 0x9FA6, 0xA006, 0xA066, 0xA0C6, 0xA127, 0xA187, 0xA1E8,
	0xA248, 0xA2A9, 0xA30A, 0xA36B, 0xA3CC, 0xA42D, 0xA48E, 0xA4EF,
	0xA550, 0xA5B2, 0xA613, 0xA675, 0xA6D6, 0xA738, 0xA79A, 0xA7FC,
	0xA85E, 0xA8C0, 0xA922, 0xA984, 0xA9E7, 0xAA49, 0xAAAC, 0xAB0E,
	0xAB71, 0xABD4, 0xAC37, 0xAC9A, 0xACFD, 0xAD60, 0xADC3, 0xAE27,
	0xAE8A, 0xAEED, 0xAF51, 0xAFB5, 0xB019, 0xB07C, 0xB0E0, 0xB145,
	0xB1A9, 0xB20D, 0xB271, 0xB2D6, 0xB33A, 0xB39F, 0xB403, 0xB468,
	0xB4CD, 0xB532, 0xB597, 0xB5FC, 0xB662, 0xB6C7, 0xB72C, 0xB792,
	0xB7F7, 0xB85D, 0xB8C3, 0xB929, 0xB98F, 0xB9F5, 0xBA5B, 0xBAC1,
	0xBB28, 0xBB8E, 0xBBF5, 0xBC5B, 0xBCC2, 0xBD29, 0xBD90, 0xBDF7,
	0xBE5E, 0xBEC5, 0xBF2C, 0xBF94, 0xBFFB, 0xC063, 0xC0CA, 0xC132,
	0xC19A, 0xC202, 0xC26A, 0xC2D2, 0xC33A, 0xC3A2, 0xC40B, 0xC473,
	0xC4DC, 0xC544, 0xC5AD, 0xC616, 0xC67F, 0xC6E8, 0xC751, 0xC7BB,
	0xC824, 0xC88D, 0xC8F7, 0xC960, 0xC9CA, 0xCA34, 0xCA9E, 0xCB08,
	0xCB72, 0xCBDC, 0xCC47, 0xCCB1, 0xCD1B, 0xCD86, 0xCDF1, 0xCE5B,
	0xCEC6, 0xCF31, 0xCF9C, 0xD008, 0xD073, 0xD0DE, 0xD14A, 0xD1B5,
	0xD221, 0xD28D, 0xD2F8, 0xD364, 0xD3D0, 0xD43D, 0xD4A9, 0xD515,
	0xD582, 0xD5EE, 0xD65B, 0xD6C7, 0xD734, 0xD7A1, 0xD80E, 0xD87B,
	0xD8E9, 0xD956, 0xD9C3, 0xDA31, 0xDA9E, 0xDB0C, 0xDB7A, 0xDBE8,
	0xDC56, 0xDCC4, 0xDD32, 0xDDA0, 0xDE0F, 0xDE7D, 0xDEEC, 0xDF5B,
	0xDFC9, 0xE038, 0xE0A7, 0xE116, 0xE186, 0xE1F5, 0xE264, 0xE2D4,
	0xE343, 0xE3B3, 0xE423, 0xE493, 0xE503, 0xE573, 0xE5E3, 0xE654,
	0xE6C4, 0xE735, 0xE7A5, 0xE816, 0xE887, 0xE8F8, 0xE969, 0xE9DA,
	0xEA4B, 0xEABC, 0xEB2E, 0xEB9F, 0xEC11, 0xEC83, 0xECF5, 0xED66,
	0xEDD9, 0xEE4B, 0xEEBD, 0xEF2F, 0xEFA2, 0xF014, 0xF087, 0xF0FA,
	0xF16D, 0xF1E0, 0xF253, 0xF2C6, 0xF339, 0xF3AD, 0xF420, 0xF494,
	0xF507, 0xF57B, 0xF5EF, 0xF663, 0xF6D7, 0xF74C, 0xF7C0, 0xF834,
	0xF8A9, 0xF91E, 0xF992, 0xFA07, 0xFA7C, 0xFAF1, 0xFB66, 0xFBDC,
	0xFC51, 0xFCC7, 0xFD3C, 0xFDB2, 0xFE28, 0xFE9E, 0xFF14, 0xFF8A};

static const u8 getvoltbl[] = {
	0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
	0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x18,
	0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19, 0x1A, 0x1A, 0x1A, 0x1B, 0x1B, 0x1B, 0x1C, 0x1C, 0x1C,
	0x1D, 0x1D, 0x1D, 0x1E, 0x1E, 0x1E, 0x1F, 0x1F, 0x1F, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22,
	0x22, 0x23, 0x23, 0x24, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x27, 0x28, 0x28, 0x29,
	0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x31, 0x31,
	0x32, 0x32, 0x33, 0x33, 0x34, 0x35, 0x35, 0x36, 0x36, 0x37, 0x38, 0x38, 0x39, 0x3A, 0x3A, 0x3B,
	0x3C, 0x3C, 0x3D, 0x3E, 0x3F, 0x3F, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x52, 0x53, 0x54, 0x55,
	0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x67,
	0x68, 0x69, 0x6A, 0x6B, 0x6D, 0x6E, 0x6F, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A, 0x7B,
	0x7D, 0x7E, 0x7F, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25,
	0x26, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D,
	0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30, 0x31, 0x31, 0x32, 0x33, 0x33, 0x34, 0x34, 0x35, 0x36,
	0x36, 0x37, 0x37, 0x38, 0x39, 0x39, 0x3A, 0x3B, 0x3B, 0x3C, 0x3D, 0x3E, 0x3E, 0x3F, 0x40, 0x40,
	0x41, 0x42, 0x43, 0x43, 0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4D,
	0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
	0x5E, 0x5F, 0x60, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6F, 0x70,
	0x71, 0x73, 0x74, 0x75, 0x77, 0x78, 0x79, 0x7B, 0x7C, 0x7E, 0x7E, 0x40, 0x41, 0x42, 0x43, 0x43,
	0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51,
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61,
	0x62, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6B, 0x6C, 0x6D, 0x6E, 0x70, 0x71, 0x72, 0x74, 0x75,
	0x76, 0x78, 0x79, 0x7B, 0x7C, 0x7D, 0x7E, 0x40, 0x41, 0x42, 0x42, 0x43, 0x44, 0x45, 0x46, 0x46,
	0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55,
	0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6A, 0x6C, 0x6D, 0x6E, 0x6F, 0x71, 0x72, 0x73, 0x75, 0x76, 0x77, 0x79, 0x7A,
	0x7C, 0x7D, 0x7E, 0x7F};

TEMPLATE static u32 bios_nop(ARM *cpu)
{
	return 3;
}

TEMPLATE static u32 WaitByLoop(ARM *cpu)
{
	u32 elapsed;

	if (PROCNUM == ARMCPU_ARM9)
	{
		if (((ARMv5*)cpu)->CP15Control & ((1 << 16) | (1 << 18))) // DTCM or ITCM is on (cache)
			elapsed = cpu->R[0] * 2;
		else
			elapsed = cpu->R[0] * 8;
	}
	else
		elapsed = cpu->R[0] * 4;
	cpu->R[0] = 0;
	return elapsed;
}

TEMPLATE static u32 wait4IRQ(ARM *cpu)
{
	cpu->Halted = CPU_FREEZE_IRQ_IE_IF;
	return 1;
}

TEMPLATE static u32 intrWaitARM(ARM *cpu)
{
	//TODO - account for differences between arm7 and arm9 (according to gbatek, the "bug doesn't work")

	const u32 intrFlagAdr = (PROCNUM == ARMCPU_ARM7)
								? 0x380FFF8
								: (((ARMv5*)cpu)->DTCMBase & 0xFFFFF000) + 0x3FF8;

	//set IME=1
	//without this, no irq handlers can happen (even though IF&IE waits can happily happen)
	//and so no bits in the OS irq flag variable can get set by the handlers
	cpu->DataWrite32(0x04000208, 1);

	//analyze the OS irq flag variable
	u32 intr;
	cpu->DataRead32(intrFlagAdr, &intr);
	u32 intrFlag = (cpu->R[1] & intr);

	//if the user requested us to discard flags, then clear the flag(s) we're going to be waiting on.
	//(be sure to only do this only on the first run through. use a little state machine to control that)
	if (cpu->intrWaitARM_state == 0 && cpu->R[0] == 1)
	{
		intr ^= intrFlag;
		cpu->DataWrite32(intrFlagAdr, intr);

		//we want to make sure we wait at least once below
		intrFlag = 0;
	}

	cpu->intrWaitARM_state = 1;

	//now, if the condition is satisfied (and it won't be the first time through, no matter what, due to cares taken above)
	if (intrFlag)
	{
		//write back the OS irq flags with the ones we were waiting for cleared
		intr ^= intrFlag;
		cpu->DataWrite32(intrFlagAdr, intr);

		cpu->intrWaitARM_state = 0;
		return 1;
	}

	//the condition wasn't satisfied. this means that we need to halt, wait for some enabled interrupt,
	//and then ensure that we return to this opcode again to check the condition again
	cpu->Halted = CPU_FREEZE_IRQ_IE_IF;

	//(rewire PC to jump back to this opcode)
	u32 instructAddr = cpu->NextInstr[0];
	cpu->R[15] = instructAddr;
	cpu->NextInstr[1] = instructAddr;
	return 1;
}

TEMPLATE static u32 waitVBlankARM(ARM *cpu)
{
	cpu->R[0] = 1;
	cpu->R[1] = 1;
	return intrWaitARM<PROCNUM>(cpu);
}

TEMPLATE static u32 sleep(ARM *cpu)
{
	//just set REG_HALTCNT to the fixed Sleep value
	cpu->DataWrite8(REG_HALTCNT, 0xC0);
	return 1;
}

//ARM7 only
TEMPLATE static u32 CustomHalt(ARM *cpu)
{
	//just set REG_HALTCNT to the provided value
	cpu->DataWrite8(REG_HALTCNT, cpu->R[2]);
	return 1;
}

TEMPLATE static u32 divide(ARM *cpu)
{
	s32 num = (s32)cpu->R[0];
	s32 dnum = (s32)cpu->R[1];

	if (dnum == 0)
		return 0;

	s32 res = num / dnum;
	cpu->R[0] = (u32)res;
	cpu->R[1] = (u32)(num % dnum);
	cpu->R[3] = (u32)abs(res);

	//INFO("ARM%c: SWI 0x09 (divide): in num %i, dnum %i, out R0:%i, R1:%i, R3:%i\n", PROCNUM?'7':'9', num, dnum, cpu->R[0], cpu->R[1], cpu->R[3]);

	return 6;
}

TEMPLATE static u32 copy(ARM *cpu)
{
	u32 src = cpu->R[0];
	u32 dst = cpu->R[1];
	u32 cnt = cpu->R[2];

	//INFO("swi copy from %08X to %08X, cnt=%08X\n", src, dst, cnt);

	switch (cnt & (1<<26))
	{
	case 0:
		src &= 0xFFFFFFFE;
		dst &= 0xFFFFFFFE;
		switch (cnt & (1<<24))
		{
		case 0:
			cnt &= 0x1FFFFF;
			while (cnt)
			{
				cpu->DataRead16(src, &tmp);
				cpu->DataWrite16(dst, tmp);
				cnt--;
				dst += 2;
				src += 2;
			}
			break;
		case 1:
		{
			cpu->DataRead16(src, &tmp);
			u16 val = (u16)tmp;
			cnt &= 0x1FFFFF;
			while (cnt)
			{
				cpu->DataWrite16(dst, val);
				cnt--;
				dst += 2;
			}
		}
		break;
		}
		break;
	case 1:
		src &= 0xFFFFFFFC;
		dst &= 0xFFFFFFFC;
		switch (cnt & (1<<24))
		{
		case 0:
			cnt &= 0x1FFFFF;
			while (cnt)
			{
				cpu->DataRead32(src, &tmp);
				cpu->DataWrite32(dst, tmp);
				cnt--;
				dst += 4;
				src += 4;
			}
			break;
		case 1:
		{
			u32 val;
			cpu->DataRead32(src, &val);
			cnt &= 0x1FFFFF;
			while (cnt)
			{
				cpu->DataWrite32(dst, val);
				cnt--;
				dst += 4;
			}
		}
		break;
		}
		break;
	}
	return 1;
}

TEMPLATE static u32 fastCopy(ARM *cpu)
{
	u32 src = cpu->R[0] & 0xFFFFFFFC;
	u32 dst = cpu->R[1] & 0xFFFFFFFC;
	u32 cnt = cpu->R[2];

	//INFO("swi fastcopy from %08X to %08X, cnt=%08X\n", src, dst, cnt);

	switch (cnt & (1 << 24))
	{
	case 0:
		cnt &= 0x1FFFFF;
		while (cnt)
		{
			cpu->DataRead32(src, &tmp);
			cpu->DataWrite32(dst, tmp);
			cnt--;
			dst += 4;
			src += 4;
		}
		break;
	case 1:
	{
		u32 val;
		cpu->DataRead32(src, &val);
		cnt &= 0x1FFFFF;
		while (cnt)
		{
			cpu->DataWrite32(dst, val);
			cnt--;
			dst += 4;
		}
	}
	break;
	}
	return 1;
}

TEMPLATE static u32 LZ77UnCompVram(ARM *cpu)
{
	int i1, i2;
	int byteCount;
	int byteShift;
	u32 writeValue;
	int len;
	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];
	u32 header;
	cpu->DataRead32(source, &header);
	source += 4;

	//INFO("swi lz77uncompvram\n");

	if (((source & 0xe000000) == 0) ||
		((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return 0;

	byteCount = 0;
	byteShift = 0;
	writeValue = 0;

	len = header >> 8;

	while (len > 0)
	{
		cpu->DataRead8(source++, &tmp);
		u8 d = tmp;

		if (d)
		{
			for (i1 = 0; i1 < 8; i1++)
			{
				if (d & 0x80)
				{
					int length;
					int offset;
					u32 windowOffset;
					cpu->DataRead8(source++, &tmp);
					u16 data = tmp << 8;
					cpu->DataRead8(source++, &tmp);
					data |= tmp;
					length = (data >> 12) + 3;
					offset = (data & 0x0FFF);
					windowOffset = dest + byteCount - offset - 1;
					for (i2 = 0; i2 < length; i2++)
					{
						cpu->DataRead8(windowOffset++, &tmp);
						writeValue |= (tmp << byteShift);
						byteShift += 8;
						byteCount++;

						if (byteCount == 2)
						{
							cpu->DataWrite16(dest, writeValue);
							dest += 2;
							byteCount = 0;
							byteShift = 0;
							writeValue = 0;
						}
						len--;
						if (len == 0)
							return 0;
					}
				}
				else
				{
					cpu->DataRead8(source++, &tmp);
					writeValue |= (tmp << byteShift);
					byteShift += 8;
					byteCount++;
					if (byteCount == 2)
					{
						cpu->DataWrite16(dest, writeValue);
						dest += 2;
						byteCount = 0;
						byteShift = 0;
						writeValue = 0;
					}
					len--;
					if (len == 0)
						return 0;
				}
				d <<= 1;
			}
		}
		else
		{
			for (i1 = 0; i1 < 8; i1++)
			{
				cpu->DataRead8(source++, &tmp);
				writeValue |= (tmp << byteShift);
				byteShift += 8;
				byteCount++;
				if (byteCount == 2)
				{
					cpu->DataWrite16(dest, writeValue);
					dest += 2;
					byteShift = 0;
					byteCount = 0;
					writeValue = 0;
				}
				len--;
				if (len == 0)
					return 0;
			}
		}
	}
	return 1;
}

TEMPLATE static u32 LZ77UnCompWram(ARM *cpu)
{
	int i1, i2;
	int len;
	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];

	u32 header;
	cpu->DataRead32(source, &header);
	source += 4;

	//INFO("swi lz77uncompwram\n");

	if (((source & 0xe000000) == 0) ||
		((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return 0;

	len = header >> 8;

	while (len > 0)
	{
		cpu->DataRead8(source++, &tmp);
		u8 d = tmp;

		if (d)
		{
			for (i1 = 0; i1 < 8; i1++)
			{
				if (d & 0x80)
				{
					int length;
					int offset;
					u32 windowOffset;
					cpu->DataRead8(source++, &tmp);
					u16 data = tmp << 8;
					cpu->DataRead8(source++, &tmp);
					data |= tmp;
					length = (data >> 12) + 3;
					offset = (data & 0x0FFF);
					windowOffset = dest - offset - 1;
					for (i2 = 0; i2 < length; i2++)
					{
						cpu->DataRead8(windowOffset++, &tmp);
						cpu->DataWrite8(dest++, tmp);
						len--;
						if (len == 0)
							return 0;
					}
				}
				else
				{
					cpu->DataRead8(source++, &tmp);
					cpu->DataWrite8(dest++, tmp);
					len--;
					if (len == 0)
						return 0;
				}
				d <<= 1;
			}
		}
		else
		{
			for (i1 = 0; i1 < 8; i1++)
			{
				cpu->DataRead8(source++, &tmp);
				cpu->DataWrite8(dest++, tmp);
				len--;
				if (len == 0)
					return 0;
			}
		}
	}
	return 1;
}

TEMPLATE static u32 RLUnCompVram(ARM *cpu)
{
	int i;
	int len;
	int byteCount;
	int byteShift;
	u32 writeValue;
	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];

	u32 header;
	cpu->DataRead32(source, &header);
	source += 4;

	//INFO("swi rluncompvram\n");

	if (((source & 0xe000000) == 0) ||
		((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return 0;

	len = header >> 8;
	byteCount = 0;
	byteShift = 0;
	writeValue = 0;

	while (len > 0)
	{
		cpu->DataRead8(source++, &tmp);
		u8 d = tmp;
		int l = d & 0x7F;
		if (d & 0x80)
		{
			cpu->DataRead8(source++, &tmp);
			u8 data = tmp;
			l += 3;
			for (i = 0; i < l; i++)
			{
				writeValue |= (data << byteShift);
				byteShift += 8;
				byteCount++;

				if (byteCount == 2)
				{
					cpu->DataWrite16(dest, writeValue);
					dest += 2;
					byteCount = 0;
					byteShift = 0;
					writeValue = 0;
				}
				len--;
				if (len == 0)
					return 0;
			}
		}
		else
		{
			l++;
			for (i = 0; i < l; i++)
			{
				cpu->DataRead8(source++, &tmp);
				writeValue |= (tmp << byteShift);
				byteShift += 8;
				byteCount++;
				if (byteCount == 2)
				{
					cpu->DataWrite16(dest, writeValue);
					dest += 2;
					byteCount = 0;
					byteShift = 0;
					writeValue = 0;
				}
				len--;
				if (len == 0)
					return 0;
			}
		}
	}
	return 1;
}

TEMPLATE static u32 RLUnCompWram(ARM *cpu)
{
	//this routine is used by yoshi touch&go from the very beginning

	//printf("RLUnCompWram\n");

	int i;
	int len;
	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];

	u32 header;
	cpu->DataRead32(source, &header);
	source += 4;

	//INFO("swi rluncompwram\n");

	if (((source & 0xe000000) == 0) ||
		((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return 0;

	len = header >> 8;

	while (len > 0)
	{
		cpu->DataRead8(source++, &tmp);
		u8 d = tmp;
		int l = d & 0x7F;
		if (d & 0x80)
		{
			cpu->DataRead8(source++, &tmp);
			u8 data = tmp;
			l += 3;
			for (i = 0; i < l; i++)
			{
				cpu->DataWrite8(dest++, data);
				len--;
				if (len == 0)
					return 0;
			}
		}
		else
		{
			l++;
			for (i = 0; i < l; i++)
			{
				cpu->DataRead8(source++, &tmp);
				cpu->DataWrite8(dest++, tmp);
				len--;
				if (len == 0)
					return 0;
			}
		}
	}
	return 1;
}

TEMPLATE static u32 UnCompHuffman(ARM *cpu)
{
	//this routine is used by the nintendo logo in the firmware boot screen

	u32 source, dest, writeValue, header, treeStart, mask;
	u32 data;
	u8 treeSize, currentNode, rootNode;
	int byteCount, byteShift, len, pos;
	int writeData;

	source = cpu->R[0];
	dest = cpu->R[1];

	cpu->DataRead32(source, &header);
	source += 4;

	//INFO("swi uncomphuffman\n");

	if (((source & 0xe000000) == 0) ||
		((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
		return 0;

	cpu->DataRead8(source++, &tmp);
	treeSize = tmp;

	treeStart = source;

	source += ((treeSize + 1) << 1) - 1; // minus because we already skipped one byte

	len = header >> 8;

	mask = 0x80000000;
	cpu->DataRead32(source, &data);
	source += 4;

	pos = 0;
	cpu->DataRead8(treeStart, &tmp);
	rootNode = tmp;
	currentNode = rootNode;
	writeData = 0;
	byteShift = 0;
	byteCount = 0;
	writeValue = 0;

	if ((header & 0x0F) == 8)
	{
		while (len > 0)
		{
			// take left
			if (pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3F) + 1) << 1);

			if (data & mask)
			{
				// right
				if (currentNode & 0x40)
					writeData = 1;
				cpu->DataRead8(treeStart + pos + 1, &tmp);
				currentNode = tmp;
			}
			else
			{
				// left
				if (currentNode & 0x80)
					writeData = 1;
				cpu->DataRead8(treeStart + pos, &tmp);
				currentNode = tmp;
			}

			if (writeData)
			{
				writeValue |= (currentNode << byteShift);
				byteCount++;
				byteShift += 8;

				pos = 0;
				currentNode = rootNode;
				writeData = 0;

				if (byteCount == 4)
				{
					byteCount = 0;
					byteShift = 0;
					cpu->DataWrite32(dest, writeValue);
					writeValue = 0;
					dest += 4;
					len -= 4;
				}
			}
			mask >>= 1;
			if (mask == 0)
			{
				mask = 0x80000000;
				cpu->DataRead32(source, &data);
				source += 4;
			}
		}
	}
	else
	{
		int halfLen = 0;
		int value = 0;
		while (len > 0)
		{
			// take left
			if (pos == 0)
				pos++;
			else
				pos += (((currentNode & 0x3F) + 1) << 1);

			if ((data & mask))
			{
				// right
				if (currentNode & 0x40)
					writeData = 1;
				cpu->DataRead8(treeStart + pos + 1, &tmp);
				currentNode = tmp;
			}
			else
			{
				// left
				if (currentNode & 0x80)
					writeData = 1;
				cpu->DataRead8(treeStart + pos, &tmp);
				currentNode = tmp;
			}

			if (writeData)
			{
				if (halfLen == 0)
					value |= currentNode;
				else
					value |= (currentNode << 4);

				halfLen += 4;
				if (halfLen == 8)
				{
					writeValue |= (value << byteShift);
					byteCount++;
					byteShift += 8;

					halfLen = 0;
					value = 0;

					if (byteCount == 4)
					{
						byteCount = 0;
						byteShift = 0;
						cpu->DataWrite32(dest, writeValue);
						dest += 4;
						writeValue = 0;
						len -= 4;
					}
				}
				pos = 0;
				currentNode = rootNode;
				writeData = 0;
			}
			mask >>= 1;
			if (mask == 0)
			{
				mask = 0x80000000;
				cpu->DataRead32(source, &data);
				source += 4;
			}
		}
	}
	return 1;
}
TEMPLATE static u32 BitUnPack(ARM *cpu)
{
	u32 source, dest, header, base, temp;
	s32 len, bits, revbits, dataSize, data, bitwritecount, mask, bitcount, addBase;
	u8 b;

	source = cpu->R[0];
	dest = cpu->R[1];
	header = cpu->R[2];

	cpu->DataRead16(header, (u32*)&len);
	cpu->DataRead8(header + 2, (u32*)&bits);
	switch (bits)
	{
	case 1:
	case 2:
	case 4:
	case 8:
		break;
	default:
		return (0); // error
	}
	cpu->DataRead8(header + 3, (u32*)&dataSize);
	switch (dataSize)
	{
	case 1:
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
		break;
	default:
		return (0); // error
	}

	revbits = 8 - bits;
	cpu->DataRead32(header + 4, &base);
	addBase = (base & 0x80000000) ? 1 : 0;
	base &= 0x7fffffff;

	//INFO("SWI10: bitunpack src 0x%08X dst 0x%08X hdr 0x%08X (src len %05i src bits %02i dst bits %02i)\n\n", source, dest, header, len, bits, dataSize);

	data = 0;
	bitwritecount = 0;
	while (1)
	{
		len -= 1;
		if (len < 0)
			break;
		mask = 0xff >> revbits;
		cpu->DataRead8(source, &tmp);
		b = tmp;
		source++;
		bitcount = 0;
		while (1)
		{
			if (bitcount >= 8)
				break;
			temp = b & mask;
			if (temp)
				temp += base;
			else if (addBase)
				temp += base;

			//you might think you should do this. but you would be wrong!
			//this is meant for palette adjusting things, i.e. 16col data to 256col data in colors 240..255. In that case theres no modulo normally.
			//Users expecting modulo have done something wrong anyway.
			//temp &= (1<<bits)-1;

			data |= temp << bitwritecount;
			bitwritecount += dataSize;
			if (bitwritecount >= 32)
			{
				cpu->DataWrite32(dest, data);
				dest += 4;
				data = 0;
				bitwritecount = 0;
			}
			bitcount += bits;
			b >>= bits;
		}
	}
	return 1;
}

TEMPLATE static u32 Diff8bitUnFilterWram(ARM *cpu) //this one might be different on arm7 and needs checking
{
	//INFO("swi Diff8bitUnFilterWram\n");

	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];

	cpu->DataRead32(source, &tmp);
	CompressionHeader header(tmp);
	source += 4;

	if (header.DataSize() != 1)
		printf("WARNING: incorrect header passed to Diff8bitUnFilterWram\n");
	if (header.Type() != 8)
		printf("WARNING: incorrect header passed to Diff8bitUnFilterWram\n");
	u32 len = header.DecompressedSize();

	cpu->DataRead8(source++, &tmp);
	u8 data = tmp;
	cpu->DataWrite8(dest++, data);
	len--;

	while (len > 0)
	{
		cpu->DataRead8(source++, &tmp);
		u8 diff = tmp;
		data += diff;
		cpu->DataWrite8(dest++, data);
		len--;
	}
	return 1;
}

TEMPLATE static u32 Diff16bitUnFilter(ARM *cpu)
{
	//INFO("swi Diff16bitUnFilter\n");

	u32 source = cpu->R[0];
	u32 dest = cpu->R[1];

	cpu->DataRead32(source, &tmp);
	CompressionHeader header(tmp);
	source += 4;

	if (header.DataSize() != 2)
		printf("WARNING: incorrect header passed to Diff16bitUnFilter\n");
	if (header.Type() != 8)
		printf("WARNING: incorrect header passed to Diff16bitUnFilter\n");
	u32 len = header.DecompressedSize();

	cpu->DataRead16(source, &tmp);
	u16 data = (u16)tmp;
	source += 2;
	cpu->DataWrite16(dest, data);
	dest += 2;
	len -= 2;

	while (len >= 2)
	{
		cpu->DataRead16(source, &tmp);
		u16 diff = (u16)tmp;
		source += 2;
		data += diff;
		cpu->DataWrite16(dest, data);
		dest += 2;
		len -= 2;
	}
	return 1;
}

TEMPLATE static u32 bios_sqrt(ARM *cpu)
{
	cpu->R[0] = (u32)sqrt((double)(cpu->R[0]));
	return 1;
}

//ARM9 only
TEMPLATE static u32 CustomPost(ARM *cpu)
{
	//just write provided value to REG_POSTFLG
	cpu->DataWrite8(REG_POSTFLG, cpu->R[0]);
	return 1;
}

//ARM7 only
TEMPLATE static u32 getSineTab(ARM *cpu)
{
	//ds returns garbage according to gbatek, but we must protect ourselves
	if (cpu->R[0] >= ARRAY_SIZE(getsinetbl))
	{
		printf("Invalid SWI getSineTab: %08X\n", cpu->R[0]);
		return 1;
	}

	cpu->R[0] = getsinetbl[cpu->R[0]];
	return 1;
}

//ARM7 only
TEMPLATE static u32 getPitchTab(ARM *cpu)
{
	//ds returns garbage according to gbatek, but we must protect ourselves
	if (cpu->R[0] >= ARRAY_SIZE(getpitchtbl))
	{
		printf("Invalid SWI getPitchTab: %08X\n", cpu->R[0]);
		return 1;
	}

	cpu->R[0] = getpitchtbl[cpu->R[0]];
	return 1;
}

//ARM7 only
TEMPLATE static u32 getVolumeTab(ARM *cpu)
{
	//ds returns garbage according to gbatek, but we must protect ourselves
	if (cpu->R[0] >= ARRAY_SIZE(getvoltbl))
	{
		printf("Invalid SWI getVolumeTab: %08X\n", cpu->R[0]);
		return 1;
	}
	cpu->R[0] = getvoltbl[cpu->R[0]];
	return 1;
}

TEMPLATE static u32 getCRC16(ARM *cpu)
{
	//dawn of sorrow uses this to checksum its save data;
	//if this implementation is wrong, then it won't match what the real bios returns,
	//and savefiles created with a bios will be invalid when loaded with non-bios (and vice-versa).
	//Once upon a time, desmume was doing this wrongly; this was due to its mis-use of high bits of the input CRC.
	//Additionally, that implementation was not handling odd sizes and addresses correctly (but this was discovered independently)
	//We have confirmed that the crc16 works the same on the arm9 and arm7.
	//The following call is left here so we can A/B test with the old version. Glad I left it, because we keep coming back to this code.
	//[removed in MelonDS; see DeSmuME if you need this]

	u16 crc = (u16)cpu->R[0];
	u32 datap = cpu->R[1];
	u32 size = cpu->R[2] >> 1;
	u16 currVal = 0;

	const u16 val[] = {0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401, 0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400};

	for (u32 i = 0; i < size; i++)
	{
		cpu->DataRead16(datap + i * 2, &tmp);
		currVal = (u16)tmp;

		for (int j = 0; j < 4; j++)
		{
			u16 tabVal = val[crc & 0xF];
			crc >>= 4;
			crc ^= tabVal;

			u16 tempVal = currVal >> (4 * j);
			tabVal = val[tempVal & 0xF];
			crc ^= tabVal;
		}
	}

	cpu->R[0] = crc;

	//R3 contains the last processed halfword
	//this is significant -- why? Can we get a test case? Supposedly there is one..
	cpu->R[3] = currVal;

	return 1;
}

TEMPLATE static u32 isDebugger(ARM *cpu)
{
	// //gbatek has additional specifications which are not emulated here
	// if(nds.Is_DebugConsole())
	// 	cpu->R[0] = 1;
	// else
		cpu->R[0] = 0;
	return 1;
}

//ARM7 only
TEMPLATE static u32 SoundBias(ARM *cpu)
{
	u32 curBias;
	cpu->DataRead32(0x04000504, &curBias);
	u32 newBias = (curBias == 0) ? 0x000 : 0x200;
	u32 delay = (newBias > curBias) ? (newBias - curBias) : (curBias - newBias);

	cpu->DataWrite32(0x04000504, newBias);
	return cpu->R[1] * delay;
}

//ARM7 only
TEMPLATE static u32 getBootProcs(ARM *cpu)
{
	cpu->R[0] = 0x00000A2E;
	cpu->R[1] = 0x00002C3C;
	cpu->R[3] = 0x000005FF;
	return 1;
}

TEMPLATE static u32 SoftReset(ARM *cpu)
{
	//not emulated yet
	return 1;
}

u32 (*ARM_swi_tab[2][32])(ARM *cpu) = {
	{
		SoftReset<ARMCPU_ARM9>,			   // 0x00
		bios_nop<ARMCPU_ARM9>,			   // 0x01
		bios_nop<ARMCPU_ARM9>,			   // 0x02
		WaitByLoop<ARMCPU_ARM9>,		   // 0x03
		intrWaitARM<ARMCPU_ARM9>,		   // 0x04
		waitVBlankARM<ARMCPU_ARM9>,		   // 0x05
		wait4IRQ<ARMCPU_ARM9>,			   // 0x06
		bios_nop<ARMCPU_ARM9>,			   // 0x07
		bios_nop<ARMCPU_ARM9>,			   // 0x08
		divide<ARMCPU_ARM9>,			   // 0x09
		bios_nop<ARMCPU_ARM9>,			   // 0x0A
		copy<ARMCPU_ARM9>,				   // 0x0B
		fastCopy<ARMCPU_ARM9>,			   // 0x0C
		bios_sqrt<ARMCPU_ARM9>,			   // 0x0D
		getCRC16<ARMCPU_ARM9>,			   // 0x0E
		isDebugger<ARMCPU_ARM9>,		   // 0x0F
		BitUnPack<ARMCPU_ARM9>,			   // 0x10
		LZ77UnCompWram<ARMCPU_ARM9>,	   // 0x11
		LZ77UnCompVram<ARMCPU_ARM9>,	   // 0x12
		UnCompHuffman<ARMCPU_ARM9>,		   // 0x13
		RLUnCompWram<ARMCPU_ARM9>,		   // 0x14
		RLUnCompVram<ARMCPU_ARM9>,		   // 0x15
		Diff8bitUnFilterWram<ARMCPU_ARM9>, // 0x16
		bios_nop<ARMCPU_ARM9>,			   // 0x17
		Diff16bitUnFilter<ARMCPU_ARM9>,	   // 0x18
		bios_nop<ARMCPU_ARM9>,			   // 0x19
		bios_nop<ARMCPU_ARM9>,			   // 0x1A
		bios_nop<ARMCPU_ARM9>,			   // 0x1B
		bios_nop<ARMCPU_ARM9>,			   // 0x1C
		bios_nop<ARMCPU_ARM9>,			   // 0x1D
		bios_nop<ARMCPU_ARM9>,			   // 0x1E
		CustomPost<ARMCPU_ARM9>,		   // 0x1F
	},
	{
		SoftReset<ARMCPU_ARM7>,		 // 0x00
		bios_nop<ARMCPU_ARM7>,		 // 0x01
		bios_nop<ARMCPU_ARM7>,		 // 0x02
		WaitByLoop<ARMCPU_ARM7>,	 // 0x03
		intrWaitARM<ARMCPU_ARM7>,	 // 0x04
		waitVBlankARM<ARMCPU_ARM7>,  // 0x05
		wait4IRQ<ARMCPU_ARM7>,		 // 0x06
		sleep<ARMCPU_ARM7>,			 // 0x07
		SoundBias<ARMCPU_ARM7>,		 // 0x08
		divide<ARMCPU_ARM7>,		 // 0x09
		bios_nop<ARMCPU_ARM7>,		 // 0x0A
		copy<ARMCPU_ARM7>,			 // 0x0B
		fastCopy<ARMCPU_ARM7>,		 // 0x0C
		bios_sqrt<ARMCPU_ARM7>,		 // 0x0D
		getCRC16<ARMCPU_ARM7>,		 // 0x0E
		isDebugger<ARMCPU_ARM7>,	 // 0x0F
		BitUnPack<ARMCPU_ARM7>,		 // 0x10
		LZ77UnCompWram<ARMCPU_ARM7>, // 0x11
		LZ77UnCompVram<ARMCPU_ARM7>, // 0x12
		UnCompHuffman<ARMCPU_ARM7>,  // 0x13
		RLUnCompWram<ARMCPU_ARM7>,   // 0x14
		RLUnCompVram<ARMCPU_ARM7>,   // 0x15
		bios_nop<ARMCPU_ARM7>,		 // 0x16
		bios_nop<ARMCPU_ARM7>,		 // 0x17
		bios_nop<ARMCPU_ARM7>,		 // 0x18
		bios_nop<ARMCPU_ARM7>,		 // 0x19
		getSineTab<ARMCPU_ARM7>,	 // 0x1A
		getPitchTab<ARMCPU_ARM7>,	// 0x1B
		getVolumeTab<ARMCPU_ARM7>,   // 0x1C
		getBootProcs<ARMCPU_ARM7>,   // 0x1D
		bios_nop<ARMCPU_ARM7>,		 // 0x1E
		CustomHalt<ARMCPU_ARM7>,	 // 0x1F
	}};

#define BIOS_NOP "bios_nop"
const char *ARM_swi_names[2][32] = {
	{
		"SoftReset",			// 0x00
		BIOS_NOP,				// 0x01
		BIOS_NOP,				// 0x02
		"WaitByLoop",			// 0x03
		"IntrWait",				// 0x04
		"VBlankIntrWait",		// 0x05
		"Halt",					// 0x06
		BIOS_NOP,				// 0x07
		BIOS_NOP,				// 0x08
		"Div",					// 0x09
		BIOS_NOP,				// 0x0A
		"CpuSet",				// 0x0B
		"CpuFastSet",			// 0x0C
		"Sqrt",					// 0x0D
		"GetCRC16",				// 0x0E
		"IsDebugger",			// 0x0F
		"BitUnPack",			// 0x10
		"LZ77UnCompWram",		// 0x11
		"LZ77UnCompVram",		// 0x12
		"HuffUnComp",			// 0x13
		"RLUnCompWram",			// 0x14
		"RLUnCompVram",			// 0x15
		"Diff8bitUnFilterWram", // 0x16
		BIOS_NOP,				// 0x17
		"Diff16bitUnFilter",	// 0x18
		BIOS_NOP,				// 0x19
		BIOS_NOP,				// 0x1A
		BIOS_NOP,				// 0x1B
		BIOS_NOP,				// 0x1C
		BIOS_NOP,				// 0x1D
		BIOS_NOP,				// 0x1E
		"CustomPost",			// 0x1F
	},
	{
		"SoftReset",	  // 0x00
		BIOS_NOP,		  // 0x01
		BIOS_NOP,		  // 0x02
		"WaitByLoop",	 // 0x03
		"IntrWait",		  // 0x04
		"VBlankIntrWait", // 0x05
		"Halt",			  // 0x06
		"Sleep",		  // 0x07
		"SoundBias",	  // 0x08
		"Div",			  // 0x09
		BIOS_NOP,		  // 0x0A
		"CpuSet",		  // 0x0B
		"CpuFastSet",	 // 0x0C
		"Sqrt",			  // 0x0D
		"GetCRC16",		  // 0x0E
		"IsDebugger",	 // 0x0F
		"BitUnPack",	  // 0x10
		"LZ77UnCompWram", // 0x11
		"LZ77UnCompVram", // 0x12
		"HuffUnComp",	 // 0x13
		"RLUnCompWram",   // 0x14
		"RLUnCompVram",   // 0x15
		BIOS_NOP,		  // 0x16
		BIOS_NOP,		  // 0x17
		BIOS_NOP,		  // 0x18
		BIOS_NOP,		  // 0x19
		"GetSineTable",   // 0x1A
		"GetPitchTable",  // 0x1B
		"GetVolumeTable", // 0x1C
		"GetBootProcs",   // 0x1D
		BIOS_NOP,		  // 0x1E
		"CustomHalt",	 // 0x1F
	}};
#undef BIOS_NOP
