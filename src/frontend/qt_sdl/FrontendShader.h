#pragma once
#include "SlangPreset.h"
#include <string>

struct FrontendShader {
    std::string name;
    std::string author;
    std::string description;
    std::string path; // Full filesystem path to the .slangp file
    SlangPreset preset;
};