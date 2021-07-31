#include <algorithm>
#include <functional>
#include <optional>
#include <unordered_map>
#include <variant>
#include "../include/teakra/disassembler.h"
#include "common_types.h"
#include "crash.h"
#include "parser.h"

using NodeAsConst = std::string;
struct NodeAsExpansion {};

bool operator==(NodeAsExpansion, NodeAsExpansion) {
    return true;
}

namespace std {
template <>
struct hash<NodeAsExpansion> {
    typedef NodeAsExpansion argument_type;
    typedef std::size_t result_type;
    result_type operator()(argument_type const& s) const {
        return 0x12345678;
    }
};
} // namespace std

namespace Teakra {
class ParserImpl : public Parser {
public:
    using NodeKey = std::variant<NodeAsConst, NodeAsExpansion>;

    struct Node {
        bool end = false;
        u16 opcode = 0;
        bool expansion = false;
        std::unordered_map<NodeKey, std::unique_ptr<Node>> children;
    };

    Node root;

    Opcode Parse(const std::vector<std::string>& tokens) override {
        Node* current = &root;
        for (auto& token : tokens) {
            auto const_find = current->children.find(token);
            if (const_find != current->children.end()) {
                current = const_find->second.get();
            } else {
                return Opcode{Opcode::Invalid};
            }
        }
        if (!current->end) {
            return Opcode{Opcode::Invalid};
        }
        return Opcode{current->expansion ? Opcode::ValidWithExpansion : Opcode::Valid,
                      current->opcode};
    }
};

std::unique_ptr<Parser> GenerateParser() {
    std::unique_ptr<ParserImpl> parser = std::make_unique<ParserImpl>();
    for (u32 opcode = 0; opcode < 0x10000; ++opcode) {
        u16 o = (u16)opcode;
        bool expansion = Disassembler::NeedExpansion(o);
        auto tokens = Disassembler::GetTokenList(o);

        if (std::any_of(tokens.begin(), tokens.end(), [](const auto& token) {
                return token.find("[ERROR]") != std::string::npos;
            }))
            continue;

        ParserImpl::Node* current = &parser->root;
        for (const auto& token : tokens) {
            auto& next = current->children[token];
            if (!next)
                next = std::make_unique<ParserImpl::Node>();
            current = next.get();
        }

        if (current->end) {
            ASSERT((current->opcode & (u16)(~o)) == 0);
            continue;
        }
        current->end = true;
        current->opcode = o;
        current->expansion = expansion;
    }
    return parser;
}

} // namespace Teakra
