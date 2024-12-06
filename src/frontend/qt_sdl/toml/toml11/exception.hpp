#ifndef TOML11_EXCEPTION_HPP
#define TOML11_EXCEPTION_HPP

#include <exception>

namespace toml
{

struct exception : public std::exception
{
  public:
    virtual ~exception() noexcept override = default;
    virtual const char* what() const noexcept override {return "";}
};

} // toml
#endif // TOMl11_EXCEPTION_HPP
