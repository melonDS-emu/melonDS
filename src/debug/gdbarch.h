
#ifndef GDBARCH_H_
#define GDBARCH_H_

#define GDB_ARCH_N_REG 39

enum gdb_register {
	gdb_reg_r0,
	gdb_reg_r1,
	gdb_reg_r2,
	gdb_reg_r3,
	gdb_reg_r4,
	gdb_reg_r5,
	gdb_reg_r6,
	gdb_reg_r7,
	gdb_reg_r8,
	gdb_reg_r9,
	gdb_reg_r10,
	gdb_reg_r11,
	gdb_reg_r12,
	gdb_reg_sp,
	gdb_reg_lr,
	gdb_reg_pc,

	gdb_reg_cpsr,
	gdb_reg_sp_usr,
	gdb_reg_lr_usr,

	gdb_reg_r8_fiq,
	gdb_reg_r9_fiq,
	gdb_reg_r10_fiq,
	gdb_reg_r11_fiq,
	gdb_reg_r12_fiq,

	gdb_reg_sp_fiq,
	gdb_reg_lr_fiq,
	gdb_reg_sp_irq,
	gdb_reg_lr_irq,
	gdb_reg_sp_svc,
	gdb_reg_lr_svc,
	gdb_reg_sp_abt,
	gdb_reg_lr_abt,
	gdb_reg_sp_und,
	gdb_reg_lr_und,

	gdb_reg_spsr_fiq,
	gdb_reg_spsr_irq,
	gdb_reg_spsr_svc,
	gdb_reg_spsr_abt,
	gdb_reg_spsr_und,
};

#endif

