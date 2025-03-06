#include <iostream>
#include <iomanip>
#include "lexer.h"
#include "variables/array.h"
#include "variables/dictionary.h"
#include "variables/function.h"

#if 0
int main() {
    std::string code = R"(import utils.whip;

        local var a = 42;

        func test(var collection)
        {
            const const_var = 0b1001;
            global var b = "hello";
            for (var i = 0; i < 10; i = i++) 
            {
                print(i);
                if (i == 5) break;
                #* this should be a
                multiline comment *#
            }
            ## this is a line comment
            foreach (item in collection) {
                print(item);
            }
        }
    )";

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

    return 0;
}
#else

int main()
{
    std::shared_ptr<wio::variable> var = std::make_shared<wio::variable>("int_var", 5, wio::variable_type::vt_integer );
    std::shared_ptr<wio::variable> var1 = std::make_shared<wio::variable>("string_var", "hello world", wio::variable_type::vt_string);
    std::shared_ptr<wio::variable> var2 = std::make_shared<wio::variable>("bool_var", false, wio::variable_type::vt_bool);
    std::shared_ptr<wio::variable> var3 = std::make_shared<wio::variable>("float_var", 14.2f, wio::variable_type::vt_float);
    std::shared_ptr<wio::var_array> arr = std::make_shared<wio::var_array>(wio::var_array("arr", false, { var, var2 }));


    wio::var_dictionary dict("test_dict");
    dict.add(wio::dict_v(var1, var));
    dict.add(wio::dict_v(var2, var3));
    dict.add(wio::dict_v(var3, arr));

}

#endif