#include "teakra/disassembler.h"
#include "teakra/disassembler_c.h"

extern "C" {
	bool Teakra_Disasm_NeedExpansion(uint16_t opcode) {
		return Teakra::Disassembler::NeedExpansion(opcode);
	}

	size_t Teakra_Disasm_Do(char* dst, size_t dstlen,
			uint16_t opcode, uint16_t expansion /*= 0*/) {
		std::string r = Teakra::Disassembler::Do(opcode, expansion);

		if (dst) {
			size_t i = 0;
			for (; i < (dstlen-1) && i < r.length(); ++i) {
				dst[i] = r[i];
			}
			dst[dstlen-1] = '\0';
		}

		return r.length();
	}
}
