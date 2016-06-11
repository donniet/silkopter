#include "def_lang/Type_System.h"
#include "def_lang/ast/Builder.h"

#include "def_lang/IMember_Def.h"
#include "def_lang/IVector_Type.h"
#include "def_lang/IVariant_Type.h"
#include "def_lang/IOptional_Type.h"
#include "def_lang/IPoly_Type.h"
#include "def_lang/IEnum_Type.h"
#include "def_lang/IEnum_Value.h"
#include "def_lang/IEnum_Item.h"
#include "def_lang/IStruct_Type.h"
#include "def_lang/IStruct_Value.h"
#include "def_lang/IString_Value.h"
#include "def_lang/IVector_Value.h"
#include "def_lang/impl/Namespace.h"
#include "def_lang/IDeclaration_Scope.h"
#include "def_lang/All_INumeric_Values.h"
#include "def_lang/Value_Selector.h"
#include "def_lang/impl/Initializer_List.h"
#include "def_lang/Mapper.h"
#include "def_lang/JSON_Serializer.h"

#include "MurmurHash2.h"

#include <chrono>
#include <set>

#include <boost/program_options.hpp>


struct Context
{
    Context(std::string& o_h_file, std::string& o_cpp_file, ts::IDeclaration_Scope const& parent_scope, ts::Type_System& type_system)
        : h_file(o_h_file)
        , cpp_file(o_cpp_file)
        , parent_scope(parent_scope)
        , type_system(type_system)
    {}
    std::string h_filename;
    std::string& h_file;
    std::string& cpp_file;
    std::string serialization_section_h;
    std::string serialization_section_cpp;
    std::set<std::string> serialization_code_generated;
    ts::IDeclaration_Scope const& parent_scope;
    std::string ident_str;
    ts::Type_System& type_system;
};


static ts::Result<void> generate_code(Context& context, ts::ast::Node const& ast_root_node, ts::Type_System const& ts);

static ts::Symbol_Path s_namespace;
static std::string s_extra_include;
static bool s_enum_hashes = false;
static std::string s_json_name;


int main(int argc, char **argv)
{
    namespace po = boost::program_options;

    std::string def_filename;
    std::string out_filename;

    po::options_description desc("Options");
    desc.add_options()
        ("help", "This help message")
        ("ast", "Print the AST")
        ("nice", "Format the AST JSON nicely")
        ("namespace", po::value<std::string>(), "The namespace where to put it all")
        ("json-name", po::value<std::string>(), "Name of the function that returns the ast json string")
        ("xheader", po::value<std::string>(), "A custom support header that will be included in the generated code files")
        ("enum-hashes", po::value<bool>(), "Serialize enums as hashes instead of strings")
        ("def", po::value<std::string>(&def_filename), "Definition file")
        ("out", po::value<std::string>(&out_filename), "Output file");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << "\n";
        return 1;
    }

    if (def_filename.empty())
    {
        std::cout << "Filename is required!\n" << desc << "\n";
        return 1;
    }

    if (out_filename.empty())
    {
        out_filename = def_filename;
    }

    std::string h_filename = out_filename + ".h";
    std::ofstream h_stream(h_filename);
    if (!h_stream.is_open())
    {
        std::cerr << "Cannot open out file '" + h_filename + "'";
        return 1;
    }
    std::string cpp_filename = out_filename + ".cpp";
    std::ofstream cpp_stream(cpp_filename);
    if (!cpp_stream.is_open())
    {
        std::cerr << "Cannot open out file '" + cpp_filename + "'";
        return 1;
    }

    bool show_ast = vm.count("ast") != 0;
    bool nice_json = vm.count("nice") != 0;
    s_namespace = vm.count("namespace") ? ts::Symbol_Path(vm["namespace"].as<std::string>()) : ts::Symbol_Path();
    s_json_name = vm.count("json-name") ? vm["json-name"].as<std::string>() : std::string();
    s_extra_include = vm.count("xheader") ? vm["xheader"].as<std::string>() : std::string();
    s_enum_hashes = vm.count("enum-hashes") ? vm["enum-hashes"].as<bool>() : false;

    ts::ast::Builder builder;

    auto parse_result = builder.parse_file(def_filename);
    if (parse_result != ts::success)
    {
        std::cerr << parse_result.error().what();
        std::cerr.flush();
        return 1;
    }

    if (show_ast)
    {
        std::cout << builder.get_ast_root_node().to_string(0, true) << std::endl;
        std::cout.flush();
    }

    ts::Type_System ts;
    ts.populate_builtin_types();

    auto compile_result = builder.compile(ts);
    if (compile_result != ts::success)
    {
        std::cerr << compile_result.error().what();
        std::cerr.flush();
        return 1;
    }

    std::string h_file, cpp_file;
    Context context(h_file, cpp_file, ts, ts);
    context.h_filename = h_filename;
    auto result = generate_code(context, builder.get_ast_root_node(), ts);
    if (result != ts::success)
    {
        std::cerr << result.error().what();
        std::cerr.flush();
    }

    h_stream << h_file;
    cpp_stream << cpp_file;

    return 0;
}

static void generate_ast_code(Context& context, std::string const& ast_json)
{
    context.h_file += context.ident_str + "// Returns the ast json from which a ast root node can be serialized\n";
    context.h_file += context.ident_str + "std::string const& " + s_json_name + "();\n";

    context.cpp_file += context.ident_str + "std::string const& " + s_json_name + "()\n";
    context.cpp_file += context.ident_str + "{\n";
    context.cpp_file += context.ident_str + "  static const std::string s_json = R\"xxx(";
    context.cpp_file += context.ident_str + ast_json;
    context.cpp_file += context.ident_str + ")xxx\";\n";
    context.cpp_file += context.ident_str + "  return s_json;\n";
    context.cpp_file += context.ident_str + "}\n\n";
}

static void generate_aux_functions(Context& context)
{
    context.cpp_file += context.ident_str + "template <typename T>\n";
    context.cpp_file += context.ident_str + "T clamp(T v, T min, T max)\n";
    context.cpp_file += context.ident_str + "{\n";
    context.cpp_file += context.ident_str + "  return std::min(std::max(v, min), max);\n";
    context.cpp_file += context.ident_str + "}\n";
    context.cpp_file += context.ident_str + "template <typename T>\n";
    context.cpp_file += context.ident_str + "T min(T v, T min)\n";
    context.cpp_file += context.ident_str + "{\n";
    context.cpp_file += context.ident_str + "  return std::min(v, min);\n";
    context.cpp_file += context.ident_str + "}\n";
    context.cpp_file += context.ident_str + "template <typename T>\n";
    context.cpp_file += context.ident_str + "T max(T v, T max)\n";
    context.cpp_file += context.ident_str + "{\n";
    context.cpp_file += context.ident_str + "  return std::max(v, max);\n";
    context.cpp_file += context.ident_str + "}\n";
}

static ts::Result<void> generate_declaration_scope_code(Context& context, ts::IDeclaration_Scope const& ds);
static ts::Symbol_Path get_type_relative_scope_path(ts::IDeclaration_Scope const& scope, ts::IType const& type);


static ts::Symbol_Path get_native_type(ts::IDeclaration_Scope const& scope, ts::IType const& type)
{
    if (ts::IVariant_Type const* t = dynamic_cast<ts::IVariant_Type const*>(&type))
    {
        std::string str = "boost::variant<";
        for (size_t i = 0; i < t->get_inner_type_count(); i++)
        {
            std::shared_ptr<const ts::IType> const& inner_type = t->get_inner_type(i);
            str += scope.get_symbol_path().get_path_to(get_native_type(scope, *inner_type)).to_string() + ",";
        }
        str.pop_back();
        str += ">";
        return ts::Symbol_Path(str);
    }
    else if (ts::IOptional_Type const* t = dynamic_cast<ts::IOptional_Type const*>(&type))
    {
        std::string str = "boost::optional<";
        std::shared_ptr<const ts::IType> const& inner_type = t->get_inner_type();
        str += scope.get_symbol_path().get_path_to(get_native_type(scope, *inner_type)).to_string();
        str += ">";
        return ts::Symbol_Path(str);
    }
    else if (ts::IVector_Type const* t = dynamic_cast<ts::IVector_Type const*>(&type))
    {
        return ts::Symbol_Path("std::vector<" + get_native_type(scope, *t->get_inner_type()).to_string() + ">");
    }
    else if (ts::IPoly_Type const* t = dynamic_cast<ts::IPoly_Type const*>(&type))
    {
        return ts::Symbol_Path("std::shared_ptr<" + get_native_type(scope, *t->get_inner_type()).to_string() + ">");
    }
    else if (dynamic_cast<ts::IStruct_Type const*>(&type) ||
             dynamic_cast<ts::IEnum_Type const*>(&type))
    {
        //these are defined locally, within s_namespace so make sure they don't have absolute paths
        return type.get_native_type().to_relative();
    }

    return type.get_native_type();
}

static ts::Symbol_Path get_type_relative_scope_path(ts::IDeclaration_Scope const& scope, ts::IType const& type)
{
    ts::Symbol_Path scope_path = scope.get_symbol_path();
    ts::Symbol_Path type_path = get_native_type(scope, type);

    return scope_path.get_path_to(type_path);
}

static std::string to_string(bool v) { return v ? "true" : "false"; }
static std::string to_string(int64_t v) { return std::to_string(v) + "LL"; }
static std::string to_string(float v) { return std::to_string(v) + "f"; }
static std::string to_string(double v) { return std::to_string(v); }
static std::string to_string(ts::vec2f const& v) { return to_string(v.x) + ", " + to_string(v.y); }
static std::string to_string(ts::vec2d const& v) { return to_string(v.x) + ", " + to_string(v.y); }
static std::string to_string(ts::vec2<int64_t> const& v) { return to_string(v.x) + ", " + to_string(v.y); }
static std::string to_string(ts::vec3f const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z); }
static std::string to_string(ts::vec3d const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z); }
static std::string to_string(ts::vec3<int64_t> const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z); }
static std::string to_string(ts::vec4f const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z) + ", " + to_string(v.w); }
static std::string to_string(ts::vec4d const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z) + ", " + to_string(v.w); }
static std::string to_string(ts::vec4<int64_t> const& v) { return to_string(v.x) + ", " + to_string(v.y) + ", " + to_string(v.z) + ", " + to_string(v.w); }
static std::string to_string(std::string const& v) { return v.empty() ? v : "\"" + v + "\""; }
static std::string to_string(ts::IEnum_Item const& v) { return v.get_symbol_path().to_relative().to_string(); }


static std::string to_string(ts::IValue const& value)
{
    if (ts::IBool_Value const* v = dynamic_cast<ts::IBool_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IInt_Value const* v = dynamic_cast<ts::IInt_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IFloat_Value const* v = dynamic_cast<ts::IFloat_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IDouble_Value const* v = dynamic_cast<ts::IDouble_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec2f_Value const* v = dynamic_cast<ts::IVec2f_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec2d_Value const* v = dynamic_cast<ts::IVec2d_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec2i_Value const* v = dynamic_cast<ts::IVec2i_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec3f_Value const* v = dynamic_cast<ts::IVec3f_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec3d_Value const* v = dynamic_cast<ts::IVec3d_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec3i_Value const* v = dynamic_cast<ts::IVec3i_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec4f_Value const* v = dynamic_cast<ts::IVec4f_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec4d_Value const* v = dynamic_cast<ts::IVec4d_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IVec4i_Value const* v = dynamic_cast<ts::IVec4i_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IString_Value const* v = dynamic_cast<ts::IString_Value const*>(&value))
    {
        return to_string(v->get_value());
    }
    if (ts::IEnum_Value const* v = dynamic_cast<ts::IEnum_Value const*>(&value))
    {
        return to_string(*v->get_value());
    }
    return "";
}

static void generate_member_def_declaration_code(Context& context, ts::IMember_Def const& member_def)
{
    std::string native_type_str = get_type_relative_scope_path(context.parent_scope, *member_def.get_type()).to_string();

    context.h_file += context.ident_str +
            native_type_str +
            " m_" +
            member_def.get_name() +
            " = {" +
            to_string(*member_def.get_default_value()) +
            "};\n";
}

template <typename T, typename Native>
static std::string generate_scalar_type_clamping_code(Context& context, T const& type)
{
    std::string native_type_str = get_type_relative_scope_path(context.parent_scope, type).to_string();
    if (type.get_min_value() == std::numeric_limits<Native>::lowest() && type.get_max_value() == std::numeric_limits<Native>::max())
    {
        return "value";
    }
    if (type.get_min_value() == std::numeric_limits<Native>::lowest())
    {
        return "min(value, " + native_type_str + "(" + to_string(type.get_max_value()) + "));";
    }
    if (type.get_max_value() == std::numeric_limits<Native>::max())
    {
        return "max(value, " + native_type_str + "(" + to_string(type.get_min_value()) + "));";
    }
    return "clamp(value, " + native_type_str + "(" + to_string(type.get_min_value()) + "), " + native_type_str + "(" + to_string(type.get_max_value()) + "))";
}

static std::string generate_numeric_type_clamping_code(Context& context, ts::IType const& _type)
{
    std::string native_type_str = get_type_relative_scope_path(context.parent_scope, _type).to_string();
    if (ts::IInt_Type const* type = dynamic_cast<ts::IInt_Type const*>(&_type))
    {
        return generate_scalar_type_clamping_code<ts::IInt_Type, int64_t>(context, *type);
    }
    if (ts::IFloat_Type const* type = dynamic_cast<ts::IFloat_Type const*>(&_type))
    {
        return generate_scalar_type_clamping_code<ts::IFloat_Type, float>(context, *type);
    }
    if (ts::IDouble_Type const* type = dynamic_cast<ts::IDouble_Type const*>(&_type))
    {
        return generate_scalar_type_clamping_code<ts::IDouble_Type, double>(context, *type);
    }
    if (ts::IVec2f_Type const* type = dynamic_cast<ts::IVec2f_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec2d_Type const* type = dynamic_cast<ts::IVec2d_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec2i_Type const* type = dynamic_cast<ts::IVec2i_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec3f_Type const* type = dynamic_cast<ts::IVec3f_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec3d_Type const* type = dynamic_cast<ts::IVec3d_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec3i_Type const* type = dynamic_cast<ts::IVec3i_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec4f_Type const* type = dynamic_cast<ts::IVec4f_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec4d_Type const* type = dynamic_cast<ts::IVec4d_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    if (ts::IVec4i_Type const* type = dynamic_cast<ts::IVec4i_Type const*>(&_type))
    {
        return "clamp(value, " + native_type_str + "(" + to_string(type->get_min_value()) + "), " + native_type_str + "(" + to_string(type->get_max_value()) + "))";
    }
    return "value";
}

static void generate_member_def_setter_getter_code(Context& context, ts::IStruct_Type const& struct_type, ts::IMember_Def const& member_def)
{
    ts::IDeclaration_Scope const* ds = &struct_type;
    while (dynamic_cast<const ts::IStruct_Type*>(ds))
    {
        ds = ds->get_parent_scope();
    }
    TS_ASSERT(ds);

    std::string struct_type_str = get_type_relative_scope_path(*ds, struct_type).to_string();
    std::string native_type_str = get_type_relative_scope_path(context.parent_scope, *member_def.get_type()).to_string();

    context.h_file += context.ident_str + "void set_" + member_def.get_name() + "(" + native_type_str + " const& value);\n";
    context.h_file += context.ident_str + "auto get_" + member_def.get_name() + "() const -> " + native_type_str + " const&;\n";

    bool has_mutable_getter = std::dynamic_pointer_cast<const ts::IStruct_Type>(member_def.get_type()) != nullptr ||
                                std::dynamic_pointer_cast<const ts::IVector_Type>(member_def.get_type()) != nullptr ||
                                std::dynamic_pointer_cast<const ts::IVariant_Type>(member_def.get_type()) != nullptr ||
                                std::dynamic_pointer_cast<const ts::IOptional_Type>(member_def.get_type()) != nullptr ||
                                std::dynamic_pointer_cast<const ts::IPoly_Type>(member_def.get_type()) != nullptr;

    if (has_mutable_getter)
    {
        context.h_file += context.ident_str + "auto get_" + member_def.get_name() + "() -> " + native_type_str + "&;\n";
    }

    context.cpp_file += context.ident_str + "void " + struct_type_str + "::set_" + member_def.get_name() + "(" + native_type_str + " const& value)\n" +
            context.ident_str + "{\n" +
            context.ident_str + "  m_" + member_def.get_name() + " = " + generate_numeric_type_clamping_code(context, *member_def.get_type()) + ";\n" +
            context.ident_str + "}\n";

    context.cpp_file += context.ident_str + "auto " + struct_type_str + "::get_" + member_def.get_name() + "() const -> " + native_type_str + " const& \n" +
            context.ident_str + "{\n" +
            context.ident_str + "  return m_" + member_def.get_name() + ";\n" +
            context.ident_str + "}\n\n";

    if (has_mutable_getter)
    {
        context.cpp_file += context.ident_str + "auto " + struct_type_str + "::get_" + member_def.get_name() + "() -> " + native_type_str + "& \n" +
                context.ident_str + "{\n" +
                context.ident_str + "  return m_" + member_def.get_name() + ";\n" +
                context.ident_str + "}\n\n";
    }
}

static ts::Result<void> generate_struct_type_serialization_code(Context& context, ts::IStruct_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_object()) { return ts::Error(\"Expected object value when deserializing\"); }\n";

    for (size_t i = 0; i < type.get_member_def_count(); i++)
    {
        ts::IMember_Def const& member_def = *type.get_member_def(i);

        context.serialization_section_cpp += "  {\n"
                                             "    auto const* member_sz_value = sz_value.find_object_member_by_name(\"" + member_def.get_name() + "\");\n"
                                             "    if (!member_sz_value) { return ts::Error(\"Cannot find member value '" + member_def.get_name() + "'\"); }\n"
                                             "    std::remove_cv<std::remove_reference<decltype(value.get_" + member_def.get_name() + "())>::type>::type v;\n"
                                             "    auto result = deserialize(v, *member_sz_value);\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "    value.set_" + member_def.get_name() + "(v);\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  return ts::success;\n"
                                         "}\n";



    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  ts::serialization::Value sz_value(ts::serialization::Value::Type::OBJECT);\n";

    for (size_t i = 0; i < type.get_member_def_count(); i++)
    {
        ts::IMember_Def const& member_def = *type.get_member_def(i);

        context.serialization_section_cpp += "  {\n"
                                             "    auto result = serialize(value.get_" + member_def.get_name() + "());\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "    sz_value.add_object_member(\"" + member_def.get_name() + "\", result.extract_payload());\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  return sz_value;\n"
                                         "}\n";

    return ts::success;
}

static ts::Result<void> generate_struct_type_code(Context& context, ts::IStruct_Type const& struct_type)
{
    std::string struct_name = get_native_type(context.parent_scope, struct_type).back();
    context.h_file += context.ident_str + "struct " + struct_name;
    if (struct_type.get_base_struct())
    {
        context.h_file += " : public " + get_native_type(context.parent_scope, *struct_type.get_base_struct()).to_string();
    }
    context.h_file += "\n";
    context.h_file += context.ident_str +  "{\n";
    context.h_file += context.ident_str + "public:\n\n";

    Context c(context.h_file, context.cpp_file, struct_type, context.type_system);
    c.serialization_code_generated = context.serialization_code_generated;

    c.ident_str = context.ident_str + "  ";
    auto result = generate_declaration_scope_code(c, struct_type);
    if (result != ts::success)
    {
        return result;
    }

    context.serialization_code_generated.insert(c.serialization_code_generated.begin(), c.serialization_code_generated.end());
    context.serialization_section_cpp += c.serialization_section_cpp;
    context.serialization_section_h += c.serialization_section_h;

    context.h_file += c.ident_str + struct_name + "() noexcept {};\n";
    context.h_file += c.ident_str + "virtual ~" + struct_name + "() noexcept {};\n\n";

    for (size_t i = 0; i < struct_type.get_noninherited_member_def_count(); i++)
    {
        generate_member_def_setter_getter_code(c, struct_type, *struct_type.get_noninherited_member_def(i));
        context.h_file += "\n";
        context.cpp_file += "\n////////////////////////////////////////////////////////////\n\n";
    }

    context.h_file += "\n";
    context.h_file += context.ident_str + "private:\n\n";

    for (size_t i = 0; i < struct_type.get_noninherited_member_def_count(); i++)
    {
        generate_member_def_declaration_code(c, *struct_type.get_noninherited_member_def(i));
    }

    context.h_file += context.ident_str + "};\n\n";

    return generate_struct_type_serialization_code(context, struct_type);
}

static ts::Result<void> generate_enum_type_serialization_code(Context& context, ts::IEnum_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    if (s_enum_hashes)
    {
        std::vector<uint32_t> hashes;
        {
            std::map<uint32_t, std::string> mm;
            for (size_t i = 0; i < type.get_item_count(); i++)
            {
                std::string const& item_str = type.get_item(i)->get_name();
                uint32_t hash = MurmurHash2(item_str.data(), static_cast<int>(item_str.size()), 0);
                auto it = mm.find(hash);
                if (it != mm.end())
                {
                    return ts::Error("Enum items '" + item_str + "' and '" + it->second + "' have the same murmur hash: " + std::to_string(hash));
                }
                mm.emplace(hash, item_str);
                hashes.push_back(hash);
            }
        }

        context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
        context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                                "{\n"
                                                "  if (!sz_value.is_integral_number()) { return ts::Error(\"Expected integral number value when deserializing\"); }\n"
                                                "  uint32_t key = static_cast<uint32_t>(sz_value.get_as_integral_number());\n"
                                                "  typedef " + native_type_str + " _etype;\n"
                                                "  static std::map<uint32_t, _etype> s_map = {\n";
        for (size_t i = 0; i < type.get_item_count(); i++)
        {
            std::string const& item_str = type.get_item(i)->get_name();
            uint32_t hash = hashes[i];
            context.serialization_section_cpp += "    { " + std::to_string(hash) + ", _etype::" + item_str + " },\n";
        }
        context.serialization_section_cpp +=   "  };\n"
                                               "  auto it = s_map.find(key);\n"
                                               "  if (it == s_map.end()) { return ts::Error(\"Cannot find item \" + std::to_string(key) + \" when deserializing\"); }\n"
                                               "  value = it->second;\n"
                                               "  return ts::success;\n"
                                               "}\n";

        context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
        context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                             "{\n"
                                             "  typedef " + native_type_str + " _etype;\n"
                                                                                                                                                "  static std::map<_etype, uint32_t> s_map = {\n";
        for (size_t i = 0; i < type.get_item_count(); i++)
        {
            std::string const& item_str = type.get_item(i)->get_name();
            uint32_t hash = hashes[i];
            context.serialization_section_cpp += "    { _etype::" + item_str + ", " + std::to_string(hash) + " },\n";
        }
        context.serialization_section_cpp += "  };\n"
                                             "  auto it = s_map.find(value);\n"
                                             "  if (it == s_map.end()) { return ts::Error(\"Cannot serialize type\"); }\n"
                                             "  return ts::serialization::Value(it->second);\n"
                                             "}\n";
    }
    else
    {
        context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
        context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                               "{\n"
                                               "  if (!sz_value.is_string()) { return ts::Error(\"Expected string value when deserializing\"); }\n"
                                               "  std::string const& key = sz_value.get_as_string();\n"
                                               "  typedef " + native_type_str + " _etype;\n"
                                               "  static std::map<std::string, _etype> s_map = {\n";
        for (size_t i = 0; i < type.get_item_count(); i++)
        {
            std::string const& item_str = type.get_item(i)->get_name();
            context.serialization_section_cpp += "    { \"" + item_str + "\", _etype::" + item_str + " },\n";
        }
        context.serialization_section_cpp +=   "  };\n"
                                               "  auto it = s_map.find(key);\n"
                                               "  if (it == s_map.end()) { return ts::Error(\"Cannot find item \" + key + \" when deserializing\"); }\n"
                                               "  value = it->second;\n"
                                               "  return ts::success;\n"
                                               "}\n";

        context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
        context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                             "{\n"
                                             "  typedef " + native_type_str + " _etype;\n"
                                             "  static std::map<_etype, std::string> s_map = {\n";
        for (size_t i = 0; i < type.get_item_count(); i++)
        {
            std::string const& item_str = type.get_item(i)->get_name();
            context.serialization_section_cpp += "    { _etype::" + item_str + ", \"" +  item_str + "\" },\n";
        }
        context.serialization_section_cpp += "  };\n"
                                             "  auto it = s_map.find(value);\n"
                                             "  if (it == s_map.end()) { return ts::Error(\"Cannot serialize type\"); }\n"
                                             "  return ts::serialization::Value(it->second);\n"
                                             "}\n";
    }
    return ts::success;
}

static ts::Result<void> generate_enum_type_code(Context& context, ts::IEnum_Type const& enum_type)
{
    std::string enum_name = get_native_type(context.parent_scope, enum_type).back();
    context.h_file += context.ident_str + "enum class " + enum_name + "\n";
    context.h_file += context.ident_str + "{\n";

    for (size_t i = 0; i < enum_type.get_item_count(); i++)
    {
        std::shared_ptr<const ts::IEnum_Item> item = enum_type.get_item(i);
        context.h_file += context.ident_str + "  " + item->get_name() + " = " + std::to_string(item->get_integral_value()) + ",\n";
    }

    context.h_file += context.ident_str + "};\n\n";

    return generate_enum_type_serialization_code(context, enum_type);
}

static ts::Result<void> generate_poly_type_code(Context& context, ts::IPoly_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    std::vector<std::shared_ptr<const ts::IStruct_Type>> inner_types = type.get_all_inner_types();

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (sz_value.is_empty()) { value = nullptr; return ts::success; }\n"
                                           "  if (!sz_value.is_object()) { return ts::Error(\"Expected object or null value when deserializing\"); }\n"
                                           "  auto const* type_sz_value = sz_value.find_object_member_by_name(\"type\");\n"
                                           "  if (!type_sz_value || !type_sz_value->is_string()) { return ts::Error(\"Expected 'type' string value when deserializing\"); }\n"
                                           "  auto const* value_sz_value = sz_value.find_object_member_by_name(\"value\");\n"
                                           "  if (!value_sz_value) { return ts::Error(\"Expected 'value' when deserializing\"); }\n"
                                           "  std::string const& path = type_sz_value->get_as_string();\n"
                                           "  if (false) { return ts::Error(\"\"); } //this is here just to have the next items with 'else if'\n";

    for (std::shared_ptr<const ts::IStruct_Type> inner_type: inner_types)
    {
        std::string native_inner_type_str = get_native_type(context.parent_scope, *inner_type).to_string();
        context.serialization_section_cpp += "  else if (path == \"" + inner_type->get_symbol_path().to_string() + "\")\n"
                                         "  {\n"
                                         "    value.reset(new " + native_inner_type_str + "());\n"
                                         "    return deserialize((" + native_inner_type_str + "&)*value, *value_sz_value);\n"
                                         "  }\n";
    }
    context.serialization_section_cpp += "  else { return ts::Error(\"Cannot find type '\" + path + \"' when deserializing\"); }\n"
                                         "  return ts::success;\n"
                                         "}\n";

    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  if (!value) { return ts::serialization::Value(ts::serialization::Value::Type::EMPTY); }\n"
                                           "  ts::serialization::Value sz_value(ts::serialization::Value::Type::OBJECT);\n"
                                           "  if (false) { return ts::Error(\"\"); } //this is here just to have the next items with 'else if'\n";

    for (std::shared_ptr<const ts::IStruct_Type> inner_type: inner_types)
    {
        std::string native_inner_type_str = get_native_type(context.parent_scope, *inner_type).to_string();
        context.serialization_section_cpp += "  else if (typeid(*value) == typeid(" + native_inner_type_str + "))\n"
                                         "  {\n"
                                         "    sz_value.add_object_member(\"type\", \"" + native_inner_type_str + "\");\n"
                                         "    auto result = serialize((" + native_inner_type_str + "&)*value);\n"
                                         "    if (result != ts::success) { return result; }\n"
                                         "    sz_value.add_object_member(\"value\", result.extract_payload());\n"
                                         "    return std::move(sz_value);\n"
                                         "  }\n";
    }
    context.serialization_section_cpp += "  else { return ts::Error(\"Cannot serialize type\"); }\n"
                                     "}\n";
    return ts::success;
}
static ts::Result<void> generate_bool_type_code(Context& context, ts::IBool_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_bool()) { return ts::Error(\"Expected bool value when deserializing\"); }\n"
                                           "  value = sz_value.get_as_bool();\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  return ts::serialization::Value(value);\n"
                                           "}\n";
    return ts::success;
}
static ts::Result<void> generate_string_type_code(Context& context, ts::IString_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_string()) { return ts::Error(\"Expected string value when deserializing\"); }\n"
                                           "  value = sz_value.get_as_string();\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  return ts::serialization::Value(value);\n"
                                           "}\n";
    return ts::success;
}
static ts::Result<void> generate_variant_type_code(Context& context, ts::IVariant_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_object()) { return ts::Error(\"Expected object value when deserializing\"); }\n"
                                           "  auto const* type_sz_value = sz_value.find_object_member_by_name(\"type\");\n"
                                           "  if (!type_sz_value || !type_sz_value->is_string()) { return ts::Error(\"Expected 'type' string value when deserializing\"); }\n"
                                           "  auto const* value_sz_value = sz_value.find_object_member_by_name(\"value\");\n"
                                           "  if (!value_sz_value) { return ts::Error(\"Expected 'value' when deserializing\"); }\n"
                                           "  std::string const& path = type_sz_value->get_as_string();\n"
                                           "  if (false) { return ts::Error(\"\"); } //this is here just to have the next items with 'else if'\n";

    for (size_t i = 0; i < type.get_inner_type_count(); i++)
    {
        std::string native_inner_type_str = get_native_type(context.parent_scope, *type.get_inner_type(i)).to_string();
        context.serialization_section_cpp += "  else if (path == \"" + type.get_inner_type(i)->get_symbol_path().to_string() + "\")\n"
                                             "  {\n"
                                             "    " + native_inner_type_str + " v;\n"
                                             "    auto result = deserialize(boost::get<" + native_inner_type_str + ">(value), *value_sz_value);\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "    value = v;\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  else { return ts::Error(\"Cannot find type '\" + path + \"' when deserializing\"); }\n"
                                         "  return ts::success;\n"
                                         "}\n";

    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                         "{\n"
                                         "  ts::serialization::Value sz_value(ts::serialization::Value::Type::OBJECT);\n"
                                         "  if (false) { return ts::Error(\"\"); } //this is here just to have the next items with 'else if'\n";

    for (size_t i = 0; i < type.get_inner_type_count(); i++)
    {
        std::string native_inner_type_str = get_native_type(context.parent_scope, *type.get_inner_type(i)).to_string();
        context.serialization_section_cpp += "  else if (auto* v = boost::get<" + native_inner_type_str + ">(&value))\n"
                                             "  {\n"
                                             "    sz_value.add_object_member(\"type\", \"" + native_inner_type_str + "\");\n"
                                             "    auto result = serialize(*v);\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "    sz_value.add_object_member(\"value\", result.extract_payload());\n"
                                             "    return std::move(sz_value);\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  else { return ts::Error(\"Cannot serialize type\"); }\n"
                                         "}\n";
    return ts::success;
}
static ts::Result<void> generate_optional_type_code(Context& context, ts::IOptional_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    std::string native_inner_type_str = get_native_type(context.parent_scope, *type.get_inner_type()).to_string();

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (sz_value.is_empty()) { value = boost::none; return ts::success; }\n"
                                           "  value = " + native_inner_type_str + "();\n"
                                           "  return deserialize(*value, sz_value);\n"
                                           "}\n";

    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                         "{\n"
                                         "  if (!value) { return ts::serialization::Value(ts::serialization::Value::Type::EMPTY); }\n"
                                         "  return serialize(*value);\n"
                                         "}\n";
    return ts::success;
}
static ts::Result<void> generate_vector_type_code(Context& context, ts::IVector_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  value.clear();\n"
                                           "  if (!sz_value.is_array()) { return ts::Error(\"Expected array value when deserializing\"); }\n"
                                           "  value.resize(sz_value.get_array_element_count());\n"
                                           "  for (size_t i = 0; i < value.size(); i++)\n"
                                           "  {\n"
                                           "    auto result = deserialize(value[i], sz_value.get_array_element_value(i));\n"
                                           "    if (result != ts::success) { return result; }\n"
                                           "  }\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  ts::serialization::Value sz_value(ts::serialization::Value::Type::ARRAY);\n"
                                           "  for (size_t i = 0; i < value.size(); i++)\n"
                                           "  {\n"
                                           "    auto result = serialize(value[i]);\n"
                                           "    if (result != ts::success) { return result; }\n"
                                           "    sz_value.add_array_element(result.extract_payload());\n"
                                           "  }\n"
                                           "  return sz_value;\n"
                                           "}\n";
    return ts::success;
}
static ts::Result<void> generate_int_type_code(Context& context, ts::IInt_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_integral_number()) { return ts::Error(\"Expected integral number value when deserializing\"); }\n"
                                           "  value = sz_value.get_as_integral_number();\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  return ts::serialization::Value(value);\n"
                                           "}\n";
    return ts::success;
}
static ts::Result<void> generate_float_type_code(Context& context, ts::IFloat_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_real_number()) { return ts::Error(\"Expected real number value when deserializing\"); }\n"
                                           "  value = (float)sz_value.get_as_real_number();\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  return ts::serialization::Value(value);\n"
                                           "}\n";
    return ts::success;
}
static ts::Result<void> generate_double_type_code(Context& context, ts::IDouble_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_real_number()) { return ts::Error(\"Expected real number value when deserializing\"); }\n"
                                           "  value = sz_value.get_as_real_number();\n"
                                           "  return ts::success;\n"
                                           "}\n";
    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  return ts::serialization::Value(value);\n"
                                           "}\n";
    return ts::success;
}

static ts::Result<void> generate_vecXY_type_code(Context& context, std::string const& native_type_str, std::vector<std::string> const& component_names)
{
    if (context.serialization_code_generated.find(native_type_str) != context.serialization_code_generated.end())
    {
        return ts::success;
    }
    context.serialization_code_generated.insert(native_type_str);

    context.serialization_section_h += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value);\n";
    context.serialization_section_cpp += "ts::Result<void> deserialize(" + native_type_str + "& value, ts::serialization::Value const& sz_value)\n"
                                           "{\n"
                                           "  if (!sz_value.is_object()) { return ts::Error(\"Expected object value when deserializing\"); }\n";
    for (std::string const& component_name: component_names)
    {
        context.serialization_section_cpp += "  {\n"
                                             "    auto const* sz_v = sz_value.find_object_member_by_name(\"" + component_name + "\");\n"
                                             "    if (!sz_v) { return ts::Error(\"Cannot find component '" + component_name + "'\"); }\n"
                                             "    auto result = deserialize(value." + component_name + ", *sz_v);\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  return ts::success;\n"
                                         "}\n";

    context.serialization_section_h += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value);\n";
    context.serialization_section_cpp += "ts::Result<ts::serialization::Value> serialize(" + native_type_str + " const& value)\n"
                                           "{\n"
                                           "  ts::serialization::Value sz_value(ts::serialization::Value::Type::OBJECT);\n";
    for (std::string const& component_name: component_names)
    {
        context.serialization_section_cpp += "  {\n"
                                             "    auto result = serialize(value." + component_name + ");\n"
                                             "    if (result != ts::success) { return result; }\n"
                                             "    sz_value.add_object_member(\"" + component_name + "\", result.extract_payload());\n"
                                             "  }\n";
    }
    context.serialization_section_cpp += "  return sz_value;\n"
                                         "}\n";
    return ts::success;
}

static ts::Result<void> generate_vec2i_type_code(Context& context, ts::IVec2i_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y"});
}
static ts::Result<void> generate_vec2f_type_code(Context& context, ts::IVec2f_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y"});
}
static ts::Result<void> generate_vec2d_type_code(Context& context, ts::IVec2d_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y"});
}
static ts::Result<void> generate_vec3i_type_code(Context& context, ts::IVec3i_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z"});
}
static ts::Result<void> generate_vec3f_type_code(Context& context, ts::IVec3f_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z"});
}
static ts::Result<void> generate_vec3d_type_code(Context& context, ts::IVec3d_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z"});
}
static ts::Result<void> generate_vec4i_type_code(Context& context, ts::IVec4i_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z", "w"});
}
static ts::Result<void> generate_vec4f_type_code(Context& context, ts::IVec4f_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z", "w"});
}
static ts::Result<void> generate_vec4d_type_code(Context& context, ts::IVec4d_Type const& type)
{
    std::string native_type_str = get_native_type(context.parent_scope, type).to_string();
    return generate_vecXY_type_code(context, native_type_str, {"x", "y", "z", "w"});
}

static ts::Result<void> generate_type_code(Context& context, ts::IType const& _type)
{
    if (ts::IStruct_Type const* type = dynamic_cast<ts::IStruct_Type const*>(&_type))
    {
        return generate_struct_type_code(context, *type);
    }
    else if (ts::IEnum_Type const* type = dynamic_cast<ts::IEnum_Type const*>(&_type))
    {
        return generate_enum_type_code(context, *type);
    }
    else if (ts::IPoly_Type const* type = dynamic_cast<ts::IPoly_Type const*>(&_type))
    {
        return generate_poly_type_code(context, *type);
    }
    else if (ts::IBool_Type const* type = dynamic_cast<ts::IBool_Type const*>(&_type))
    {
        return generate_bool_type_code(context, *type);
    }
    else if (ts::IString_Type const* type = dynamic_cast<ts::IString_Type const*>(&_type))
    {
        return generate_string_type_code(context, *type);
    }
    else if (ts::IVariant_Type const* type = dynamic_cast<ts::IVariant_Type const*>(&_type))
    {
        return generate_variant_type_code(context, *type);
    }
    else if (ts::IOptional_Type const* type = dynamic_cast<ts::IOptional_Type const*>(&_type))
    {
        return generate_optional_type_code(context, *type);
    }
    else if (ts::IVector_Type const* type = dynamic_cast<ts::IVector_Type const*>(&_type))
    {
        return generate_vector_type_code(context, *type);
    }
    else if (ts::IInt_Type const* type = dynamic_cast<ts::IInt_Type const*>(&_type))
    {
        return generate_int_type_code(context, *type);
    }
    else if (ts::IFloat_Type const* type = dynamic_cast<ts::IFloat_Type const*>(&_type))
    {
        return generate_float_type_code(context, *type);
    }
    else if (ts::IDouble_Type const* type = dynamic_cast<ts::IDouble_Type const*>(&_type))
    {
        return generate_double_type_code(context, *type);
    }
    else if (ts::IVec2i_Type const* type = dynamic_cast<ts::IVec2i_Type const*>(&_type))
    {
        return generate_vec2i_type_code(context, *type);
    }
    else if (ts::IVec2f_Type const* type = dynamic_cast<ts::IVec2f_Type const*>(&_type))
    {
        return generate_vec2f_type_code(context, *type);
    }
    else if (ts::IVec2d_Type const* type = dynamic_cast<ts::IVec2d_Type const*>(&_type))
    {
        return generate_vec2d_type_code(context, *type);
    }
    else if (ts::IVec3i_Type const* type = dynamic_cast<ts::IVec3i_Type const*>(&_type))
    {
        return generate_vec3i_type_code(context, *type);
    }
    else if (ts::IVec3f_Type const* type = dynamic_cast<ts::IVec3f_Type const*>(&_type))
    {
        return generate_vec3f_type_code(context, *type);
    }
    else if (ts::IVec3d_Type const* type = dynamic_cast<ts::IVec3d_Type const*>(&_type))
    {
        return generate_vec3d_type_code(context, *type);
    }
    else if (ts::IVec4i_Type const* type = dynamic_cast<ts::IVec4i_Type const*>(&_type))
    {
        return generate_vec4i_type_code(context, *type);
    }
    else if (ts::IVec4f_Type const* type = dynamic_cast<ts::IVec4f_Type const*>(&_type))
    {
        return generate_vec4f_type_code(context, *type);
    }
    else if (ts::IVec4d_Type const* type = dynamic_cast<ts::IVec4d_Type const*>(&_type))
    {
        return generate_vec4d_type_code(context, *type);
    }

    return ts::Error("Unhandled type - " + _type.get_symbol_path().to_string());
}

static ts::Result<void> generate_namespace_code(Context& context, ts::Namespace const& ns)
{
    context.h_file += context.ident_str + "namespace " + ns.get_name() + "\n";
    context.h_file += context.ident_str + "{\n\n";
    context.cpp_file += context.ident_str + "namespace " + ns.get_name() + "\n";
    context.cpp_file += context.ident_str + "{\n\n";

    auto result = generate_declaration_scope_code(context, ns);
    if (result != ts::success)
    {
        return result;
    }

    context.h_file += context.ident_str + "}\n";
    context.cpp_file += context.ident_str + "}\n";

    return ts::success;
}

static ts::Result<void> generate_symbol_code(Context& context, ts::ISymbol const& symbol)
{
    if (ts::Namespace const* ns = dynamic_cast<ts::Namespace const*>(&symbol))
    {
        return generate_namespace_code(context, *ns);
    }
    else if (ts::IType const* type = dynamic_cast<ts::IType const*>(&symbol))
    {
        return generate_type_code(context, *type);
    }

    TS_ASSERT(false);
    return ts::Error("Unhandled symbol - " + symbol.get_name());
}

static ts::Result<void> generate_declaration_scope_code(Context& context, ts::IDeclaration_Scope const& ds)
{
    for (size_t i = 0; i < ds.get_symbol_count(); i++)
    {
        auto result = generate_symbol_code(context, *ds.get_symbol(i));
        if (result != ts::success)
        {
            return result;
        }
    }

    return ts::success;
}

static ts::Result<void> generate_code(Context& context, ts::ast::Node const& ast_root_node, ts::Type_System const& ts)
{
    auto serialize_result = ast_root_node.serialize();
    if (serialize_result != ts::success)
    {
        return serialize_result.error();
    }

    context.h_file += "#pragma once\n\n";
    context.h_file += "#include <stdint.h>\n";
    context.h_file += "#include <string>\n";
    context.h_file += "#include <vector>\n";
    context.h_file += "#include <memory>\n";
    context.h_file += "#include <boost/variant.hpp>\n";
    context.h_file += "#include <def_lang/Result.h>\n";
    context.h_file += "#include <def_lang/Serialization.h>\n";
    if (!s_extra_include.empty())
    {
        context.h_file += "#include \"" + s_extra_include + "\"\n";
    }



    context.cpp_file += "#include \"" + context.h_filename + "\"\n";

    generate_aux_functions(context);

    if (!s_namespace.empty())
    {
        for (size_t i = 0; i < s_namespace.get_count(); i++)
        {
            context.h_file += "namespace " + s_namespace.get(i) + "\n"
                        "{\n\n";

            context.cpp_file += "namespace " + s_namespace.get(i) + "\n"
                          "{\n\n";
        }
    }

    if (!s_json_name.empty())
    {
        std::string ast_json = ts::serialization::to_json(serialize_result.payload(), false);
        generate_ast_code(context, ast_json);
    }

    auto result = generate_declaration_scope_code(context, ts);
    if (result != ts::success)
    {
        return result;
    }

    context.h_file += context.serialization_section_h;
    context.cpp_file += context.serialization_section_cpp;

    for (size_t i = 0; i < s_namespace.get_count(); i++)
    {
        context.h_file += "}\n";
        context.cpp_file += "}\n";
    }

    return ts::success;
}