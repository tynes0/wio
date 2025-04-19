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

    array arr[1,2,3];

    foreach(item in arr)
        Print(item); ## Output 123

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

    foreach (ch in "String")
    {
        Print(ch + ' '); ## Output: S t r i n g 
    }

    ```
    - Note: Any control or loop can also be used with a single operation instead of a block.

4. __local and global Keywords:__
    - The local keyword ensures that a global variable, regardless of its base type, remains specific to that file. That is, it cannot be accessed from other files that import that file.
    - The global keyword ensures that a variable within a scope that has entered the process behaves like a global variable and can be accessed from outside.
    - Example Usage: 
    ```wio
    local dict MyDict = {}; ## This variable is only accessible in this file.
    
    if(true)
        global array MyArray[1,2,3];
    else 
        global array MySecondArray[1,2,3];

    MyArray[1] = 4; ## This is valid
    MySecondArray[1] = 4; ## This is NOT valid
    ```