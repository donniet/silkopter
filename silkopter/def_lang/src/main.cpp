#include "Type_System.h"
#include "ast/Builder.h"

#include "IStruct_Type.h"
#include "IStruct_Value.h"
#include "IString_Value.h"
#include "IVector_Value.h"
#include "All_INumeric_Values.h"
#include "Value_Selector.h"
#include "Mapper.h"

int main(int argc, char **argv)
{
    ast::Builder builder;

    auto parse_result = builder.parse("test.def");
    if (parse_result != ts::success)
    {
        std::cerr << parse_result.error().what();
    }

    std::cout << builder.get_ast_root_node().to_string(0, true) << std::endl;
    std::cout.flush();

    ts::Type_System ts;
    ts.populate_builtin_types();

    auto compile_result = builder.compile(ts);
    if (compile_result != ts::success)
    {
        std::cerr << compile_result.error().what();
    }

    std::shared_ptr<ts::IStruct_Type> type = ts.find_specialized_symbol_by_path<ts::IStruct_Type>(ts::Symbol_Path("silk::Multirotor_Config"));
    TS_ASSERT(type);

    std::shared_ptr<ts::IStruct_Value> value = type->create_specialized_value();

    {
//        std::shared_ptr<ts::IString_Value> name = value->select_specialized<ts::IString_Value>("name");
//        auto result = name->set_value("silkopter");

//        std::shared_ptr<ts::IVector_Value> motors = value->select_specialized<ts::IVector_Value>("motors");
//        result = motors->insert_default_value(0);

//        std::shared_ptr<ts::IVec3f_Value> motor = value->select_specialized<ts::IVec3f_Value>("motors[0].position");
    }

    {
        std::string name;
        float mass;
        auto result = ts::mapper::get(*value, "name", name);
        TS_ASSERT(result == ts::success);
        result = ts::mapper::get(*value, "mass", mass);
        TS_ASSERT(result == ts::success);
    }

    return 0;
}
