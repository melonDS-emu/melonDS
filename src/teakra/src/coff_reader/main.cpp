#include <cstdio>
#include <string>
#include <teakra/disassembler.h>
#include "coff.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Please input a file\n");
        return -1;
    }
    std::FILE* in;
    std::FILE* out;
    in = std::fopen(argv[1], "rb");
    if (!in) {
        printf("Failed to open input file\n");
        return -1;
    }
    out = std::fopen((argv[1] + std::string(".out")).c_str(), "wt");
    if (!out) {
        printf("Failed to open output file\n");
        return -1;
    }

    Coff coff(in);

    u32 prog_grow = 0, data_grow = 0;
    for (const auto& section : coff.sections) {
        u32 addr;
        bool is_prog;
        if ((section.flags & SFlag::RegionMask) == SFlag::Prog) {
            if (section.prog_addr < prog_grow) {
                throw "overlap";
            }
            is_prog = true;
            addr = section.prog_addr;
            prog_grow = section.prog_addr + (u32)section.data.size() / 2;
        } else {
            if (section.data_addr < data_grow) {
                throw "overlap";
            }
            is_prog = false;
            addr = section.data_addr;
            data_grow = section.data_addr + (u32)section.data.size() / 2;
        }

        fprintf(out, "\n===Section===\n");
        fprintf(out, "name: %s\n", section.name.c_str());
        fprintf(out, "addr: %s.%08X\n", is_prog ? "Prog" : "Data", addr);
        fprintf(out, "flag: %08X\n\n", section.flags);

        std::vector<u16> data(section.data.size() / 2);
        std::memcpy(data.data(), section.data.data(), section.data.size());

        for (auto iter = data.begin(); iter != data.end(); ++iter) {
            u32 current_rel_addr = (u32)(iter - data.begin());
            u32 current_addr = current_rel_addr + addr;

            if (coff.symbols_lut.count(current_addr)) {
                const auto& symbol_list = coff.symbols_lut.at(current_addr);
                auto begin = symbol_list.begin();
                while (true) {
                    auto symbol_index =
                        std::find_if(begin, symbol_list.end(),
                                     [is_prog, current_addr, &coff](const auto symbol_index) {
                                         const auto& symbol = coff.symbols[symbol_index];
                                         if (symbol.region == Coff::SymbolEx::Absolute ||
                                             symbol.region == Coff::SymbolEx::Aux)
                                             throw "what";
                                         if (symbol.region == Coff::SymbolEx::Prog && !is_prog)
                                             return false;
                                         if (symbol.region == Coff::SymbolEx::Data && is_prog)
                                             return false;
                                         if (symbol.value != current_addr)
                                             throw "what";
                                         return true;
                                     });
                    if (symbol_index != symbol_list.end()) {
                        const auto& symbol = coff.symbols[*symbol_index];
                        fprintf(out, "$[%s]%s\n", storage_names.at(symbol.storage),
                                symbol.name.c_str());
                    } else {
                        break;
                    }
                    begin = symbol_index + 1;
                }
            }

            if (section.line_numbers.count(current_addr)) {
                for (const auto& line_number : section.line_numbers.at(current_addr)) {
                    const auto& symbol = coff.symbols[line_number.symbol];
                    fprintf(out, "#%d + $[%s]%s\n", line_number.line,
                            storage_names.at(symbol.storage), symbol.name.c_str());
                }
            }

            fprintf(out, ".%08X    ", current_addr);
            fprintf(out, "%04X", *iter);

            if (section.flags & SFlag::Exec) {
                u16 opcode = *iter;
                std::string dsm;
                if (Teakra::Disassembler::NeedExpansion(opcode)) {
                    ++iter;
                    if (iter == data.end()) {
                        fprintf(out, "[broken expansion]\n");
                        break;
                    }
                    u16 exp = *iter;
                    dsm = Teakra::Disassembler::Do(opcode, exp);
                    fprintf(out, " %04X ", exp);
                } else {
                    dsm = Teakra::Disassembler::Do(opcode);
                    fprintf(out, "      ");
                }
                fprintf(out, "%s  ;", dsm.c_str());
            }

            if (section.relocations.count(current_rel_addr)) {
                for (const auto& relocation : section.relocations.at(current_rel_addr)) {
                    const auto& symbol = coff.symbols[relocation.symbol];
                    fprintf(out, "{@%08X + $[%s]%s}", relocation.type,
                            storage_names.at(symbol.storage), symbol.name.c_str());
                }
            }

            fprintf(out, "\n");
        }
    }

    std::fclose(in);
    std::fclose(out);
}
