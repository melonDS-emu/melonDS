/* Original RGB/YUV input controls; no BIOS, camera device or Nintendo data. */
#include "CameraManager.h"
#include "Config.h"
#include <QCoreApplication>
#include <array>
#include <stdio.h>

extern "C" int __wrap_main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    setvbuf(stdout, nullptr, _IONBF, 0);
    for (int index = 0; index < 2; index++)
    {
        auto config = Config::GetGlobalTable().GetTable(index ? "DSi.Camera1" : "DSi.Camera0");
        config.SetInt("InputType", 0);
        config.SetBool("XFlip", false);
    }
    std::array<melonDS::u32, 8> rgb = {0xFF806040, 0xFFC08060, 0xFF20A040, 0xFF40C060,
                                      0xFF406080, 0xFF6080C0, 0xFF804060, 0xFFC06080};
    int passed = 0;
    for (bool destinationYUV : {true, false})
    {
        CameraManager reference(1, 4, 2, !destinationYUV);
        std::array<melonDS::u32, 8> input{}, expected{}, received{};
        if (destinationYUV)
            input = rgb;
        else
        {
            CameraManager encoder(0, 4, 2, false);
            encoder.feedFrame(rgb.data(), 4, 2, false);
            encoder.captureFrame(input.data(), 4, 2, true);
        }
        reference.feedFrame(input.data(), 4, 2, !destinationYUV);
        reference.captureFrame(expected.data(), 4, 2, destinationYUV);
        CameraManager actual(0, 4, 2, destinationYUV);
        actual.feedFrame(input.data(), 4, 2, !destinationYUV);
        actual.captureFrame(received.data(), 4, 2, destinationYUV);
        bool ok = received == expected;
        passed += ok;
        printf("%s: %s camera input conversion\n", ok ? "PASS" : "FAIL", destinationYUV ? "RGB -> YUV" : "YUV -> RGB");
    }
    printf("%d/2 frame conversion controls passed\n", passed);
    return passed == 2 ? 0 : 1;
}
