#include "InputAndroid.h"
#include "../NDS.h"

namespace MelonDSAndroid
{
    void touchScreen(u16 x, u16 y)
    {
        NDS::TouchScreen(x, y);
    }

    void releaseScreen()
    {
        NDS::ReleaseScreen();
    }

    void pressKey(u32 key)
    {
        NDS::PressKey(key);
    }

    void releaseKey(u32 key)
    {
        NDS::ReleaseKey(key);
    }
}

