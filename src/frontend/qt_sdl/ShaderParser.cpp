#include "ShaderParser.h"
#include "FrontendShader.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <iostream>

ShaderParser::ShaderParser(const std::string& shaderDir) : m_shaderDir(shaderDir) {}

std::vector<FrontendShader> ShaderParser::GetPresets() {
    std::vector<FrontendShader> presets;

    // Curated list of shader presets to expose in the filter UI
    const std::vector<std::string> curatedPaths = {
        "edge-smoothing/xbrz/xbrz-freescale.slangp",
        "edge-smoothing/xbrz/6xbrz-linear.slangp",
        "edge-smoothing/xbrz/5xbrz-linear.slangp",
        "edge-smoothing/xbrz/4xbrz-linear.slangp",
        "edge-smoothing/scalenx/scale2x.slangp",
        "edge-smoothing/scalenx/epx.slangp",
        "edge-smoothing/hqx/hq2x.slangp",
        "edge-smoothing/hqx/hq3x.slangp",
        "edge-smoothing/hqx/hq4x.slangp",
        "edge-smoothing/eagle/super-2xsai.slangp",
        "edge-smoothing/eagle/supereagle.slangp",
        "edge-smoothing/xbr/super-xbr.slangp",
        "edge-smoothing/sabr/sabr.slangp",
        "edge-smoothing/omniscale/omniscale.slangp",
        "handheld/lcd-grid-v2.slangp",
        "handheld/color-mod/nds-color.slangp",
        "handheld/color-mod/dslite-color.slangp",
        "handheld/zfast-lcd.slangp",
        "handheld/gameboy.slangp",
        "dithering/mdapt.slangp",
        "ntsc/ntsc-256px-composite.slangp",
        "anti-aliasing/fxaa.slangp",
        "anti-aliasing/smaa.slangp",
        "sharpen/adaptive-sharpen.slangp",
        "sharpen/luma-sharpen.slangp",
        "sharpen/Anime4k.slangp",
        "interpolation/bicubic.slangp",
        "interpolation/lanczos2.slangp",
        "crt/crt-easymode.slangp",
        "crt/crt-geom.slangp",
        "crt/crt-lottes.slangp",
        "crt/crt-hyllian.slangp",
        "crt/crt-aperture.slangp",
        "crt/crt-caligari.slangp",
        "crt/crt-mattias.slangp"
    };

    for (const auto& relPath : curatedPaths) {
        std::string fullPath = QDir(QString::fromStdString(m_shaderDir)).filePath(QString::fromStdString(relPath)).toStdString();
        QFileInfo fileInfo(QString::fromStdString(fullPath));

        if (!fileInfo.exists()) continue;

        FrontendShader feShader;
        if (feShader.preset.load(fullPath)) {
            feShader.path = fullPath;
            
            if (relPath.find("xbrz-freescale") != std::string::npos) {
                feShader.name = "xBRZ Freescale (Auto)";
                feShader.author = "Zenju / libretro";
                feShader.description = "xBRZ upscales images by detecting edges and drawing smooth lines. Good for 2D pixel art.";
            } else if (relPath.find("6xbrz") != std::string::npos) {
                feShader.name = "xBRZ 6x";
                feShader.author = "Zenju / libretro";
                feShader.description = "Fixed 6x pass xBRZ upscaler for pixel art edge smoothing.";
            } else if (relPath.find("5xbrz") != std::string::npos) {
                feShader.name = "xBRZ 5x";
                feShader.author = "Zenju / libretro";
                feShader.description = "Fixed 5x pass xBRZ upscaler for pixel art edge smoothing.";
            } else if (relPath.find("4xbrz") != std::string::npos) {
                feShader.name = "xBRZ 4x";
                feShader.author = "Zenju / libretro";
                feShader.description = "Fixed 4x pass xBRZ upscaler for pixel art edge smoothing.";
            } else if (relPath.find("scale2x") != std::string::npos) {
                feShader.name = "Scale2x";
                feShader.author = "Andrea Mazzoleni";
                feShader.description = "A fast pixel art scaler that smooths jagged edges without blurring.";
            } else if (relPath.find("epx") != std::string::npos) {
                feShader.name = "EPX";
                feShader.author = "Eric Johnston";
                feShader.description = "Eric's Pixel Expansion. A fast 2x upscaler developed originally for LucasArts.";
            } else if (relPath.find("hq2x") != std::string::npos) {
                feShader.name = "HQ2x";
                feShader.author = "Maxim Stepin";
                feShader.description = "High Quality 2x upscaler that interpolates edges to preserve pixel art details smoothly.";
            } else if (relPath.find("hq3x") != std::string::npos) {
                feShader.name = "HQ3x";
                feShader.author = "Maxim Stepin";
                feShader.description = "High Quality 3x upscaler that interpolates edges to preserve pixel art details smoothly.";
            } else if (relPath.find("hq4x") != std::string::npos) {
                feShader.name = "HQ4x";
                feShader.author = "Maxim Stepin";
                feShader.description = "High Quality 4x upscaler that interpolates edges to preserve pixel art details smoothly.";
            } else if (relPath.find("super-2xsai") != std::string::npos) {
                feShader.name = "Super 2xSaI";
                feShader.author = "Derek Liauw Kie Fa";
                feShader.description = "Super 2x Scale and Interpolate. A classic emulator pixel art upscaler.";
            } else if (relPath.find("supereagle") != std::string::npos) {
                feShader.name = "Super Eagle";
                feShader.author = "Derek Liauw Kie Fa";
                feShader.description = "An early scale engine that creates a slightly blurrier, smoothly blended upscale.";
            } else if (relPath.find("super-xbr") != std::string::npos) {
                feShader.name = "Super-xBR";
                feShader.author = "Hyllian";
                feShader.description = "Advanced edge-directed interpolation filter to smoothly upscale images.";
            } else if (relPath.find("sabr") != std::string::npos) {
                feShader.name = "SABR";
                feShader.author = "Joshua Street";
                feShader.description = "Shape Anti-Aliasing by Bicubic Interpolation. Great for smoothing out pixel graphics.";
            } else if (relPath.find("omniscale") != std::string::npos) {
                feShader.name = "OmniScale";
                feShader.author = "Lior Halphon";
                feShader.description = "A scale-independent 2D upscale filter based on edge-directed interpolation.";
            } else if (relPath.find("nds-color") != std::string::npos) {
                feShader.name = "NDS Color";
                feShader.author = "pokefan531";
                feShader.description = "Adjusts gamma and color profile to simulate the washed-out look of the original Nintendo DS hardware.";
            } else if (relPath.find("dslite-color") != std::string::npos) {
                feShader.name = "DS-Lite Color";
                feShader.author = "pokefan531";
                feShader.description = "Simulates the specific screen color temperature and gamma of the Nintendo DS Lite hardware.";
            } else if (relPath.find("lcd-grid-v2") != std::string::npos) {
                feShader.name = "LCD Grid v2";
                feShader.author = "cgwg";
                feShader.description = "Simulates an LCD pixel grid typical of classic handheld consoles.";
            } else if (relPath.find("zfast-lcd") != std::string::npos) {
                feShader.name = "zFast LCD";
                feShader.author = "SoltanGris42";
                feShader.description = "A fast shader that accurately emulates the subtle ghosting and LCD grid of handheld screens.";
            } else if (relPath.find("gameboy") != std::string::npos) {
                feShader.name = "GameBoy DMG";
                feShader.author = "cgwg / libretro";
                feShader.description = "Emulates the iconic dot matrix grid and greenish tint of the original GameBoy hardware.";
            } else if (relPath.find("mdapt") != std::string::npos) {
                feShader.name = "MDAPT Dithering";
                feShader.author = "Sp00kyFox";
                feShader.description = "Merge Dithering and Pseudo-Transparency. Smartly blends checkerboard dithering patterns.";
            } else if (relPath.find("ntsc-256px-composite") != std::string::npos) {
                feShader.name = "NTSC Composite";
                feShader.author = "Themaister";
                feShader.description = "Simulates the artifacts, color bleeding, and fringing of a classic composite video signal.";
            } else if (relPath.find("fxaa") != std::string::npos) {
                feShader.name = "FXAA";
                feShader.author = "Timothy Lottes";
                feShader.description = "Fast Approximate Anti-Aliasing. A very fast screen-space anti-aliasing technique for 3D edges.";
            } else if (relPath.find("smaa") != std::string::npos) {
                feShader.name = "SMAA";
                feShader.author = "Jorge Jimenez";
                feShader.description = "Subpixel Morphological Anti-Aliasing. Advanced technique for smoothing 3D polygon edges.";
            } else if (relPath.find("adaptive-sharpen") != std::string::npos) {
                feShader.name = "Adaptive Sharpen";
                feShader.author = "bakamac";
                feShader.description = "Dynamically sharpens images while preserving delicate details.";
            } else if (relPath.find("luma-sharpen") != std::string::npos) {
                feShader.name = "Luma Sharpen";
                feShader.author = "CeeJay.dk";
                feShader.description = "Sharpens the image using the luma channel to prevent color artifacts.";
            } else if (relPath.find("Anime4k") != std::string::npos) {
                feShader.name = "Anime4K";
                feShader.author = "bloc97";
                feShader.description = "State-of-the-art open-source high-quality real-time anime upscaling algorithm.";
            } else if (relPath.find("bicubic") != std::string::npos) {
                feShader.name = "Bicubic";
                feShader.author = "libretro";
                feShader.description = "Standard mathematical interpolation filter for smooth upscaling.";
            } else if (relPath.find("lanczos2") != std::string::npos) {
                feShader.name = "Lanczos2";
                feShader.author = "libretro";
                feShader.description = "High-quality resampling filter using a Lanczos window.";
            } else if (relPath.find("crt-easymode") != std::string::npos) {
                feShader.name = "CRT EasyMode";
                feShader.author = "Easymode";
                feShader.description = "A simple and clean CRT shader that provides a nice classic TV scanline look.";
            } else if (relPath.find("crt-geom") != std::string::npos) {
                feShader.name = "CRT Geom";
                feShader.author = "cgwg";
                feShader.description = "Simulates the screen curvature, glow, and scanlines of a classic CRT monitor.";
            } else if (relPath.find("crt-lottes") != std::string::npos) {
                feShader.name = "CRT Lottes";
                feShader.author = "Timothy Lottes";
                feShader.description = "Emulates the distinct shadow mask and scanline look of an old-school CRT TV.";
            } else if (relPath.find("crt-hyllian") != std::string::npos) {
                feShader.name = "CRT Hyllian";
                feShader.author = "Hyllian";
                feShader.description = "A high-quality CRT shader designed to preserve sharpness while adding scanlines and phosphor masks.";
            } else if (relPath.find("crt-aperture") != std::string::npos) {
                feShader.name = "CRT Aperture";
                feShader.author = "Wozniak";
                feShader.description = "A moderately heavy shader simulating Sony Trinitron aperture grille CRT TVs.";
            } else if (relPath.find("crt-caligari") != std::string::npos) {
                feShader.name = "CRT Caligari";
                feShader.author = "Caligari";
                feShader.description = "A sharp CRT shader that uses simple mathematics to simulate scanlines and shadow mask.";
            } else if (relPath.find("crt-mattias") != std::string::npos) {
                feShader.name = "CRT Mattias";
                feShader.author = "Mattias Gustavsson";
                feShader.description = "Simulates an old, distorted, slightly curved CRT screen with heavy color bleeding.";
            } else {
                // Fallback for any preset not explicitly named above
                feShader.name = fileInfo.baseName().toStdString();
                feShader.author = "Unknown";
                feShader.description = "No description available.";
            }
            
            presets.push_back(feShader);
        }
    }

    std::sort(presets.begin(), presets.end(), [](const FrontendShader& a, const FrontendShader& b) {
        return a.name < b.name;
    });

    return presets;
}
