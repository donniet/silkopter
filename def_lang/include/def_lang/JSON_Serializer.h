#pragma once

#include "Serialization.h"
#include "Result.h"

namespace ts
{
namespace serialization
{

std::string to_json(Value const& value, bool nice);
Result<Value> from_json(std::string const& value);
Result<Value> from_json(void const* data, size_t size);


}
}
