# Wio Programming Language Documentation

## 1. Introduction

### What is Wio?

Wio is a lightweight and fast scripting language designed primarily for game development and embedded scripting environments (actually for self-improvement purposes :d). It combines the best aspects of modern dynamic languages like Python, Lua, and JavaScript with a clean syntax and a C-like structure.
 
Note: Since wio is under active development, it may not provide everything stated below. In fact, it would be more logical to say that it is useless for the game for now.

### Key Features

- Dynamically typed variables  
- High-level data structures: `array`, `dict`, `pair`, `vec2/3/4`
- Modular design with `import` support  
- Function overloading and higher-order functions  
- Custom I/O, math, and utility modules built-in  


### Hello World Example

```wio
Print("Hello, world!");
```

### Syntax Basics

1. __Comment Lines:__ You can write single-line comments with ## and multi-line comments with #* ... *#.
    ```wio
    #*
    This is a
    good comment!
    *#
    
    func MyFunc()
    {
        Println("Hello, world!"); ## Prints Hello world!
    }
    
    MyFunc();
    ```
2. __Variable Definition:__ There are four basic types: var, array, dict and func. 
    - Basic types cannot be converted into each other.
    - Variable types are dynamic. 
    - Anything can be dynamically contained within an array, including var, array, dict and func.
    - In the dict type, the key type can consist of certain types. (You can find the explanations in the section where the dict keyword and type are explained in detail.)
    - Variables are created in the form of ```var MyVar = 123;```
    - All variables except func can be const (func can be considered const under any circumstances).
    - Const ariables are created in the form of ```const var MyConstVar = 123;```
    - When we use const keyword without specifying the base type, the base type behaves as 'var'. So ```const var MyConstVar = 123;``` and ```const MyConstVar = 123;``` are equal.
3. __Control Blocks:__ 
    - __if - else if - else__: It has a basic control structure. If the if condition is true, the block below it runs. If it is false, it moves on to other blocks or continues without doing anything if there are no more blocks. If else if exists and the first condition is false, it checks the condition again and continues in this way until it finds true. If true is not found and the else block exists, the else block runs.
    ``` wio
    var x = 5;
    if(x == 3)
    {
        ## This part is not accessible.
    }
    else if(x == 5)
    {
        ## This part is accessible.
    }
    else
    {
        ## This part is not accessible.
    }

    ```
    - __While Loops:__ It is defined as ``` while(condition) { } ```
    ``` wio

    while (true) 
    { 
        break; ## This part is accessible. 
    }

    while (false)
    {
        break; ## This part is not accessible. 
    }

    ```
    - __For Loops:__ It is defined as ``` for(variable definition; condition; operation) { } ``` These may also be empty.
    ``` wio

    for(var i = 0; i < 10; i++) {} ## Valid.

    for(var i = 0; i < 10;) {} ## Valid. (If you do not perform the operations in the body, an infinite loop will occur.)
    
    for(var i = 0; ; i++) {} ## Valid. (If the break statement is not used, an infinite loop occurs.)

    for(var i = 0;;) {} ## Valid. (If the break statement is not used, an infinite loop occurs.)

    for(;;) {} ## Valid. (infinite loop.)

    var j = 0;
    for(;j < 10; j++) {} ## Valid.

    ```
    - __Foreach Loops:__ It is defined as ` for(ItemName in container) { } ` Since any basic type can be in containers, the type is not specified. The name you want to give to the variable is written directly.
    ``` wio
    foreach(item in [1,2,3])
        Print(item); ## Output 123
    
    Println();
    
    dict MyDict = 
    { 
        [1 : 123],
        ["MyString" : 2]
    };
    
    foreach(pair in MyDict)
    {
        Print(pair.Key);
        Print(" = ");
        Print(pair.Value);
        Print(" ");
    } ## Output: 1 = 123 MyString = 2 
    
    Println();
    
    var str = "String";

    foreach (ch in str)
    {
        ch = 'x'; 
    }
    Print(str); ## Output: xxxxxx

    ```
    - Note: Any control or loop can also be used with a single operation instead of a block.
    - __break and continue:__ break lets you break out of the loop in which it was called. When continue is called, it allows you to move directly to the next iteration.
    ``` wio

    for (var i = 0; i < 10; ++i) 
    { 
        if(i == 3)
            continue;
        if(i == 6) 
            break;
        Print(i); ## output 01245
    }

    ```

4. __local and global Keywords:__
    - The local keyword ensures that a global variable, regardless of its base type, remains specific to that file. That is, it cannot be accessed from other files that import that file.
    - The global keyword ensures that a variable within a scope that has entered the process behaves like a global variable and can be accessed from outside. 
    - Note: In a non-global definition, the use of local is ignored. Also, a variable that is made global with the global keyword can be marked as local.
    - Example Usage: 
    ```wio
    local dict MyDict = {}; ## This variable is only accessible in this file.
    
    if(true)
        global array MyArray[1,2,3];
    else 
        global array MySecondArray[1,2,3];

    MyArray[1] = 4; ## This is valid
    MySecondArray[1] = 4; ## This is NOT valid

    {
        local var x = 0; ## Since this variable is not global, local usage is ignored.
        global local var y = 0; ## This value was first made global. Then it was made specific to the file with the local keyword.
    }

    ```
5. __null, true and false keywords:__
    - __null and ? operator:__ null can be assigned to any primitive type and makes the variable have no value. You can also check if the value is null with the ? operator. Creating empty variables is equivalent to initializing them with null.
    ```wio
    var x = null; ## this is equal to var x;

    if(?x)
    {
        ## We couldn't get here because there is no value
    }
    
    ```
    - __true and false:__ These are boolean values used to represent logical truth and falsehood. true typically means a condition is met or valid, while false indicates the opposite.
    ```wio
    if(false)
    {
        ## We couldn't get here
    }
    if(true)
    {
        ## We came here
    }
    
    ```
6.  __Functions:__ Functions are reusable blocks of code that perform a specific task. They help organize logic, avoid repetition, and make code easier to understand and maintain.
    - __Defining a Function:__ Functions are defined using the func keyword followed by a name and optional parameters:
    ```wio
    func FunctionName() 
    {
	    ## Code goes here
    }
    ```
    -  __Function Parameters:__ Function parameters can be declared with different type keywords. These determine the expected type of the argument passed to the function. var cannot be used for array, dict, or func values. These types must use their own keyword.
        - __Examples:__
        ```wio
        func SayHello(var name) { }           ## Accepts string, int, float, etc.
        func Process(array items) { }         ## Accepts only arrays
        func Configure(dict settings) { }     ## Accepts only dicts
        func Run(func callback) { }           ## Accepts only functions
        ```
        - __Reference Parameters with ref:__ Any of the above parameter types can be passed by reference using ref. This allows the function to modify the original value passed in. Note: The ref keyword can be used on both sides of the type definition.
        ```wio
        func Swap(ref var a, var ref b #* same  as ref var b *#) 
        {
	        var temp = a;
            a = b;
            b = temp;
        }        
        func Append(array ref items, var x) 
        {
        	items.append(x);
        }
        ```
        - __Invalid Example:__
        ```wio
        func Test(var x) {}
        Test([1, 2, 3]);  ## Error: 'var' does not accept array
        ```
    - __Calling a Function:__ Once defined, a function can be called simply by using its name: If the function expects parameters, the call must match the number of arguments and their basic types.
    ```wio
    Greet([1, 'c', 3]); ## Output: [1, 'c', 3]
    ```
    - __The return Statement:__ The return statement is used to exit a function and optionally send a value back to the caller. If a function does not use return, it returns null by default. Once return is executed, the function immediately exits.
    ```wio
    func Add(var a, var b) 
    {
	    return a + b;
    }

    func DoSomething() 
    {
	    Print("This will print");
	    return;
	    Print("This will NOT print");
    }
    ```
    - __Function Declaration vs Definition:__ In this language, a function can be either declared or defined—or both.
        - __Function Declaration:__ A declaration tells the interpreter/compiler that a function exists and what its parameters are, but it does not include the actual implementation (body) yet. Useful when defining the function later in the code. Allows calling the function before its actual implementation appears.
        ```wio
        func Foo(var x, var y);  ## This is a declaration
        ```
        - __Function Definition:__ A definition includes the full implementation of a function—both its signature and its body. You must define a function before calling it, unless it was previously declared. A function can only be defined once.
        ```wio
        func Foo(var x, var y) 
        {
        	Print("Function body goes here");
        }
        ```
        - Example:
        ```wio
        func Foo(var x, var y);  ## Declaration

        Foo(1, 2);               ## OK: declared earlier
        
        func Foo(var x, var y)   ## Definition
        { 
        	Print("Called");
        }
        
        ```