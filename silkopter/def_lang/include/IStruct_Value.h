#pragma once

#include "IValue.h"
#include "IMember_Container.h"

namespace ts
{

class IStruct_Type;

class IStruct_Value : virtual public IValue, virtual public IMember_Container
{
public:
    virtual ~IStruct_Value() = default;

    virtual std::shared_ptr<IStruct_Type const> get_specialized_type() const = 0;
};


}