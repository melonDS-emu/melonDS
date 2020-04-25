#include "../ARM.h"

int main(int argc, char* argv[])
{
    FILE* f = fopen("ARMJIT_Offsets.h", "w");
#define writeOffset(field) \
        fprintf(f, "#define ARM_" #field "_offset 0x%x\n", offsetof(ARM, field))

    writeOffset(CPSR);
    writeOffset(Cycles);
    writeOffset(StopExecution);

    fclose(f);
    return 0;
}