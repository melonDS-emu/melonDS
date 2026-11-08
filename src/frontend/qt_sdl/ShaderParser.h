#pragma once

#include "FrontendShader.h"
#include <string>
#include <vector>

class ShaderParser {
public:
    explicit ShaderParser(const std::string& shaderDir);
    std::vector<FrontendShader> GetPresets();

private:
    std::string m_shaderDir;
};
