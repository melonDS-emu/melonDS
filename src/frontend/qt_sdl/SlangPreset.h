/*
    Copyright 2016-2026 melonDS team

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef SLANGPRESET_H
#define SLANGPRESET_H

#include <string>
#include <vector>
#include <map>

struct SlangParameter {
    std::string name;
    std::string description;
    float defaultValue;
    float minValue;
    float maxValue;
    float step;
};

struct SlangShaderPass {
    std::string shaderPath;
    std::string alias;
    std::string scaleTypeX = "source";
    std::string scaleTypeY = "source";
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    bool filterLinear = false;
    bool srgbFramebuffer = false;
    bool floatFramebuffer = false;
};

struct SlangLUT {
    std::string id;
    std::string path;
    bool linear = false;
};

class SlangPreset {
public:
    SlangPreset();
    ~SlangPreset();

    bool load(const std::string& filename);
    const std::vector<SlangShaderPass>& getPasses() const { return m_passes; }
    const std::vector<SlangLUT>& getLUTs() const { return m_luts; }
    const std::map<std::string, float>& getParameters() const { return m_parameters; }
    const std::vector<SlangParameter>& getPragmaParameters() const { return m_pragma_parameters; }
    void clear() { m_passes.clear(); m_luts.clear(); m_parameters.clear(); m_pragma_parameters.clear(); }
    // Concatenates passes from another preset into this one, merging LUTs and parameters. (used for stacking filters)
    void appendPassesFrom(const SlangPreset& other);

private:
    std::vector<SlangShaderPass> m_passes;
    std::vector<SlangLUT> m_luts;
    std::map<std::string, float> m_parameters;
    std::vector<SlangParameter> m_pragma_parameters;

    std::string trim(const std::string& str);
    std::string removeQuotes(const std::string& str);
    // Recursively parses #pragma parameter directives from a .slang file and its #include tree
    void parsePragmasRecursive(const std::string& filePath, const std::string& baseDir, std::vector<std::string>& visited);
};

#endif // SLANGPRESET_H
