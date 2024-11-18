#ifndef TOML11_CONTEXT_HPP
#define TOML11_CONTEXT_HPP

#include "error_info.hpp"
#include "spec.hpp"

#include <vector>

namespace toml
{
namespace detail
{

template<typename TypeConfig>
class context
{
  public:

    explicit context(const spec& toml_spec)
        : toml_spec_(toml_spec), errors_{}
    {}

    bool has_error() const noexcept {return !errors_.empty();}

    std::vector<error_info> const& errors() const noexcept {return errors_;}

    semantic_version&       toml_version()       noexcept {return toml_spec_.version;}
    semantic_version const& toml_version() const noexcept {return toml_spec_.version;}

    spec&       toml_spec()       noexcept {return toml_spec_;}
    spec const& toml_spec() const noexcept {return toml_spec_;}

    void report_error(error_info err)
    {
        this->errors_.push_back(std::move(err));
    }

    error_info pop_last_error()
    {
        assert( ! errors_.empty());
        auto e = std::move(errors_.back());
        errors_.pop_back();
        return e;
    }

  private:

    spec toml_spec_;
    std::vector<error_info> errors_;
};

} // detail
} // toml

#if defined(TOML11_COMPILE_SOURCES)
namespace toml
{
struct type_config;
struct ordered_type_config;
namespace detail
{
extern template class context<::toml::type_config>;
extern template class context<::toml::ordered_type_config>;
} // detail
} // toml
#endif // TOML11_COMPILE_SOURCES

#endif // TOML11_CONTEXT_HPP
