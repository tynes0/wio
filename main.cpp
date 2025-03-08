#include <iostream>
#include <fstream>
#include <iomanip>
#include "lexer.h"
#include "parser.h"

#include "ast.h"

#if 1

int main() 
{
    std::ifstream stream("tests/test1.wio", std::ios::binary | std::ios::ate);

    std::streampos end = stream.tellg();
    stream.seekg(0, std::ios::beg);
    uint64_t size = end - stream.tellg();

    std::string code;
    code.resize(size + 1);
    stream.read(code.data(), size + 1);
    stream.close();

    try
    {
        wio::lexer lexer(code);
        auto tokens = lexer.get_tokens();
        wio::parser p(tokens);
        auto tree = p.parse();

        auto prog = std::static_pointer_cast<wio::program>(tree);

        auto ac = prog->accept();
    }
    catch (wio::exception& e)
    {
        std::cout << e.what();
    }

    return 0;
}
#else

int main()
{
    try
    {
        wio::lexer lexer(code);
        auto tokens = lexer.get_tokens();
        for (const auto& token : tokens)
        {
            std::cout << "Token: " << frenum::to_string(token.type);
            if (frenum::to_string(token.type).size() < 9)
                std::cout << "\t\t";
            else if (frenum::to_string(token.type).size() < 17)
                std::cout << "\t";
            std::cout << "\t";
            std::cout << "Value: " << token.value << std::endl;
        }
    }
    catch (wio::exception& e)
    {
        std::cout << e.what();
    }
}

#endif