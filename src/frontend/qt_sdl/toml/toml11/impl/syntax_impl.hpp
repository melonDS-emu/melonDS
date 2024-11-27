#ifndef TOML11_SYNTAX_IMPL_HPP
#define TOML11_SYNTAX_IMPL_HPP

#include "../fwd/syntax_fwd.hpp"
#include "../scanner.hpp"
#include "../spec.hpp"

namespace toml
{
namespace detail
{
namespace syntax
{

using char_type = location::char_type;

// ===========================================================================
// UTF-8

// avoid redundant representation and out-of-unicode sequence

TOML11_INLINE character_in_range utf8_1byte(const spec&)
{
    return character_in_range(0x00, 0x7F);
}

TOML11_INLINE sequence utf8_2bytes(const spec&)
{
    return sequence(character_in_range(0xC2, 0xDF),
                    character_in_range(0x80, 0xBF));
}

TOML11_INLINE sequence utf8_3bytes(const spec&)
{
    return sequence(/*1~2 bytes = */either(
        sequence(character         (0xE0),       character_in_range(0xA0, 0xBF)),
        sequence(character_in_range(0xE1, 0xEC), character_in_range(0x80, 0xBF)),
        sequence(character         (0xED),       character_in_range(0x80, 0x9F)),
        sequence(character_in_range(0xEE, 0xEF), character_in_range(0x80, 0xBF))
    ), /*3rd byte = */ character_in_range(0x80, 0xBF));
}

TOML11_INLINE sequence utf8_4bytes(const spec&)
{
    return sequence(/*1~2 bytes = */either(
        sequence(character         (0xF0),       character_in_range(0x90, 0xBF)),
        sequence(character_in_range(0xF1, 0xF3), character_in_range(0x80, 0xBF)),
        sequence(character         (0xF4),       character_in_range(0x80, 0x8F))
    ), character_in_range(0x80, 0xBF), character_in_range(0x80, 0xBF));
}

TOML11_INLINE non_ascii::non_ascii(const spec& s) noexcept
    : scanner_(utf8_2bytes(s), utf8_3bytes(s), utf8_4bytes(s))
{}


// ===========================================================================
// Whitespace

TOML11_INLINE character_either wschar(const spec&)
{
    return character_either{char_type(' '), char_type('\t')};
}

TOML11_INLINE repeat_at_least ws(const spec& s)
{
    return repeat_at_least(0, wschar(s));
}

// ===========================================================================
// Newline

TOML11_INLINE either newline(const spec&)
{
    return either(character(char_type('\n')), literal("\r\n"));
}

// ===========================================================================
// Comments

TOML11_INLINE either allowed_comment_char(const spec& s)
{
    if(s.v1_1_0_allow_control_characters_in_comments)
    {
        return either(
            character_in_range(0x01, 0x09),
            character_in_range(0x0E, 0x7F),
            non_ascii(s)
        );
    }
    else
    {
        return either(
            character(0x09),
            character_in_range(0x20, 0x7E),
            non_ascii(s)
        );
    }
}

// XXX Note that it does not take newline
TOML11_INLINE sequence comment(const spec& s)
{
    return sequence(character(char_type('#')),
                    repeat_at_least(0, allowed_comment_char(s)));
}

// ===========================================================================
// Boolean

TOML11_INLINE either boolean(const spec&)
{
    return either(literal("true"), literal("false"));
}

// ===========================================================================
// Integer

TOML11_INLINE digit::digit(const spec&) noexcept
    : scanner_(char_type('0'), char_type('9'))
{}

TOML11_INLINE alpha::alpha(const spec&) noexcept
    : scanner_(
        character_in_range(char_type('a'), char_type('z')),
        character_in_range(char_type('A'), char_type('Z'))
    )
{}

TOML11_INLINE hexdig::hexdig(const spec& s) noexcept
    : scanner_(
        digit(s),
        character_in_range(char_type('a'), char_type('f')),
        character_in_range(char_type('A'), char_type('F'))
    )
{}

// non-digit-graph = ([a-zA-Z]|unicode mb char)
// graph           = ([a-zA-Z0-9]|unicode mb char)
// suffix          = _ non-digit-graph (graph | _graph)
TOML11_INLINE sequence num_suffix(const spec& s)
{
    const auto non_digit_graph = [&s]() {
        return either(
            alpha(s),
            non_ascii(s)
        );
    };
    const auto graph = [&s]() {
        return either(
            alpha(s),
            digit(s),
            non_ascii(s)
        );
    };

    return sequence(
            character(char_type('_')),
            non_digit_graph(),
            repeat_at_least(0,
                either(
                    sequence(character(char_type('_')), graph()),
                    graph()
                )
            )
        );
}

TOML11_INLINE sequence dec_int(const spec& s)
{
    const auto digit19 = []() {
        return character_in_range(char_type('1'), char_type('9'));
    };
    return sequence(
            maybe(character_either{char_type('-'), char_type('+')}),
            either(
                sequence(
                    digit19(),
                    repeat_at_least(1,
                        either(
                            digit(s),
                            sequence(character(char_type('_')), digit(s))
                        )
                    )
                ),
                digit(s)
            )
        );
}

TOML11_INLINE sequence hex_int(const spec& s)
{
    return sequence(
            literal("0x"),
            hexdig(s),
            repeat_at_least(0,
                either(
                    hexdig(s),
                    sequence(character(char_type('_')), hexdig(s))
                )
            )
        );
}

TOML11_INLINE sequence oct_int(const spec&)
{
    const auto digit07 = []() {
        return character_in_range(char_type('0'), char_type('7'));
    };
    return sequence(
            literal("0o"),
            digit07(),
            repeat_at_least(0,
                either(
                    digit07(),
                    sequence(character(char_type('_')), digit07())
                )
            )
        );
}

TOML11_INLINE sequence bin_int(const spec&)
{
    const auto digit01 = []() {
        return character_either{char_type('0'), char_type('1')};
    };
    return sequence(
            literal("0b"),
            digit01(),
            repeat_at_least(0,
                either(
                    digit01(),
                    sequence(character(char_type('_')), digit01())
                )
            )
        );
}

TOML11_INLINE either integer(const spec& s)
{
    return either(
            hex_int(s),
            oct_int(s),
            bin_int(s),
            dec_int(s)
        );
}


// ===========================================================================
// Floating

TOML11_INLINE sequence zero_prefixable_int(const spec& s)
{
    return sequence(
            digit(s),
            repeat_at_least(0,
                either(
                    digit(s),
                    sequence(character('_'), digit(s))
                )
            )
        );
}

TOML11_INLINE sequence fractional_part(const spec& s)
{
    return sequence(
            character('.'),
            zero_prefixable_int(s)
        );
}

TOML11_INLINE sequence exponent_part(const spec& s)
{
    return sequence(
            character_either{char_type('e'), char_type('E')},
            maybe(character_either{char_type('+'), char_type('-')}),
            zero_prefixable_int(s)
        );
}

TOML11_INLINE sequence hex_floating(const spec& s)
{
    // C99 hexfloat (%a)
    // [+-]? 0x ( [0-9a-fA-F]*\.[0-9a-fA-F]+ | [0-9a-fA-F]+\.? ) [pP] [+-]? [0-9]+

    // - 0x(int).(frac)p[+-](int)
    // - 0x(int).p[+-](int)
    // - 0x.(frac)p[+-](int)
    // - 0x(int)p[+-](int)

    return sequence(
            maybe(character_either{char_type('+'), char_type('-')}),
            character('0'),
            character_either{char_type('x'), char_type('X')},
            either(
                sequence(
                    repeat_at_least(0, hexdig(s)),
                    character('.'),
                    repeat_at_least(1, hexdig(s))
                ),
                sequence(
                    repeat_at_least(1, hexdig(s)),
                    maybe(character('.'))
                )
            ),
            character_either{char_type('p'), char_type('P')},
            maybe(character_either{char_type('+'), char_type('-')}),
            repeat_at_least(1, character_in_range('0', '9'))
        );
}

TOML11_INLINE either floating(const spec& s)
{
    return either(
            sequence(
                dec_int(s),
                either(
                    exponent_part(s),
                    sequence(fractional_part(s), maybe(exponent_part(s)))
                )
            ),
            sequence(
                maybe(character_either{char_type('-'), char_type('+')}),
                either(literal("inf"), literal("nan"))
            )
        );
}

// ===========================================================================
// Datetime

TOML11_INLINE sequence local_date(const spec& s)
{
    return sequence(
            repeat_exact(4, digit(s)),
            character('-'),
            repeat_exact(2, digit(s)),
            character('-'),
            repeat_exact(2, digit(s))
        );
}
TOML11_INLINE sequence local_time(const spec& s)
{
    auto time = sequence(
            repeat_exact(2, digit(s)),
            character(':'),
            repeat_exact(2, digit(s))
        );

    if(s.v1_1_0_make_seconds_optional)
    {
        time.push_back(maybe(sequence(
                character(':'),
                repeat_exact(2, digit(s)),
                maybe(sequence(character('.'), repeat_at_least(1, digit(s))))
            )));
    }
    else
    {
        time.push_back(character(':'));
        time.push_back(repeat_exact(2, digit(s)));
        time.push_back(
            maybe(sequence(character('.'), repeat_at_least(1, digit(s))))
        );
    }

    return time;
}
TOML11_INLINE either time_offset(const spec& s)
{
    return either(
            character_either{'Z', 'z'},
            sequence(character_either{'+', '-'},
                     repeat_exact(2, digit(s)),
                     character(':'),
                     repeat_exact(2, digit(s))
             )
        );
}
TOML11_INLINE sequence full_time(const spec& s)
{
    return sequence(local_time(s), time_offset(s));
}
TOML11_INLINE character_either time_delim(const spec&)
{
    return character_either{'T', 't', ' '};
}
TOML11_INLINE sequence local_datetime(const spec& s)
{
    return sequence(local_date(s), time_delim(s), local_time(s));
}
TOML11_INLINE sequence offset_datetime(const spec& s)
{
    return sequence(local_date(s), time_delim(s), full_time(s));
}

// ===========================================================================
// String

TOML11_INLINE sequence escaped(const spec& s)
{
    character_either escape_char{
        '\"','\\', 'b', 'f', 'n', 'r', 't'
    };
    if(s.v1_1_0_add_escape_sequence_e)
    {
        escape_char.push_back(char_type('e'));
    }

    either escape_seq(
            std::move(escape_char),
            sequence(character('u'), repeat_exact(4, hexdig(s))),
            sequence(character('U'), repeat_exact(8, hexdig(s)))
        );

    if(s.v1_1_0_add_escape_sequence_x)
    {
        escape_seq.push_back(
            sequence(character('x'), repeat_exact(2, hexdig(s)))
        );
    }

    return sequence(
            character('\\'),
            std::move(escape_seq)
        );
}

TOML11_INLINE either basic_char(const spec& s)
{
    const auto basic_unescaped = [&s]() {
        return either(
                wschar(s),
                character(0x21),                // 22 is "
                character_in_range(0x23, 0x5B), // 5C is backslash
                character_in_range(0x5D, 0x7E), // 7F is DEL
                non_ascii(s)
            );
    };
    return either(basic_unescaped(), escaped(s));
}

TOML11_INLINE sequence basic_string(const spec& s)
{
    return sequence(
            character('"'),
            repeat_at_least(0, basic_char(s)),
            character('"')
        );
}

// ---------------------------------------------------------------------------
// multiline string

TOML11_INLINE sequence escaped_newline(const spec& s)
{
    return sequence(
            character('\\'), ws(s), newline(s),
            repeat_at_least(0, either(wschar(s), newline(s)))
        );
}

TOML11_INLINE sequence ml_basic_string(const spec& s)
{
    const auto mlb_content = [&s]() {
        return either(basic_char(s), newline(s), escaped_newline(s));
    };
    const auto mlb_quotes = []() {
        return either(literal("\"\""), character('\"'));
    };

    return sequence(
            literal("\"\"\""),
            maybe(newline(s)),
            repeat_at_least(0, mlb_content()),
            repeat_at_least(0,
                sequence(
                    mlb_quotes(),
                    repeat_at_least(1, mlb_content())
                )
            ),
            // XXX """ and mlb_quotes are intentionally reordered to avoid
            //     unexpected match of mlb_quotes
            literal("\"\"\""),
            maybe(mlb_quotes())
        );
}

// ---------------------------------------------------------------------------
// literal string

TOML11_INLINE either literal_char(const spec& s)
{
    return either(
            character         (0x09),
            character_in_range(0x20, 0x26),
            character_in_range(0x28, 0x7E),
            non_ascii(s)
        );
}

TOML11_INLINE sequence literal_string(const spec& s)
{
    return sequence(
            character('\''),
            repeat_at_least(0, literal_char(s)),
            character('\'')
        );
}

TOML11_INLINE sequence ml_literal_string(const spec& s)
{
    const auto mll_quotes = []() {
        return either(literal("''"), character('\''));
    };
    const auto mll_content = [&s]() {
        return either(literal_char(s), newline(s));
    };

    return sequence(
            literal("'''"),
            maybe(newline(s)),
            repeat_at_least(0, mll_content()),
            repeat_at_least(0, sequence(
                    mll_quotes(),
                    repeat_at_least(1, mll_content())
                )
            ),
            literal("'''"),
            maybe(mll_quotes())
            // XXX ''' and mll_quotes are intentionally reordered to avoid
            //     unexpected match of mll_quotes
        );
}

TOML11_INLINE either string(const spec& s)
{
    return either(
            ml_basic_string(s),
            ml_literal_string(s),
            basic_string(s),
            literal_string(s)
        );
}

// ===========================================================================
// Keys

// to keep `expected_chars` simple
TOML11_INLINE non_ascii_key_char::non_ascii_key_char(const spec& s) noexcept
{
    assert(s.v1_1_0_allow_non_english_in_bare_keys);
    (void)s; // for NDEBUG
}

TOML11_INLINE std::uint32_t non_ascii_key_char::read_utf8(location& loc) const
{
    // U+0000   ... U+0079  ; 0xxx_xxxx
    // U+0080   ... U+07FF  ; 110y_yyyx 10xx_xxxx;
    // U+0800   ... U+FFFF  ; 1110_yyyy 10yx_xxxx 10xx_xxxx
    // U+010000 ... U+10FFFF; 1111_0yyy 10yy_xxxx 10xx_xxxx 10xx_xxxx

    const unsigned char b1 = loc.current(); loc.advance(1);
    if(b1 < 0x80)
    {
        return static_cast<std::uint32_t>(b1);
    }
    else if((b1 >> 5) == 6) // 0b110 == 6
    {
        const auto b2 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 5) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t codep = (c1 << 6) + c2;

        if(codep < 0x80)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else if((b1 >> 4) == 14) // 0b1110 == 14
    {
        const auto b2 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b3 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 4) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t c3 = b3 & ((1 << 6) - 1);

        const std::uint32_t codep = (c1 << 12) + (c2 << 6) + c3;
        if(codep < 0x800)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else if((b1 >> 3) == 30) // 0b11110 == 30
    {
        const auto b2 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b3 = loc.current(); loc.advance(1); if(loc.eof()) {return 0xFFFFFFFF;}
        const auto b4 = loc.current(); loc.advance(1);

        const std::uint32_t c1 = b1 & ((1 << 3) - 1);
        const std::uint32_t c2 = b2 & ((1 << 6) - 1);
        const std::uint32_t c3 = b3 & ((1 << 6) - 1);
        const std::uint32_t c4 = b4 & ((1 << 6) - 1);
        const std::uint32_t codep = (c1 << 18) + (c2 << 12) + (c3 << 6) + c4;

        if(codep < 0x10000)
        {
            return 0xFFFFFFFF;
        }
        return codep;
    }
    else // not a Unicode codepoint in UTF-8
    {
        return 0xFFFFFFFF;
    }
}

TOML11_INLINE region non_ascii_key_char::scan(location& loc) const
{
    if(loc.eof()) {return region{};}

    const auto first = loc;

    const auto cp = read_utf8(loc);

    if(cp == 0xFFFFFFFF)
    {
        return region{};
    }

    // ALPHA / DIGIT / %x2D / %x5F    ; a-z A-Z 0-9 - _
    // / %xB2 / %xB3 / %xB9 / %xBC-BE ; superscript digits, fractions
    // / %xC0-D6 / %xD8-F6 / %xF8-37D ; non-symbol chars in Latin block
    // / %x37F-1FFF                   ; exclude GREEK QUESTION MARK, which is basically a semi-colon
    // / %x200C-200D / %x203F-2040    ; from General Punctuation Block, include the two tie symbols and ZWNJ, ZWJ
    // / %x2070-218F / %x2460-24FF    ; include super-/subscripts, letterlike/numberlike forms, enclosed alphanumerics
    // / %x2C00-2FEF / %x3001-D7FF    ; skip arrows, math, box drawing etc, skip 2FF0-3000 ideographic up/down markers and spaces
    // / %xF900-FDCF / %xFDF0-FFFD    ; skip D800-DFFF surrogate block, E000-F8FF Private Use area, FDD0-FDEF intended for process-internal use (unicode)
    // / %x10000-EFFFF                ; all chars outside BMP range, excluding Private Use planes (F0000-10FFFF)

    if(cp == 0xB2 || cp == 0xB3 || cp == 0xB9 || (0xBC <= cp && cp <= 0xBE) ||
       (0xC0    <= cp && cp <= 0xD6  ) || (0xD8 <= cp && cp <= 0xF6) || (0xF8 <= cp && cp <= 0x37D) ||
       (0x37F   <= cp && cp <= 0x1FFF) ||
       (0x200C  <= cp && cp <= 0x200D) || (0x203F <= cp && cp <= 0x2040) ||
       (0x2070  <= cp && cp <= 0x218F) || (0x2460 <= cp && cp <= 0x24FF) ||
       (0x2C00  <= cp && cp <= 0x2FEF) || (0x3001 <= cp && cp <= 0xD7FF) ||
       (0xF900  <= cp && cp <= 0xFDCF) || (0xFDF0 <= cp && cp <= 0xFFFD) ||
       (0x10000 <= cp && cp <= 0xEFFFF) )
    {
        return region(first, loc);
    }
    loc = first;
    return region{};
}

TOML11_INLINE repeat_at_least unquoted_key(const spec& s)
{
    auto keychar = either(
            alpha(s), digit(s), character{0x2D}, character{0x5F}
        );

    if(s.v1_1_0_allow_non_english_in_bare_keys)
    {
        keychar.push_back(non_ascii_key_char(s));
    }

    return repeat_at_least(1, std::move(keychar));
}

TOML11_INLINE either quoted_key(const spec& s)
{
    return either(basic_string(s), literal_string(s));
}

TOML11_INLINE either simple_key(const spec& s)
{
    return either(unquoted_key(s), quoted_key(s));
}

TOML11_INLINE sequence dot_sep(const spec& s)
{
    return sequence(ws(s), character('.'), ws(s));
}

TOML11_INLINE sequence dotted_key(const spec& s)
{
    return sequence(
        simple_key(s),
        repeat_at_least(1, sequence(dot_sep(s), simple_key(s)))
    );
}

TOML11_INLINE key::key(const spec& s) noexcept
    : scanner_(dotted_key(s), simple_key(s))
{}

TOML11_INLINE sequence keyval_sep(const spec& s)
{
    return sequence(ws(s), character('='), ws(s));
}

// ===========================================================================
// Table key

TOML11_INLINE sequence std_table(const spec& s)
{
    return sequence(character('['), ws(s), key(s), ws(s), character(']'));
}

TOML11_INLINE sequence array_table(const spec& s)
{
    return sequence(literal("[["), ws(s), key(s), ws(s), literal("]]"));
}

// ===========================================================================
// extension: null

TOML11_INLINE literal null_value(const spec&)
{
    return literal("null");
}

} // namespace syntax
} // namespace detail
} // namespace toml
#endif // TOML11_SYNTAX_IMPL_HPP
