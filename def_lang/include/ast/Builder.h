#pragma once

#include <fstream>
#include "def_lang/Result.h"
#include "ast/Node.h"
#include "def_lang/Type_System.h"
#include "def_lang/Source_Location.h"

namespace ast
{
class Lexer;
}

namespace yy
{
class parser;
}

namespace ast
{

class Builder final
{
    friend class yy::parser;
    friend class ast::Lexer;
public:
    Builder();
    ~Builder();

    ts::Result<void> parse(std::string const& filename);

    Node& get_ast_root_node();
    Node const& get_ast_root_node() const;

    ts::Result<void> compile(ts::Type_System& ts);

    Lexer& get_lexer();

protected:
    void report_error(ts::Error const& error);
    std::string get_filename() const;

    ts::Source_Location get_location() const;

    bool start_file(std::string const& filename);
    bool end_file();

private:
    Node m_root_node;
    std::shared_ptr<Lexer> m_lexer;
    std::shared_ptr<yy::parser> m_parser;

    struct Import
    {
        std::string filename;
        std::shared_ptr<std::ifstream> stream;
    };

    std::vector<Import> m_imports;

    ts::Result<void> m_parse_result;
};

}
