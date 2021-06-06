#pragma once
#include <memory>
#include <string>
#include <vector>
#include "common_types.h"

namespace Teakra {

class Parser {
public:
    virtual ~Parser() = default;
    struct Opcode {
        enum {
            Invalid,
            Valid,
            ValidWithExpansion,
        } status = Invalid;
        u16 opcode = 0;
    };

    virtual Opcode Parse(const std::vector<std::string>& tokens) = 0;
};

std::unique_ptr<Parser> GenerateParser();

} // namespace Teakra
