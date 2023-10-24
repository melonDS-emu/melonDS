
#ifndef GDBSTUB_H_
#error "DO NOT INCLUDE THIS FILE YOURSELF!"
#endif

private:
	static ExecResult Handle_g(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_G(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_m(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_M(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_X(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_c(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_s(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_p(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_P(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_H(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_Question(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_Exclamation(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_D(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_r(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_R(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_k(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_z(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_Z(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_q(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_Q(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_q_HostInfo(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_Rcmd(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_Supported(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_CRC(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_C(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_fThreadInfo(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_sThreadInfo(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_TStatus(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_features(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_q_Attached(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_v_Attach(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_Kill(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_Run(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_Stopped(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_MustReplyEmpty(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_Cont(GdbStub* stub, const u8* cmd, ssize_t len);
	static ExecResult Handle_v_ContQuery(GdbStub* stub, const u8* cmd, ssize_t len);

	static ExecResult Handle_Q_StartNoAckMode(GdbStub* stub, const u8* cmd, ssize_t len);

