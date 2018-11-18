#ifndef MELONDS__INPUTANDROID_H
#define MELONDS__INPUTANDROID_H

#include "../types.h"

namespace MelonDSAndroid
{
    void touchScreen(u16 x, u16 y);
    void releaseScreen();
    void pressKey(u32 key);
    void releaseKey(u32 key);
}

#endif //MELONDS__INPUTANDROID_H
