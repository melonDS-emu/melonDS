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

#include "SlangPreset.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <map>
#include <regex>

SlangPreset::SlangPreset() = default;

SlangPreset::~SlangPreset() = default;

std::string SlangPreset::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string SlangPreset::removeQuotes(const std::string& str) {
    std::string res = str;
    if (res.length() >= 2 && res.front() == '"' && res.back() == '"') {
        res = res.substr(1, res.length() - 2);
    }
    return res;
}

void SlangPreset::parsePragmasRecursive(const std::string& filePath, const std::string& baseDir, std::vector<std::string>& visited) {
    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open()) return;

    std::regex pragmaRegex("#pragma\\s+parameter\\s+([^\\s]+)\\s+\"([^\"]+)\"\\s+([-+]?[0-9]*\\.?[0-9]+)\\s+([-+]?[0-9]*\\.?[0-9]+)\\s+([-+]?[0-9]*\\.?[0-9]+)\\s+([-+]?[0-9]*\\.?[0-9]+)");
    std::regex includeRegex("#include\\s+\"([^\"]+)\"");

    std::string line;
    while (std::getline(shaderFile, line)) {
        std::smatch match;
        if (std::regex_search(line, match, pragmaRegex)) {
            try {
                SlangParameter param;
                param.name = match[1];
                param.description = match[2];
                param.defaultValue = std::stof(match[3]);
                param.minValue = std::stof(match[4]);
                param.maxValue = std::stof(match[5]);
                param.step = std::stof(match[6]);

                // 10x range expansion for extra slider flexibility
                if (param.minValue >= 0.0f) {
                    param.maxValue *= 10.0f;
                } else if (param.minValue < 0.0f && param.maxValue > 0.0f) {
                    param.minValue *= 5.0f;
                    param.maxValue *= 5.0f;
                } else if (param.maxValue <= 0.0f) {
                    param.minValue *= 10.0f;
                }

                m_pragma_parameters.push_back(param);
            } catch (...) {}
        } else if (std::regex_search(line, match, includeRegex)) {
            std::string includePath = baseDir + "/" + match[1].str();
            
            if (std::find(visited.begin(), visited.end(), includePath) == visited.end()) {
                visited.push_back(includePath);
                parsePragmasRecursive(includePath, baseDir, visited);
            }
        }
    }
}

void SlangPreset::appendPassesFrom(const SlangPreset& other) {
    for (const auto& pass : other.m_passes) {
        SlangShaderPass newPass = pass;
        newPass.alias = "";
        m_passes.push_back(newPass);
    }
    for (const auto& lut : other.m_luts) {
        bool found = false;
        for (const auto& existing : m_luts) {
            if (existing.id == lut.id) { found = true; break; }
        }
        if (!found) m_luts.push_back(lut);
    }
    for (const auto& [key, val] : other.m_parameters) {
        m_parameters[key] = val;
    }
    for (const auto& pparam : other.m_pragma_parameters) {
        bool found = false;
        for (auto& existing : m_pragma_parameters) {
            if (existing.name == pparam.name) {
                existing = pparam;
                found = true;
                break;
            }
        }
        if (!found) m_pragma_parameters.push_back(pparam);
    }
}

bool SlangPreset::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open slangp file: " << filename << std::endl;
        return false;
    }

    std::string baseDir = "";
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseDir = filename.substr(0, lastSlash + 1);
    }

    clear();
    std::map<std::string, std::string> config;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = trim(line.substr(0, equalPos));
            std::string value = trim(line.substr(equalPos + 1));
            value = removeQuotes(value);
            config[key] = value;
        }
    }

    int numShaders = 0;
    if (config.find("shaders") != config.end()) {
        try {
            numShaders = std::stoi(config["shaders"]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid shaders count: " << config["shaders"] << std::endl;
            return false;
        }
    }

    for (int i = 0; i < numShaders; ++i) {
        SlangShaderPass pass;
        std::string prefix = std::to_string(i);

        if (config.find("shader" + prefix) != config.end()) {
            std::string path = config["shader" + prefix];
            if (!path.empty() && path[0] != '/' && path[0] != '\\' && path.find(":") == std::string::npos) {
                pass.shaderPath = baseDir + path;
            } else {
                pass.shaderPath = path;
            }
        } else {
            std::cerr << "Missing shader path for pass " << i << std::endl;
            continue;
        }

        if (config.find("filter_linear" + prefix) != config.end()) {
            pass.filterLinear = (config["filter_linear" + prefix] == "true");
        }
        
        if (config.find("srgb_framebuffer" + prefix) != config.end()) {
            pass.srgbFramebuffer = (config["srgb_framebuffer" + prefix] == "true");
        }

        if (config.find("float_framebuffer" + prefix) != config.end()) {
            pass.floatFramebuffer = (config["float_framebuffer" + prefix] == "true");
        }

        if (config.find("alias" + prefix) != config.end()) {
            pass.alias = config["alias" + prefix];
        }

        if (config.find("scale_type" + prefix) != config.end()) {
            pass.scaleTypeX = config["scale_type" + prefix];
            pass.scaleTypeY = config["scale_type" + prefix];
        }
        if (config.find("scale_type_x" + prefix) != config.end()) {
            pass.scaleTypeX = config["scale_type_x" + prefix];
        }
        if (config.find("scale_type_y" + prefix) != config.end()) {
            pass.scaleTypeY = config["scale_type_y" + prefix];
        }

        try {
            if (config.find("scale" + prefix) != config.end()) {
                pass.scaleX = std::stof(config["scale" + prefix]);
                pass.scaleY = pass.scaleX;
            }
            if (config.find("scale_x" + prefix) != config.end()) {
                pass.scaleX = std::stof(config["scale_x" + prefix]);
            }
            if (config.find("scale_y" + prefix) != config.end()) {
                pass.scaleY = std::stof(config["scale_y" + prefix]);
            }
        } catch (const std::exception& e) {
            std::cerr << "Invalid scale value for pass " << i << std::endl;
        }

        size_t lastSlashPragma = pass.shaderPath.find_last_of("/\\");
        std::string shaderBaseDir = (lastSlashPragma == std::string::npos) ? "." : pass.shaderPath.substr(0, lastSlashPragma);
        std::vector<std::string> visited;
        parsePragmasRecursive(pass.shaderPath, shaderBaseDir, visited);

        m_passes.push_back(pass);
    }

    // Deduplicate parameters that appear in multiple passes via shared #include files (e.g. gb-params.inc)
    {
        std::map<std::string, SlangParameter> deduped;
        for (const auto& p : m_pragma_parameters) {
            deduped[p.name] = p;
        }
        m_pragma_parameters.clear();
        for (const auto& [name, p] : deduped) {
            m_pragma_parameters.push_back(p);
        }
    }

    if (config.find("textures") != config.end()) {
        std::stringstream ss(config["textures"]);
        std::string textureId;
        while (std::getline(ss, textureId, ';')) {
            textureId = trim(textureId);
            if (textureId.empty()) continue;

            if (config.find(textureId) != config.end()) {
                SlangLUT lut;
                lut.id = textureId;
                std::string path = config[textureId];
                if (!path.empty() && path[0] != '/' && path[0] != '\\' && path.find(":") == std::string::npos) {
                    lut.path = baseDir + path;
                } else {
                    lut.path = path;
                }
                
                if (config.find(textureId + "_linear") != config.end()) {
                    lut.linear = (config[textureId + "_linear"] == "true");
                }
                m_luts.push_back(lut);
            }
        }
    }

    for (const auto& [key, value] : config) {
        if (key == "shaders" || key == "textures" || 
            key.find("shader") == 0 || key.find("filter_linear") == 0 || 
            key.find("srgb_framebuffer") == 0 || key.find("scale_type") == 0 || 
            key.find("scale") == 0 || key.find("alias") == 0 || 
            key.find("wrap_mode") == 0 || key.find("mipmap_input") == 0 || 
            key.find("float_framebuffer") == 0 || key.find("frame_count_mod") == 0) {
            continue;
        }
        
        try {
            float val = std::stof(value);
            m_parameters[key] = val;
        } catch (...) {
        }
    }

    return true;
}