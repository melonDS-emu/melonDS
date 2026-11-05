#include "../FullscreenState.h"

#include <iostream>

int main()
{
    if (getRestoreState(false) != WindowRestoreState::Normal)
    {
        std::cerr << "FAIL: Normal state was not preserved\n";
        return 1;
    }

    if (getRestoreState(true) != WindowRestoreState::Maximized)
    {
        std::cerr << "FAIL: Maximized state was not preserved\n";
        return 1;
    }

    std::cout << "All fullscreen state tests passed\n";
    return 0;
}