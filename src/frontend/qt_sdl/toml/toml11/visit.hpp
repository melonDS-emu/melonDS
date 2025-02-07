#ifndef TOML11_VISIT_HPP
#define TOML11_VISIT_HPP

#include "exception.hpp"
#include "traits.hpp"
#include "value.hpp"

namespace toml
{

template<typename Visitor, typename TC>
cxx::return_type_of_t<Visitor, const typename basic_value<TC>::boolean_type&>
visit(Visitor&& visitor, const basic_value<TC>& v)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visitor(v.as_boolean        ());}
        case value_t::integer        : {return visitor(v.as_integer        ());}
        case value_t::floating       : {return visitor(v.as_floating       ());}
        case value_t::string         : {return visitor(v.as_string         ());}
        case value_t::offset_datetime: {return visitor(v.as_offset_datetime());}
        case value_t::local_datetime : {return visitor(v.as_local_datetime ());}
        case value_t::local_date     : {return visitor(v.as_local_date     ());}
        case value_t::local_time     : {return visitor(v.as_local_time     ());}
        case value_t::array          : {return visitor(v.as_array          ());}
        case value_t::table          : {return visitor(v.as_table          ());}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

template<typename Visitor, typename TC>
cxx::return_type_of_t<Visitor, typename basic_value<TC>::boolean_type&>
visit(Visitor&& visitor, basic_value<TC>& v)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visitor(v.as_boolean        ());}
        case value_t::integer        : {return visitor(v.as_integer        ());}
        case value_t::floating       : {return visitor(v.as_floating       ());}
        case value_t::string         : {return visitor(v.as_string         ());}
        case value_t::offset_datetime: {return visitor(v.as_offset_datetime());}
        case value_t::local_datetime : {return visitor(v.as_local_datetime ());}
        case value_t::local_date     : {return visitor(v.as_local_date     ());}
        case value_t::local_time     : {return visitor(v.as_local_time     ());}
        case value_t::array          : {return visitor(v.as_array          ());}
        case value_t::table          : {return visitor(v.as_table          ());}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

template<typename Visitor, typename TC>
cxx::return_type_of_t<Visitor, typename basic_value<TC>::boolean_type&&>
visit(Visitor&& visitor, basic_value<TC>&& v)
{
    switch(v.type())
    {
        case value_t::boolean        : {return visitor(std::move(v.as_boolean        ()));}
        case value_t::integer        : {return visitor(std::move(v.as_integer        ()));}
        case value_t::floating       : {return visitor(std::move(v.as_floating       ()));}
        case value_t::string         : {return visitor(std::move(v.as_string         ()));}
        case value_t::offset_datetime: {return visitor(std::move(v.as_offset_datetime()));}
        case value_t::local_datetime : {return visitor(std::move(v.as_local_datetime ()));}
        case value_t::local_date     : {return visitor(std::move(v.as_local_date     ()));}
        case value_t::local_time     : {return visitor(std::move(v.as_local_time     ()));}
        case value_t::array          : {return visitor(std::move(v.as_array          ()));}
        case value_t::table          : {return visitor(std::move(v.as_table          ()));}
        case value_t::empty          : break;
        default: break;
    }
    throw type_error(format_error("[error] toml::visit: toml::basic_value "
            "does not have any valid type.", v.location(), "here"), v.location());
}

} // toml
#endif // TOML11_VISIT_HPP
