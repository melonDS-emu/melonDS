#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

struct NameOrIndex {
    u8 value[8];
    std::variant<std::string, u32> Get() {
        u32 temp;
        std::memcpy(&temp, value, 4);
        if (temp == 0) {
            std::memcpy(&temp, value + 4, 4);
            return temp;
        }
        std::string ret(value, value + 8);
        size_t pos = ret.find('\0');
        if (pos != std::string::npos)
            ret.erase(pos);
        return ret;
    }
    std::string GetString(std::FILE* file, u32 offset) {
        auto v = Get();
        if (v.index() == 1) {
            std::fseek(file, offset + std::get<1>(v), SEEK_SET);
            char c;
            std::string ret;
            while (true) {
                if (std::fread(&c, 1, 1, file) != 1)
                    throw "unexpected end";
                if (c == '\0')
                    break;
                ret += c;
            }
            return ret;
        } else {
            return std::get<0>(v);
        }
    }
};

struct Header {
    u16 magic;
    u16 num_section;
    u32 time;
    u32 offset_symbol;
    u32 num_symbol;
    u16 optheader_size;
    u16 flags;
};
static_assert(sizeof(Header) == 20);

struct SectionHeader {
    NameOrIndex name;
    u32 prog_addr;
    u32 data_addr;
    u32 size;
    u32 offset_data;
    u32 offset_rel;
    u32 offset_line;
    u16 num_rel;
    u16 num_line;
    u32 flags;
};
static_assert(sizeof(SectionHeader) == 40);

namespace SFlag {
constexpr u32 Exec = 0x0001;
constexpr u32 Unk2 = 0x0002;
constexpr u32 Prog = 0x0008;
constexpr u32 Data = 0x0010;
constexpr u32 RegionMask = Prog | Data;
constexpr u32 Moni = 0x0020;
constexpr u32 Un40 = 0x0040;
constexpr u32 Dupl = 0x0080;
constexpr u32 U200 = 0x0200;
} // namespace SFlag

#pragma pack(push, 1)
struct Symbol {
    NameOrIndex name;
    u32 value;
    u16 section_index;
    u16 type;
    u8 storage;
    u8 num_aux;
};
static_assert(sizeof(Symbol) == 18);

inline const std::unordered_map<u32, const char*> storage_names{
    {0, "label"},    {1, "auto"},       {2, "external"},  {3, "static"},
    {4, "register"}, {8, "struct"},     {9, "arg"},       {10, "struct-tag"},
    {11, "union"},   {12, "union-tag"}, {13, "typedef"},  {15, "enum-tag"},
    {16, "enum"},    {18, "bitfield"},  {19, "auto-arg"}, {98, "start"},
    {99, "end"},     {100, "block"},    {101, "func"},    {102, "struct-size"},
    {103, "file"},   {107, "ar"},       {108, "ar"},      {109, "ar"},
    {110, "ar"},     {111, "ar"},       {112, "ar"},      {255, "physical-function-end"},
};

/*
Storage:
0 labels? | section+address | type = 0 | no aux
1 auto variable | address, rel to sp? | variable type | variable aux
2 externals | section+address | variable type | variable aux
3 statics | section+address | variable type | variable aux
4 register variable | address, abs? | variable type | variable aux
8 structure member | address | variable type | variable aux
9 function argument | address, ? | variable type | variable aux
10 structure tag | debug/no address | type = 8 (structure) | aux = 1
11 union member | address = 0? | type = 5 (long) or 8 (structure) | aux = 0 (long) or 1 (structure)
12 union tag | debug/no address | type = 9 (union) | aux = 1
13 typedef | debug/no address | type = 5 (long) or 8 (structure) | aux = 0 (long) or 1 (structure)
15 enum tag | debug/no address | type = 10 (enum) | aux = 1
16 enum member | abs value  | type = 11 (enum member) | aux = 0
18 bitfield | address | type = 5 (long) or 15 ulong | aux = 1
19 auto arg | section+address | type = 17 | aux = 0
98 function start | section + address | type = 0 | aux = 0
99 function end | section + address | type = 0 | aux = 0
100 .eb/.bb | section + address | type = 0 | aux = 1
101 .ef/.bf | section + address | type = 0 | aux = 1
102 struct end | address | type = 0 | aux = 1
103 .file | debug value | type = 1 or 2 | aux = 1
107 .ar | section + address | type = => ar0 value | aux = 0
108 .ar | section + address | type = => ar1 value | aux = 0
109 .ar | section + address | type = => arp0 value | aux = 0
110 .ar | section + address | type = => arp1 value | aux = 0
111 .ar | section + address | type = => arp2 value | aux = 0
112 .ar | section + address | type = => arp3 value | aux = 0
255 physical function end | section + address | type = 0 | aux = 0
*/

struct Line {
    u32 symbol_index_or_addr;
    u16 line_number;
};
static_assert(sizeof(Line) == 6);

struct Relocation {
    u32 addr;
    u32 symbol;
    u32 type;
};
static_assert(sizeof(Relocation) == 12); // not 10 bytes!
#pragma pack(pop)

class Coff {

public:
    Coff(std::FILE* in) {
        fseek(in, 0, SEEK_SET);
        Header header;
        if (fread(&header, sizeof(header), 1, in) != 1)
            throw "failed to read header";
        u32 string_offset = header.offset_symbol + header.num_symbol * sizeof(Symbol);

        sections.resize(header.num_section);
        u32 expected = 0;
        for (u32 i = 0; i < header.num_section; ++i) {
            fseek(in, sizeof(header) + header.optheader_size + i * sizeof(SectionHeader), SEEK_SET);
            SectionHeader sheader;
            if (fread(&sheader, sizeof(sheader), 1, in) != 1)
                throw "failed to read sheader";
            sections[i].name = sheader.name.GetString(in, string_offset);
            sections[i].prog_addr = sheader.prog_addr;
            sections[i].data_addr = sheader.data_addr;
            if (expected == 0)
                expected = sheader.offset_data;
            if (expected != sheader.offset_data) {
                throw "";
            }
            sections[i].data.resize(sheader.size);
            if ((sheader.flags & SFlag::Dupl) == 0) {
                fseek(in, sheader.offset_data, SEEK_SET);
                if (sheader.size != 0)
                    if (fread(sections[i].data.data(), sheader.size, 1, in) != 1)
                        throw "failed to read section";
                expected += sheader.size;
            }
            sections[i].num_rel = sheader.num_rel;
            sections[i].flags = sheader.flags;
            if ((sections[i].flags & SFlag::RegionMask) == 0)
                throw "no region type";
            if ((sections[i].flags & SFlag::RegionMask) == SFlag::Prog) {
                if (sections[i].data_addr != 0)
                    throw "prog section has data addr";
            } else if ((sections[i].flags & SFlag::RegionMask) == SFlag::Data) {
                if (sections[i].prog_addr != 0)
                    throw "data section has prog addr";
            }

            fseek(in, sheader.offset_line, SEEK_SET);
            u32 current_symbol = 0;
            for (u16 j = 0; j < sheader.num_line; ++j) {
                Line line;
                if (fread(&line, sizeof(line), 1, in) != 1)
                    throw "failed to read line";
                if (line.line_number == 0) {
                    current_symbol = line.symbol_index_or_addr;
                } else {
                    u32 addr = line.symbol_index_or_addr;
                    if (addr < sections[i].prog_addr ||
                        addr >= sections[i].prog_addr + sections[i].data.size() / 2) {
                        if (addr != 0xFFFFFFFF && addr != 0xFFFFFFFE &&
                            addr != 0xFFFFFFFD) // unknown magic
                            throw "line out of range";
                    }
                    sections[i].line_numbers[addr].emplace_back(
                        Section::LineNumber{current_symbol, line.line_number});
                }
            }

            fseek(in, sheader.offset_rel, SEEK_SET);
            // printf("\n");
            for (u16 j = 0; j < sheader.num_rel; ++j) {
                Relocation relocation;
                if (fread(&relocation, sizeof(relocation), 1, in) != 1)
                    throw "failed to read relocation";
                // printf("%08X, %08X, %08X\n", relocation.addr, relocation.symbol,
                // relocation.type);
                sections[i].relocations[relocation.addr].push_back(relocation);
            }
        }

        for (u32 i = 0; i < header.num_symbol; ++i) {
            Symbol symbol;
            fseek(in, header.offset_symbol + i * sizeof(symbol), SEEK_SET);
            if (fread(&symbol, sizeof(symbol), 1, in) != 1)
                throw "failed to read symbol";
            i += symbol.num_aux;
            printf("%s\n", symbol.name.GetString(in, string_offset).c_str());
            printf("value = %08X, section = %04X, type = %04X, storage = %02X, aux = %02X\n",
                   symbol.value, symbol.section_index, symbol.type, symbol.storage, symbol.num_aux);
            SymbolEx symbol_ex;
            symbol_ex.name = symbol.name.GetString(in, string_offset);
            if (symbol.section_index > 0 && symbol.section_index < 0x7FFF) {
                u16 section_index = symbol.section_index - 1;
                symbol_ex.region =
                    sections[section_index].flags & SFlag::Prog ? SymbolEx::Prog : SymbolEx::Data;
                symbol_ex.value = symbol.value + (sections[section_index].flags & SFlag::Prog
                                                      ? sections[section_index].prog_addr
                                                      : sections[section_index].data_addr);

            } else {
                symbol_ex.region = SymbolEx::Absolute;
                symbol_ex.value = symbol.value;
            }

            symbol_ex.storage = symbol.storage;
            symbol_ex.type = symbol.type;

            symbols.push_back(symbol_ex);

            if (symbols.back().region == SymbolEx::Prog ||
                symbols.back().region == SymbolEx::Data) {
                symbols_lut[symbols.back().value].push_back(symbols.size() - 1);
            }

            symbol_ex.region = SymbolEx::Aux;
            for (u16 aux = 0; aux < symbol.num_aux; ++aux) {
                symbols.push_back(symbol_ex);
            }
        }

        // Merge duplicated sections
        std::sort(sections.begin(), sections.end(), [](const Section& left, const Section& right) {
            u32 lm = left.flags & SFlag::Dupl;
            u32 rm = right.flags & SFlag::Dupl;
            if (lm == rm) {
                return left.name < right.name;
            } else {
                return lm < rm;
            }
        });
        auto mid = sections.begin() + sections.size() / 2;
        for (auto a = sections.begin(), b = mid; a != mid; ++a, ++b) {
            if (a->name != b->name || a->prog_addr != b->prog_addr ||
                a->data_addr != b->data_addr || a->data.size() != b->data.size() ||
                (a->flags + SFlag::Dupl) != b->flags)
                throw "mismatch";
            if (!b->line_numbers.empty())
                throw "dup has line numbers";
            if (b->num_rel != 0)
                throw "dup has relocations";
        }
        sections.resize(sections.size() / 2);

        // Break up multi-region section
        auto both_begin =
            std::partition(sections.begin(), sections.end(), [](const Section& section) {
                return (section.flags & SFlag::RegionMask) != SFlag::RegionMask;
            });
        std::vector<Section> copy(both_begin, sections.end());
        for (auto i = both_begin; i != sections.end(); ++i) {
            i->flags &= ~SFlag::Prog;
            i->prog_addr = 0;
        }
        for (auto& section : copy) {
            section.flags &= ~SFlag::Data;
            section.data_addr = 0;
        }
        sections.insert(sections.end(), std::make_move_iterator(copy.begin()),
                        std::make_move_iterator(copy.end()));

        // Sort according to address
        std::sort(sections.begin(), sections.end(), [](const Section& left, const Section& right) {
            if ((left.flags & SFlag::RegionMask) == SFlag::Prog) {
                if ((right.flags & SFlag::RegionMask) == SFlag::Prog) {
                    if (left.prog_addr == right.prog_addr) {
                        return left.data.size() < right.data.size();
                    }
                    return left.prog_addr < right.prog_addr;
                } else {
                    return true;
                }
            } else {
                if ((right.flags & SFlag::RegionMask) == SFlag::Prog) {
                    return false;
                } else {
                    if (left.data_addr == right.data_addr) {
                        return left.data.size() < right.data.size();
                    }
                    return left.data_addr < right.data_addr;
                }
            }
        });
    }

    struct Section {
        std::string name;
        u32 prog_addr;
        u32 data_addr;
        std::vector<u8> data;
        u16 num_rel;
        u32 flags;

        struct LineNumber {
            u32 symbol;
            u16 line;
        };
        std::unordered_map<u32 /*abs addr*/, std::vector<LineNumber>> line_numbers;
        std::unordered_map<u32 /*rel addr*/, std::vector<Relocation>> relocations;
    };
    std::vector<Section> sections;

    struct SymbolEx {
        std::string name;
        enum Region {
            Aux,
            Absolute,
            Prog,
            Data,
        } region;
        u32 value;
        u16 type;
        u8 storage;
    };
    std::vector<SymbolEx> symbols;
    std::unordered_map<u32 /*abs addr*/, std::vector<std::size_t /*index in symbols*/>> symbols_lut;
};
