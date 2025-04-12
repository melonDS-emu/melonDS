#ifndef DEF_H
#define DEF_H


// Declare global variable with extern (to make it accessible from other files)
extern uint32_t globalChecksum;

// Declare global variables with extern (to make them accessible from other files)
extern bool isRomDetected;

// Use extern to declare these variables for use in other files

namespace RomVersions {
    constexpr uint32_t US1_0 = 0x218DA42C;
    constexpr uint32_t US1_1 = 0x91B46577;
    constexpr uint32_t EU1_0 = 0xA4A8FE5A;
    constexpr uint32_t EU1_1 = 0x910018A5;
    constexpr uint32_t JP1_0 = 0xD75F539D;
    constexpr uint32_t JP1_1 = 0x42EBF348;
    constexpr uint32_t KR1_0 = 0xE54682F3;
}

#endif // DEF_H
