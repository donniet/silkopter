#pragma once

#include "Result.h"
#include "ep/Declaration_Scope_EP.h"
#include "IType.h"

namespace ts
{

class ITemplate_Argument;
class ITemplated_Type;

class Type_System final : virtual public Declaration_Scope_EP
{
public:
    Type_System();
    Result<std::shared_ptr<const ITemplated_Type>> instantiate_template(std::string const& name, std::vector<std::shared_ptr<const ITemplate_Argument>> const& arguments);

    void populate_builtin_types();

    using Declaration_Scope_EP::add_symbol;
    using Declaration_Scope_EP::get_symbol_count;
    using Declaration_Scope_EP::get_symbol;
    using Declaration_Scope_EP::get_symbol;

};

}
