#pragma once

#include <memory>
#include <string>
#include "ISymbol.h"

namespace ts
{

class IDeclaration_Scope;

class Symbol_Impl : virtual public ISymbol
{
public:

    Symbol_Impl(std::string const& name);

    auto get_name() const -> std::string const& override;

private:
    std::string m_name;
};

}
