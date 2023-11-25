
#ifndef GDBARCH_H_
#define GDBARCH_H_

namespace Gdb
{

using namespace melonDS;
enum class Register : int
{
	r0,
	r1,
	r2,
	r3,
	r4,
	r5,
	r6,
	r7,
	r8,
	r9,
	r10,
	r11,
	r12,
	sp,
	lr,
	pc,

	cpsr,
	sp_usr,
	lr_usr,

	r8_fiq,
	r9_fiq,
	r10_fiq,
	r11_fiq,
	r12_fiq,

	sp_fiq,
	lr_fiq,
	sp_irq,
	lr_irq,
	sp_svc,
	lr_svc,
	sp_abt,
	lr_abt,
	sp_und,
	lr_und,

	spsr_fiq,
	spsr_irq,
	spsr_svc,
	spsr_abt,
	spsr_und,

	COUNT
};

constexpr int GDB_ARCH_N_REG = (int)Register::COUNT;

}

#endif

