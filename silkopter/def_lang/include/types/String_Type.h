#include "IString_Type.h"
#include "impl/Type_Template_EP.h"

namespace ts
{

class IString_Value;
class String_Value;
class String_Type;

struct String_Type_Traits : IString_Type::traits
{
    typedef String_Value value_implementation;
    typedef String_Type type_implementation;
};

class String_Type final : public Type_Template_EP<String_Type_Traits>
{
public:
    String_Type(std::string const& name);

};


}

