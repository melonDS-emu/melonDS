/* Original still-image controls; no camera hardware, BIOS or Nintendo data. */
#include "CameraManager.h"
#include "Config.h"
#include <QCoreApplication>
#include <QImage>
#include <QTemporaryDir>
#include <array>
#include <cstdio>

extern "C" int __wrap_main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir files;
    if (!files.isValid()) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int passed = 0, count = 0;
    for (auto format : {QImage::Format_RGB32, QImage::Format_RGBA64})
    {
        QImage image(4, 2, format);
        for (int y = 0; y < 2; y++)
            for (int x = 0; x < 4; x++)
                image.setPixelColor(x, y, QColor(20 + 40*x, 15 + 90*y, 220 - 35*x));
        const QString path = files.path() + "/fixture.png";
        if (!image.save(path)) return 2;
        QImage loaded(path);
        QImage converted = loaded.convertToFormat(QImage::Format_RGB32);
        for (bool yuv : {false, true})
        {
            for (int index = 0; index < 2; index++)
            {
                auto config = Config::GetGlobalTable().GetTable(index ? "DSi.Camera1" : "DSi.Camera0");
                config.SetInt("InputType", index ? 0 : 1);
                config.SetQString("ImagePath", path);
            }
            CameraManager actual(0, 4, 2, yuv), reference(1, 4, 2, false);
            reference.feedFrame(reinterpret_cast<melonDS::u32*>(converted.bits()), 4, 2, false);
            std::array<melonDS::u32, 8> received{}, expected{};
            actual.captureFrame(received.data(), 4, 2, yuv);
            reference.captureFrame(expected.data(), 4, 2, yuv);
            bool ok = received == expected;
            passed += ok;
            count++;
            std::printf("%s: depth %d PNG -> %s camera frame\n", ok ? "PASS" : "FAIL", loaded.depth(), yuv ? "YUV" : "RGB32");
        }
    }
    std::printf("%d/%d camera image controls passed\n", passed, count);
    return passed == count ? 0 : 1;
}
