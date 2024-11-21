#ifndef TOML11_SKIP_HPP
#define TOML11_SKIP_HPP

#include "context.hpp"
#include "region.hpp"
#include "scanner.hpp"
#include "syntax.hpp"
#include "types.hpp"

#include <cassert>

namespace toml
{
namespace detail
{

template<typename TC>
bool skip_whitespace(location& loc, const context<TC>& ctx)
{
    return syntax::ws(ctx.toml_spec()).scan(loc).is_ok();
}

template<typename TC>
bool skip_empty_lines(location& loc, const context<TC>& ctx)
{
    return repeat_at_least(1, sequence(
            syntax::ws(ctx.toml_spec()),
            syntax::newline(ctx.toml_spec())
        )).scan(loc).is_ok();
}

// For error recovery.
//
// In case if a comment line contains an invalid character, we need to skip it
// to advance parsing.
template<typename TC>
void skip_comment_block(location& loc, const context<TC>& ctx)
{
    while( ! loc.eof())
    {
        skip_whitespace(loc, ctx);
        if(loc.current() == '#')
        {
            while( ! loc.eof())
            {
                // both CRLF and LF ends with LF.
                if(loc.current() == '\n')
                {
                    loc.advance();
                    break;
                }
            }
        }
        else if(syntax::newline(ctx.toml_spec()).scan(loc).is_ok())
        {
            ; // an empty line. skip this also
        }
        else
        {
            // the next token is neither a comment nor empty line.
            return ;
        }
    }
    return ;
}

template<typename TC>
void skip_empty_or_comment_lines(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    repeat_at_least(0, sequence(
            syntax::ws(spec),
            maybe(syntax::comment(spec)),
            syntax::newline(spec))
        ).scan(loc);
    return ;
}

// For error recovery.
//
// Sometimes we need to skip a value and find a delimiter, like `,`, `]`, or `}`.
// To find delimiter, we need to skip delimiters in a string.
// Since we are skipping invalid value while error recovery, we don't need
// to check the syntax. Here we just skip string-like region until closing quote
// is found.
template<typename TC>
void skip_string_like(location& loc, const context<TC>&)
{
    // if """ is found, skip until the closing """ is found.
    if(literal("\"\"\"").scan(loc).is_ok())
    {
        while( ! loc.eof())
        {
            if(literal("\"\"\"").scan(loc).is_ok())
            {
                return;
            }
            loc.advance();
        }
    }
    else if(literal("'''").scan(loc).is_ok())
    {
        while( ! loc.eof())
        {
            if(literal("'''").scan(loc).is_ok())
            {
                return;
            }
            loc.advance();
        }
    }
    // if " is found, skip until the closing " or newline is found.
    else if(loc.current() == '"')
    {
        while( ! loc.eof())
        {
            loc.advance();
            if(loc.current() == '"' || loc.current() == '\n')
            {
                loc.advance();
                return;
            }
        }
    }
    else if(loc.current() == '\'')
    {
        while( ! loc.eof())
        {
            loc.advance();
            if(loc.current() == '\'' || loc.current() == '\n')
            {
                loc.advance();
                return ;
            }
        }
    }
    return;
}

template<typename TC>
void skip_value(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_array_like(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_inline_table_like(location& loc, const context<TC>& ctx);
template<typename TC>
void skip_key_value_pair(location& loc, const context<TC>& ctx);

template<typename TC>
result<value_t, error_info>
guess_value_type(const location& loc, const context<TC>& ctx);

template<typename TC>
void skip_array_like(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    assert(loc.current() == '[');
    loc.advance();

    while( ! loc.eof())
    {
        if(loc.current() == '\"' || loc.current() == '\'')
        {
            skip_string_like(loc, ctx);
        }
        else if(loc.current() == '#')
        {
            skip_comment_block(loc, ctx);
        }
        else if(loc.current() == '{')
        {
            skip_inline_table_like(loc, ctx);
        }
        else if(loc.current() == '[')
        {
            const auto checkpoint = loc;
            if(syntax::std_table(spec).scan(loc).is_ok() ||
               syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = checkpoint;
                break;
            }
            // if it is not a table-definition, then it is an array.
            skip_array_like(loc, ctx);
        }
        else if(loc.current() == '=')
        {
            // key-value pair cannot be inside the array.
            // guessing the error is "missing closing bracket `]`".
            // find the previous key just before `=`.
            while(loc.get_location() != 0)
            {
                loc.retrace();
                if(loc.current() == '\n')
                {
                    loc.advance();
                    break;
                }
            }
            break;
        }
        else if(loc.current() == ']')
        {
            break; // found closing bracket
        }
        else
        {
            loc.advance();
        }
    }
    return ;
}

template<typename TC>
void skip_inline_table_like(location& loc, const context<TC>& ctx)
{
    assert(loc.current() == '{');
    loc.advance();

    const auto& spec = ctx.toml_spec();

    while( ! loc.eof())
    {
        if(loc.current() == '\n' && ! spec.v1_1_0_allow_newlines_in_inline_tables)
        {
            break; // missing closing `}`.
        }
        else if(loc.current() == '\"' || loc.current() == '\'')
        {
            skip_string_like(loc, ctx);
        }
        else if(loc.current() == '#')
        {
            skip_comment_block(loc, ctx);
            if( ! spec.v1_1_0_allow_newlines_in_inline_tables)
            {
                // comment must end with newline.
                break; // missing closing `}`.
            }
        }
        else if(loc.current() == '[')
        {
            const auto checkpoint = loc;
            if(syntax::std_table(spec).scan(loc).is_ok() ||
               syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = checkpoint;
                break; // missing closing `}`.
            }
            // if it is not a table-definition, then it is an array.
            skip_array_like(loc, ctx);
        }
        else if(loc.current() == '{')
        {
            skip_inline_table_like(loc, ctx);
        }
        else if(loc.current() == '}')
        {
            // closing brace found. guessing the error is inside the table.
            break;
        }
        else
        {
            // skip otherwise.
            loc.advance();
        }
    }
    return ;
}

template<typename TC>
void skip_value(location& loc, const context<TC>& ctx)
{
    value_t ty = guess_value_type(loc, ctx).unwrap_or(value_t::empty);
    if(ty == value_t::string)
    {
        skip_string_like(loc, ctx);
    }
    else if(ty == value_t::array)
    {
        skip_array_like(loc, ctx);
    }
    else if(ty == value_t::table)
    {
        // In case of multiline tables, it may skip key-value pair but not the
        // whole table.
        skip_inline_table_like(loc, ctx);
    }
    else // others are an "in-line" values. skip until the next line
    {
        while( ! loc.eof())
        {
            if(loc.current() == '\n')
            {
                break;
            }
            else if(loc.current() == ',' || loc.current() == ']' || loc.current() == '}')
            {
                break;
            }
            loc.advance();
        }
    }
    return;
}

template<typename TC>
void skip_key_value_pair(location& loc, const context<TC>& ctx)
{
    while( ! loc.eof())
    {
        if(loc.current() == '=')
        {
            skip_whitespace(loc, ctx);
            skip_value(loc, ctx);
            return;
        }
        else if(loc.current() == '\n')
        {
            // newline is found before finding `=`. assuming "missing `=`".
            return;
        }
        loc.advance();
    }
    return ;
}

template<typename TC>
void skip_until_next_table(location& loc, const context<TC>& ctx)
{
    const auto& spec = ctx.toml_spec();
    while( ! loc.eof())
    {
        if(loc.current() == '\n')
        {
            loc.advance();
            const auto line_begin = loc;

            skip_whitespace(loc, ctx);
            if(syntax::std_table(spec).scan(loc).is_ok())
            {
                loc = line_begin;
                return ;
            }
            if(syntax::array_table(spec).scan(loc).is_ok())
            {
                loc = line_begin;
                return ;
            }
        }
        loc.advance();
    }
}

} // namespace detail
} // namespace toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
struct type_config;
struct ordered_type_config;

namespace detail
{
extern template bool skip_whitespace            <type_config>(location& loc, const context<type_config>&);
extern template bool skip_empty_lines           <type_config>(location& loc, const context<type_config>&);
extern template void skip_comment_block         <type_config>(location& loc, const context<type_config>&);
extern template void skip_empty_or_comment_lines<type_config>(location& loc, const context<type_config>&);
extern template void skip_string_like           <type_config>(location& loc, const context<type_config>&);
extern template void skip_array_like            <type_config>(location& loc, const context<type_config>&);
extern template void skip_inline_table_like     <type_config>(location& loc, const context<type_config>&);
extern template void skip_value                 <type_config>(location& loc, const context<type_config>&);
extern template void skip_key_value_pair        <type_config>(location& loc, const context<type_config>&);
extern template void skip_until_next_table      <type_config>(location& loc, const context<type_config>&);

extern template bool skip_whitespace            <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template bool skip_empty_lines           <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_comment_block         <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_empty_or_comment_lines<ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_string_like           <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_array_like            <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_inline_table_like     <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_value                 <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_key_value_pair        <ordered_type_config>(location& loc, const context<ordered_type_config>&);
extern template void skip_until_next_table      <ordered_type_config>(location& loc, const context<ordered_type_config>&);

} // detail
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_SKIP_HPP
