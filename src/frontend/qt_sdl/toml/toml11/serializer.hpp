#ifndef TOML11_SERIALIZER_HPP
#define TOML11_SERIALIZER_HPP

#include "comments.hpp"
#include "error_info.hpp"
#include "exception.hpp"
#include "source_location.hpp"
#include "spec.hpp"
#include "syntax.hpp"
#include "types.hpp"
#include "utility.hpp"
#include "value.hpp"

#include <iomanip>
#include <iterator>
#include <sstream>

#include <cmath>
#include <cstdio>

namespace toml
{

struct serialization_error final : public ::toml::exception
{
  public:
    explicit serialization_error(std::string what_arg, source_location loc)
        : what_(std::move(what_arg)), loc_(std::move(loc))
    {}
    ~serialization_error() noexcept override = default;

    const char* what() const noexcept override {return what_.c_str();}
    source_location const& location() const noexcept {return loc_;}

  private:
    std::string what_;
    source_location loc_;
};

namespace detail
{
template<typename TC>
class serializer
{
  public:

    using value_type           = basic_value<TC>;

    using key_type             = typename value_type::key_type            ;
    using comment_type         = typename value_type::comment_type        ;
    using boolean_type         = typename value_type::boolean_type        ;
    using integer_type         = typename value_type::integer_type        ;
    using floating_type        = typename value_type::floating_type       ;
    using string_type          = typename value_type::string_type         ;
    using local_time_type      = typename value_type::local_time_type     ;
    using local_date_type      = typename value_type::local_date_type     ;
    using local_datetime_type  = typename value_type::local_datetime_type ;
    using offset_datetime_type = typename value_type::offset_datetime_type;
    using array_type           = typename value_type::array_type          ;
    using table_type           = typename value_type::table_type          ;

    using char_type            = typename string_type::value_type;

  public:

    explicit serializer(const spec& sp)
        : spec_(sp), force_inline_(false), current_indent_(0)
    {}

    string_type operator()(const std::vector<key_type>& ks, const value_type& v)
    {
        for(const auto& k : ks)
        {
            this->keys_.push_back(k);
        }
        return (*this)(v);
    }

    string_type operator()(const key_type& k, const value_type& v)
    {
        this->keys_.push_back(k);
        return (*this)(v);
    }

    string_type operator()(const value_type& v)
    {
        switch(v.type())
        {
            case value_t::boolean        : {return (*this)(v.as_boolean        (), v.as_boolean_fmt        (), v.location());}
            case value_t::integer        : {return (*this)(v.as_integer        (), v.as_integer_fmt        (), v.location());}
            case value_t::floating       : {return (*this)(v.as_floating       (), v.as_floating_fmt       (), v.location());}
            case value_t::string         : {return (*this)(v.as_string         (), v.as_string_fmt         (), v.location());}
            case value_t::offset_datetime: {return (*this)(v.as_offset_datetime(), v.as_offset_datetime_fmt(), v.location());}
            case value_t::local_datetime : {return (*this)(v.as_local_datetime (), v.as_local_datetime_fmt (), v.location());}
            case value_t::local_date     : {return (*this)(v.as_local_date     (), v.as_local_date_fmt     (), v.location());}
            case value_t::local_time     : {return (*this)(v.as_local_time     (), v.as_local_time_fmt     (), v.location());}
            case value_t::array          :
            {
                return (*this)(v.as_array(), v.as_array_fmt(), v.comments(), v.location());
            }
            case value_t::table          :
            {
                string_type retval;
                if(this->keys_.empty()) // it might be the root table. emit comments here.
                {
                    retval += format_comments(v.comments(), v.as_table_fmt().indent_type);
                }
                if( ! retval.empty()) // we have comment.
                {
                    retval += char_type('\n');
                }

                retval += (*this)(v.as_table(), v.as_table_fmt(), v.comments(), v.location());
                return retval;
            }
            case value_t::empty:
            {
                if(this->spec_.ext_null_value)
                {
                    return string_conv<string_type>("null");
                }
                break;
            }
            default:
            {
                break;
            }
        }
        throw serialization_error(format_error(
            "[error] toml::serializer: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
    }

  private:

    string_type operator()(const boolean_type& b, const boolean_format_info&, const source_location&) // {{{
    {
        if(b)
        {
            return string_conv<string_type>("true");
        }
        else
        {
            return string_conv<string_type>("false");
        }
    } // }}}

    string_type operator()(const integer_type i, const integer_format_info& fmt, const source_location& loc) // {{{
    {
        std::ostringstream oss;
        this->set_locale(oss);

        const auto insert_spacer = [&fmt](std::string s) -> std::string {
            if(fmt.spacer == 0) {return s;}

            std::string sign;
            if( ! s.empty() && (s.at(0) == '+' || s.at(0) == '-'))
            {
                sign += s.at(0);
                s.erase(s.begin());
            }

            std::string spaced;
            std::size_t counter = 0;
            for(auto iter = s.rbegin(); iter != s.rend(); ++iter)
            {
                if(counter != 0 && counter % fmt.spacer == 0)
                {
                    spaced += '_';
                }
                spaced += *iter;
                counter += 1;
            }
            if(!spaced.empty() && spaced.back() == '_') {spaced.pop_back();}

            s.clear();
            std::copy(spaced.rbegin(), spaced.rend(), std::back_inserter(s));
            return sign + s;
        };

        std::string retval;
        if(fmt.fmt == integer_format::dec)
        {
            oss << std::setw(static_cast<int>(fmt.width)) << std::dec << i;
            retval = insert_spacer(oss.str());

            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                retval += '_';
                retval += fmt.suffix;
            }
        }
        else
        {
            if(i < 0)
            {
                throw serialization_error(format_error("binary, octal, hexadecimal "
                    "integer does not allow negative value", loc, "here"), loc);
            }
            switch(fmt.fmt)
            {
                case integer_format::hex:
                {
                    oss << std::noshowbase
                        << std::setw(static_cast<int>(fmt.width))
                        << std::setfill('0')
                        << std::hex;
                    if(fmt.uppercase)
                    {
                        oss << std::uppercase;
                    }
                    else
                    {
                        oss << std::nouppercase;
                    }
                    oss << i;
                    retval = std::string("0x") + insert_spacer(oss.str());
                    break;
                }
                case integer_format::oct:
                {
                    oss << std::setw(static_cast<int>(fmt.width)) << std::setfill('0') << std::oct << i;
                    retval = std::string("0o") + insert_spacer(oss.str());
                    break;
                }
                case integer_format::bin:
                {
                    integer_type x{i};
                    std::string tmp;
                    std::size_t bits(0);
                    while(x != 0)
                    {
                        if(fmt.spacer != 0)
                        {
                            if(bits != 0 && (bits % fmt.spacer) == 0) {tmp += '_';}
                        }
                        if(x % 2 == 1) { tmp += '1'; } else { tmp += '0'; }
                        x >>= 1;
                        bits += 1;
                    }
                    for(; bits < fmt.width; ++bits)
                    {
                        if(fmt.spacer != 0)
                        {
                            if(bits != 0 && (bits % fmt.spacer) == 0) {tmp += '_';}
                        }
                        tmp += '0';
                    }
                    for(auto iter = tmp.rbegin(); iter != tmp.rend(); ++iter)
                    {
                        oss << *iter;
                    }
                    retval = std::string("0b") + oss.str();
                    break;
                }
                default:
                {
                    throw serialization_error(format_error(
                        "none of dec, hex, oct, bin: " + to_string(fmt.fmt),
                        loc, "here"), loc);
                }
            }
        }
        return string_conv<string_type>(retval);
    } // }}}

    string_type operator()(const floating_type f, const floating_format_info& fmt, const source_location&) // {{{
    {
        using std::isnan;
        using std::isinf;
        using std::signbit;

        std::ostringstream oss;
        this->set_locale(oss);

        if(isnan(f))
        {
            if(signbit(f))
            {
                oss << '-';
            }
            oss << "nan";
            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                oss << '_';
                oss << fmt.suffix;
            }
            return string_conv<string_type>(oss.str());
        }

        if(isinf(f))
        {
            if(signbit(f))
            {
                oss << '-';
            }
            oss << "inf";
            if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
            {
                oss << '_';
                oss << fmt.suffix;
            }
            return string_conv<string_type>(oss.str());
        }

        switch(fmt.fmt)
        {
            case floating_format::defaultfloat:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << f;
                // since defaultfloat may omit point, we need to add it
                std::string s = oss.str();
                if (s.find('.') == std::string::npos &&
                    s.find('e') == std::string::npos &&
                    s.find('E') == std::string::npos )
                {
                    s += ".0";
                }
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    s += '_';
                    s += fmt.suffix;
                }
                return string_conv<string_type>(s);
            }
            case floating_format::fixed:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << std::fixed << f;
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
            case floating_format::scientific:
            {
                if(fmt.prec != 0)
                {
                    oss << std::setprecision(static_cast<int>(fmt.prec));
                }
                oss << std::scientific << f;
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
            case floating_format::hex:
            {
                if(this->spec_.ext_hex_float)
                {
                    oss << std::hexfloat << f;
                    // suffix is only for decimal numbers.
                    return string_conv<string_type>(oss.str());
                }
                else // no hex allowed. output with max precision.
                {
                    oss << std::setprecision(std::numeric_limits<floating_type>::max_digits10)
                        << std::scientific << f;
                    // suffix is only for decimal numbers.
                    return string_conv<string_type>(oss.str());
                }
            }
            default:
            {
                if(this->spec_.ext_num_suffix && ! fmt.suffix.empty())
                {
                    oss << '_' << fmt.suffix;
                }
                return string_conv<string_type>(oss.str());
            }
        }
    } // }}}

    string_type operator()(string_type s, const string_format_info& fmt, const source_location& loc) // {{{
    {
        string_type retval;
        switch(fmt.fmt)
        {
            case string_format::basic:
            {
                retval += char_type('"');
                retval += this->escape_basic_string(s);
                retval += char_type('"');
                return retval;
            }
            case string_format::literal:
            {
                if(std::find(s.begin(), s.end(), char_type('\n')) != s.end())
                {
                    throw serialization_error(format_error("toml::serializer: "
                        "(non-multiline) literal string cannot have a newline",
                        loc, "here"), loc);
                }
                retval += char_type('\'');
                retval += s;
                retval += char_type('\'');
                return retval;
            }
            case string_format::multiline_basic:
            {
                retval += string_conv<string_type>("\"\"\"");
                if(fmt.start_with_newline)
                {
                    retval += char_type('\n');
                }

                retval += this->escape_ml_basic_string(s);

                retval += string_conv<string_type>("\"\"\"");
                return retval;
            }
            case string_format::multiline_literal:
            {
                retval += string_conv<string_type>("'''");
                if(fmt.start_with_newline)
                {
                    retval += char_type('\n');
                }
                retval += s;
                retval += string_conv<string_type>("'''");
                return retval;
            }
            default:
            {
                throw serialization_error(format_error(
                    "[error] toml::serializer::operator()(string): "
                    "invalid string_format value", loc, "here"), loc);
            }
        }
    } // }}}

    string_type operator()(const local_date_type& d, const local_date_format_info&, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << d;
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type operator()(const local_time_type& t, const local_time_format_info& fmt, const source_location&) // {{{
    {
        return this->format_local_time(t, fmt.has_seconds, fmt.subsecond_precision);
    } // }}}

    string_type operator()(const local_datetime_type& dt, const local_datetime_format_info& fmt, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << dt.date;
        switch(fmt.delimiter)
        {
            case datetime_delimiter_kind::upper_T: { oss << 'T'; break; }
            case datetime_delimiter_kind::lower_t: { oss << 't'; break; }
            case datetime_delimiter_kind::space:   { oss << ' '; break; }
            default:                               { oss << 'T'; break; }
        }
        return string_conv<string_type>(oss.str()) +
            this->format_local_time(dt.time, fmt.has_seconds, fmt.subsecond_precision);
    } // }}}

    string_type operator()(const offset_datetime_type& odt, const offset_datetime_format_info& fmt, const source_location&) // {{{
    {
        std::ostringstream oss;
        oss << odt.date;
        switch(fmt.delimiter)
        {
            case datetime_delimiter_kind::upper_T: { oss << 'T'; break; }
            case datetime_delimiter_kind::lower_t: { oss << 't'; break; }
            case datetime_delimiter_kind::space:   { oss << ' '; break; }
            default:                               { oss << 'T'; break; }
        }
        oss << string_conv<std::string>(this->format_local_time(odt.time, fmt.has_seconds, fmt.subsecond_precision));
        oss << odt.offset;
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type operator()(const array_type& a, const array_format_info& fmt, const comment_type& com, const source_location& loc) // {{{
    {
        array_format f = fmt.fmt;
        if(fmt.fmt == array_format::default_format)
        {
            // [[in.this.form]], you cannot add a comment to the array itself
            // (but you can add a comment to each table).
            // To keep comments, we need to avoid multiline array-of-tables
            // if array itself has a comment.
            if( ! this->keys_.empty() &&
                ! a.empty() &&
                com.empty() &&
                std::all_of(a.begin(), a.end(), [](const value_type& e) {return e.is_table();}))
            {
                f = array_format::array_of_tables;
            }
            else
            {
                f = array_format::oneline;

                // check if it becomes long
                std::size_t approx_len = 0;
                for(const auto& e : a)
                {
                    // have a comment. cannot be inlined
                    if( ! e.comments().empty())
                    {
                        f = array_format::multiline;
                        break;
                    }
                    // possibly long types ...
                    if(e.is_array() || e.is_table() || e.is_offset_datetime() || e.is_local_datetime())
                    {
                        f = array_format::multiline;
                        break;
                    }
                    else if(e.is_boolean())
                    {
                        approx_len += (*this)(e.as_boolean(), e.as_boolean_fmt(), e.location()).size();
                    }
                    else if(e.is_integer())
                    {
                        approx_len += (*this)(e.as_integer(), e.as_integer_fmt(), e.location()).size();
                    }
                    else if(e.is_floating())
                    {
                        approx_len += (*this)(e.as_floating(), e.as_floating_fmt(), e.location()).size();
                    }
                    else if(e.is_string())
                    {
                        if(e.as_string_fmt().fmt == string_format::multiline_basic ||
                           e.as_string_fmt().fmt == string_format::multiline_literal)
                        {
                            f = array_format::multiline;
                            break;
                        }
                        approx_len += 2 + (*this)(e.as_string(), e.as_string_fmt(), e.location()).size();
                    }
                    else if(e.is_local_date())
                    {
                        approx_len += 10; // 1234-56-78
                    }
                    else if(e.is_local_time())
                    {
                        approx_len += 15; // 12:34:56.789012
                    }

                    if(approx_len > 60) // key, ` = `, `[...]` < 80
                    {
                        f = array_format::multiline;
                        break;
                    }
                    approx_len += 2; // `, `
                }
            }
        }
        if(this->force_inline_ && f == array_format::array_of_tables)
        {
            f = array_format::multiline;
        }
        if(a.empty() && f == array_format::array_of_tables)
        {
            f = array_format::oneline;
        }

        // --------------------------------------------------------------------

        if(f == array_format::array_of_tables)
        {
            if(this->keys_.empty())
            {
                throw serialization_error("array of table must have its key. "
                        "use format(key, v)", loc);
            }
            string_type retval;
            for(const auto& e : a)
            {
                assert(e.is_table());

                this->current_indent_ += e.as_table_fmt().name_indent;
                retval += this->format_comments(e.comments(), e.as_table_fmt().indent_type);
                retval += this->format_indent(e.as_table_fmt().indent_type);
                this->current_indent_ -= e.as_table_fmt().name_indent;

                retval += string_conv<string_type>("[[");
                retval += this->format_keys(this->keys_).value();
                retval += string_conv<string_type>("]]\n");

                retval += this->format_ml_table(e.as_table(), e.as_table_fmt());
            }
            return retval;
        }
        else if(f == array_format::oneline)
        {
            // ignore comments. we cannot emit comments
            string_type retval;
            retval += char_type('[');
            for(const auto& e : a)
            {
                this->force_inline_ = true;
                retval += (*this)(e);
                retval += string_conv<string_type>(", ");
            }
            if( ! a.empty())
            {
                retval.pop_back(); // ` `
                retval.pop_back(); // `,`
            }
            retval += char_type(']');
            this->force_inline_ = false;
            return retval;
        }
        else
        {
            assert(f == array_format::multiline);

            string_type retval;
            retval += string_conv<string_type>("[\n");

            for(const auto& e : a)
            {
                this->current_indent_ += fmt.body_indent;
                retval += this->format_comments(e.comments(), fmt.indent_type);
                retval += this->format_indent(fmt.indent_type);
                this->current_indent_ -= fmt.body_indent;

                this->force_inline_ = true;
                retval += (*this)(e);
                retval += string_conv<string_type>(",\n");
            }
            this->force_inline_ = false;

            this->current_indent_ += fmt.closing_indent;
            retval += this->format_indent(fmt.indent_type);
            this->current_indent_ -= fmt.closing_indent;

            retval += char_type(']');
            return retval;
        }
    } // }}}

    string_type operator()(const table_type& t, const table_format_info& fmt, const comment_type& com, const source_location& loc) // {{{
    {
        if(this->force_inline_)
        {
            if(fmt.fmt == table_format::multiline_oneline)
            {
                return this->format_ml_inline_table(t, fmt);
            }
            else
            {
                return this->format_inline_table(t, fmt);
            }
        }
        else
        {
            if(fmt.fmt == table_format::multiline)
            {
                string_type retval;
                // comment is emitted inside format_ml_table
                if(auto k = this->format_keys(this->keys_))
                {
                    this->current_indent_ += fmt.name_indent;
                    retval += this->format_comments(com, fmt.indent_type);
                    retval += this->format_indent(fmt.indent_type);
                    this->current_indent_ -= fmt.name_indent;
                    retval += char_type('[');
                    retval += k.value();
                    retval += string_conv<string_type>("]\n");
                }
                // otherwise, its the root.

                retval += this->format_ml_table(t, fmt);
                return retval;
            }
            else if(fmt.fmt == table_format::oneline)
            {
                return this->format_inline_table(t, fmt);
            }
            else if(fmt.fmt == table_format::multiline_oneline)
            {
                return this->format_ml_inline_table(t, fmt);
            }
            else if(fmt.fmt == table_format::dotted)
            {
                std::vector<string_type> keys;
                if(this->keys_.empty())
                {
                    throw serialization_error(format_error("toml::serializer: "
                        "dotted table must have its key. use format(key, v)",
                        loc, "here"), loc);
                }
                keys.push_back(this->keys_.back());

                const auto retval = this->format_dotted_table(t, fmt, loc, keys);
                keys.pop_back();
                return retval;
            }
            else
            {
                assert(fmt.fmt == table_format::implicit);

                string_type retval;
                for(const auto& kv : t)
                {
                    const auto& k = kv.first;
                    const auto& v = kv.second;

                    if( ! v.is_table() && ! v.is_array_of_tables())
                    {
                        throw serialization_error(format_error("toml::serializer: "
                            "an implicit table cannot have non-table value.",
                            v.location(), "here"), v.location());
                    }
                    if(v.is_table())
                    {
                        if(v.as_table_fmt().fmt != table_format::multiline &&
                           v.as_table_fmt().fmt != table_format::implicit)
                        {
                            throw serialization_error(format_error("toml::serializer: "
                                "an implicit table cannot have non-multiline table",
                                v.location(), "here"), v.location());
                        }
                    }
                    else
                    {
                        assert(v.is_array());
                        for(const auto& e : v.as_array())
                        {
                            if(e.as_table_fmt().fmt != table_format::multiline &&
                               v.as_table_fmt().fmt != table_format::implicit)
                            {
                                throw serialization_error(format_error("toml::serializer: "
                                    "an implicit table cannot have non-multiline table",
                                    e.location(), "here"), e.location());
                            }
                        }
                    }

                    keys_.push_back(k);
                    retval += (*this)(v);
                    keys_.pop_back();
                }
                return retval;
            }
        }
    } // }}}

  private:

    string_type escape_basic_string(const string_type& s) const // {{{
    {
        string_type retval;
        for(const char_type c : s)
        {
            switch(c)
            {
                case char_type('\\'): {retval += string_conv<string_type>("\\\\"); break;}
                case char_type('\"'): {retval += string_conv<string_type>("\\\""); break;}
                case char_type('\b'): {retval += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {retval += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {retval += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {retval += string_conv<string_type>("\\n" ); break;}
                case char_type('\r'): {retval += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    if(c == char_type(0x1B) && spec_.v1_1_0_add_escape_sequence_e)
                    {
                        retval += string_conv<string_type>("\\e");
                    }
                    else if((char_type(0x00) <= c && c <= char_type(0x08)) ||
                            (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                             c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            retval += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            retval += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        retval += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            retval += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            retval += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        retval += c;
                    }
                }
            }
        }
        return retval;
    } // }}}

    string_type escape_ml_basic_string(const string_type& s) // {{{
    {
        string_type retval;
        for(const char_type c : s)
        {
            switch(c)
            {
                case char_type('\\'): {retval += string_conv<string_type>("\\\\"); break;}
                case char_type('\b'): {retval += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {retval += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {retval += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {retval += string_conv<string_type>("\n"  ); break;}
                case char_type('\r'): {retval += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    if(c == char_type(0x1B) && spec_.v1_1_0_add_escape_sequence_e)
                    {
                        retval += string_conv<string_type>("\\e");
                    }
                    else if((char_type(0x00) <= c && c <= char_type(0x08)) ||
                            (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                            c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            retval += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            retval += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        retval += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            retval += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            retval += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        retval += c;
                    }
                }
            }
        }
        // Only 1 or 2 consecutive `"`s are allowed in multiline basic string.
        // 3 consecutive `"`s are considered as a closing delimiter.
        // We need to check if there are 3 or more consecutive `"`s and insert
        // backslash to break them down into several short `"`s like the `str6`
        // in the following example.
        // ```toml
        // str4 = """Here are two quotation marks: "". Simple enough."""
        // # str5 = """Here are three quotation marks: """."""  # INVALID
        // str5 = """Here are three quotation marks: ""\"."""
        // str6 = """Here are fifteen quotation marks: ""\"""\"""\"""\"""\"."""
        // ```
        auto found_3_quotes = retval.find(string_conv<string_type>("\"\"\""));
        while(found_3_quotes != string_type::npos)
        {
            retval.replace(found_3_quotes, 3, string_conv<string_type>("\"\"\\\""));
            found_3_quotes = retval.find(string_conv<string_type>("\"\"\""));
        }
        return retval;
    } // }}}

    string_type format_local_time(const local_time_type& t, const bool has_seconds, const std::size_t subsec_prec) // {{{
    {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.hour);
        oss << ':';
        oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.minute);
        if(has_seconds)
        {
            oss << ':';
            oss << std::setfill('0') << std::setw(2) << static_cast<int>(t.second);
            if(subsec_prec != 0)
            {
                std::ostringstream subsec;
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.millisecond);
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.microsecond);
                subsec << std::setfill('0') << std::setw(3) << static_cast<int>(t.nanosecond);
                std::string subsec_str = subsec.str();
                oss << '.' << subsec_str.substr(0, subsec_prec);
            }
        }
        return string_conv<string_type>(oss.str());
    } // }}}

    string_type format_ml_table(const table_type& t, const table_format_info& fmt) // {{{
    {
        const auto format_later = [](const value_type& v) -> bool {

            const bool is_ml_table = v.is_table() &&
                v.as_table_fmt().fmt != table_format::oneline           &&
                v.as_table_fmt().fmt != table_format::multiline_oneline &&
                v.as_table_fmt().fmt != table_format::dotted ;

            const bool is_ml_array_table = v.is_array_of_tables() &&
                v.as_array_fmt().fmt != array_format::oneline &&
                v.as_array_fmt().fmt != array_format::multiline;

            return is_ml_table || is_ml_array_table;
        };

        string_type retval;
        this->current_indent_ += fmt.body_indent;
        for(const auto& kv : t)
        {
            const auto& key = kv.first;
            const auto& val = kv.second;
            if(format_later(val))
            {
                continue;
            }
            this->keys_.push_back(key);

            retval += format_comments(val.comments(), fmt.indent_type);
            retval += format_indent(fmt.indent_type);
            if(val.is_table() && val.as_table_fmt().fmt == table_format::dotted)
            {
                retval += (*this)(val);
            }
            else
            {
                retval += format_key(key);
                retval += string_conv<string_type>(" = ");
                retval += (*this)(val);
                retval += char_type('\n');
            }
            this->keys_.pop_back();
        }
        this->current_indent_ -= fmt.body_indent;

        if( ! retval.empty())
        {
            retval += char_type('\n'); // for readability, add empty line between tables
        }
        for(const auto& kv : t)
        {
            if( ! format_later(kv.second))
            {
                continue;
            }
            // must be a [multiline.table] or [[multiline.array.of.tables]].
            // comments will be generated inside it.
            this->keys_.push_back(kv.first);
            retval += (*this)(kv.second);
            this->keys_.pop_back();
        }
        return retval;
    } // }}}

    string_type format_inline_table(const table_type& t, const table_format_info&) // {{{
    {
        // comments are ignored because we cannot write without newline
        string_type retval;
        retval += char_type('{');
        for(const auto& kv : t)
        {
            this->force_inline_ = true;
            retval += this->format_key(kv.first);
            retval += string_conv<string_type>(" = ");
            retval += (*this)(kv.second);
            retval += string_conv<string_type>(", ");
        }
        if( ! t.empty())
        {
            retval.pop_back(); // ' '
            retval.pop_back(); // ','
        }
        retval += char_type('}');
        this->force_inline_ = false;
        return retval;
    } // }}}

    string_type format_ml_inline_table(const table_type& t, const table_format_info& fmt) // {{{
    {
        string_type retval;
        retval += string_conv<string_type>("{\n");
        this->current_indent_ += fmt.body_indent;
        for(const auto& kv : t)
        {
            this->force_inline_ = true;
            retval += format_comments(kv.second.comments(), fmt.indent_type);
            retval += format_indent(fmt.indent_type);
            retval += kv.first;
            retval += string_conv<string_type>(" = ");

            this->force_inline_ = true;
            retval += (*this)(kv.second);

            retval += string_conv<string_type>(",\n");
        }
        if( ! t.empty())
        {
            retval.pop_back(); // '\n'
            retval.pop_back(); // ','
        }
        this->current_indent_ -= fmt.body_indent;
        this->force_inline_ = false;

        this->current_indent_ += fmt.closing_indent;
        retval += format_indent(fmt.indent_type);
        this->current_indent_ -= fmt.closing_indent;

        retval += char_type('}');
        return retval;
    } // }}}

    string_type format_dotted_table(const table_type& t, const table_format_info& fmt, // {{{
            const source_location&, std::vector<string_type>& keys)
    {
        // lets say we have: `{"a": {"b": {"c": {"d": "foo", "e": "bar"} } }`
        // and `a` and `b` are `dotted`.
        //
        // - in case if `c` is `oneline`:
        // ```toml
        // a.b.c = {d = "foo", e = "bar"}
        // ```
        //
        // - in case if and `c` is `dotted`:
        // ```toml
        // a.b.c.d = "foo"
        // a.b.c.e = "bar"
        // ```

        string_type retval;

        for(const auto& kv : t)
        {
            const auto& key = kv.first;
            const auto& val = kv.second;

            keys.push_back(key);

            // format recursive dotted table?
            if (val.is_table() &&
                val.as_table_fmt().fmt != table_format::oneline &&
                val.as_table_fmt().fmt != table_format::multiline_oneline)
            {
                retval += this->format_dotted_table(val.as_table(), val.as_table_fmt(), val.location(), keys);
            }
            else // non-table or inline tables. format normally
            {
                retval += format_comments(val.comments(), fmt.indent_type);
                retval += format_indent(fmt.indent_type);
                retval += format_keys(keys).value();
                retval += string_conv<string_type>(" = ");
                this->force_inline_ = true; // sub-table must be inlined
                retval += (*this)(val);
                retval += char_type('\n');
                this->force_inline_ = false;
            }
            keys.pop_back();
        }
        return retval;
    } // }}}

    string_type format_key(const key_type& key) // {{{
    {
        if(key.empty())
        {
            return string_conv<string_type>("\"\"");
        }

        // check the key can be a bare (unquoted) key
        auto loc = detail::make_temporary_location(string_conv<std::string>(key));
        auto reg = detail::syntax::unquoted_key(this->spec_).scan(loc);
        if(reg.is_ok() && loc.eof())
        {
            return key;
        }

        //if it includes special characters, then format it in a "quoted" key.
        string_type formatted = string_conv<string_type>("\"");
        for(const char_type c : key)
        {
            switch(c)
            {
                case char_type('\\'): {formatted += string_conv<string_type>("\\\\"); break;}
                case char_type('\"'): {formatted += string_conv<string_type>("\\\""); break;}
                case char_type('\b'): {formatted += string_conv<string_type>("\\b" ); break;}
                case char_type('\t'): {formatted += string_conv<string_type>("\\t" ); break;}
                case char_type('\f'): {formatted += string_conv<string_type>("\\f" ); break;}
                case char_type('\n'): {formatted += string_conv<string_type>("\\n" ); break;}
                case char_type('\r'): {formatted += string_conv<string_type>("\\r" ); break;}
                default  :
                {
                    // ASCII ctrl char
                    if( (char_type(0x00) <= c && c <= char_type(0x08)) ||
                        (char_type(0x0A) <= c && c <= char_type(0x1F)) ||
                        c == char_type(0x7F))
                    {
                        if(spec_.v1_1_0_add_escape_sequence_x)
                        {
                            formatted += string_conv<string_type>("\\x");
                        }
                        else
                        {
                            formatted += string_conv<string_type>("\\u00");
                        }
                        const auto c1 = c / 16;
                        const auto c2 = c % 16;
                        formatted += static_cast<char_type>('0' + c1);
                        if(c2 < 10)
                        {
                            formatted += static_cast<char_type>('0' + c2);
                        }
                        else // 10 <= c2
                        {
                            formatted += static_cast<char_type>('A' + (c2 - 10));
                        }
                    }
                    else
                    {
                        formatted += c;
                    }
                    break;
                }
            }
        }
        formatted += string_conv<string_type>("\"");
        return formatted;
    } // }}}
    cxx::optional<string_type> format_keys(const std::vector<key_type>& keys) // {{{
    {
        if(keys.empty())
        {
            return cxx::make_nullopt();
        }

        string_type formatted;
        for(const auto& ky : keys)
        {
            formatted += format_key(ky);
            formatted += char_type('.');
        }
        formatted.pop_back(); // remove the last dot '.'
        return formatted;
    } // }}}

    string_type format_comments(const discard_comments&, const indent_char) const // {{{
    {
        return string_conv<string_type>("");
    } // }}}
    string_type format_comments(const preserve_comments& comments, const indent_char indent_type) const // {{{
    {
        string_type retval;
        for(const auto& c : comments)
        {
            if(c.empty()) {continue;}
            retval += format_indent(indent_type);
            if(c.front() != '#') {retval += char_type('#');}
            retval += string_conv<string_type>(c);
            if(c.back() != '\n') {retval += char_type('\n');}
        }
        return retval;
    } // }}}

    string_type format_indent(const indent_char indent_type) const // {{{
    {
        const auto indent = static_cast<std::size_t>((std::max)(0, this->current_indent_));
        if(indent_type == indent_char::space)
        {
            return string_conv<string_type>(make_string(indent, ' '));
        }
        else if(indent_type == indent_char::tab)
        {
            return string_conv<string_type>(make_string(indent, '\t'));
        }
        else
        {
            return string_type{};
        }
    } // }}}

    std::locale set_locale(std::ostream& os) const
    {
        return os.imbue(std::locale::classic());
    }

  private:

    spec spec_;
    bool force_inline_; // table inside an array without fmt specification
    std::int32_t current_indent_;
    std::vector<key_type> keys_;
};
} // detail

template<typename TC>
typename basic_value<TC>::string_type
format(const basic_value<TC>& v, const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(v);
}
template<typename TC>
typename basic_value<TC>::string_type
format(const typename basic_value<TC>::key_type& k,
       const basic_value<TC>& v,
       const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(k, v);
}
template<typename TC>
typename basic_value<TC>::string_type
format(const std::vector<typename basic_value<TC>::key_type>& ks,
       const basic_value<TC>& v,
       const spec s = spec::default_version())
{
    detail::serializer<TC> ser(s);
    return ser(ks, v);
}

template<typename TC>
std::ostream& operator<<(std::ostream& os, const basic_value<TC>& v)
{
    os << format(v);
    return os;
}

} // toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
struct type_config;
struct ordered_type_config;

extern template typename basic_value<type_config>::string_type
format<type_config>(const basic_value<type_config>&, const spec);

extern template typename basic_value<type_config>::string_type
format<type_config>(const typename basic_value<type_config>::key_type& k,
                    const basic_value<type_config>& v, const spec);

extern template typename basic_value<type_config>::string_type
format<type_config>(const std::vector<typename basic_value<type_config>::key_type>& ks,
                    const basic_value<type_config>& v, const spec s);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const basic_value<ordered_type_config>&, const spec);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const typename basic_value<ordered_type_config>::key_type& k,
                            const basic_value<ordered_type_config>& v, const spec);

extern template typename basic_value<type_config>::string_type
format<ordered_type_config>(const std::vector<typename basic_value<ordered_type_config>::key_type>& ks,
                            const basic_value<ordered_type_config>& v, const spec s);

namespace detail
{
extern template class serializer<::toml::type_config>;
extern template class serializer<::toml::ordered_type_config>;
} // detail
} // toml
#endif // TOML11_COMPILE_SOURCES


#endif // TOML11_SERIALIZER_HPP
