[![Build Status](https://travis-ci.com/zkSNARK/alf.svg?branch=master)](https://travis-ci.com/zkSNARK/alf)

https://zksnark.github.io/alf/

# alf
Algebraic Line Filter - alf is a command line tool which allows users to
 filter logs or other files by creating sets of requirements which can 
 be specified in an algebraic form.

Alf parses your input and then applies it to either a file that you 
specify or whatever you pipe in, and then sends the output to stdout 
or to a file based on command line options.

## What problem does this solve?
Sometimes you want to filter the lines in a file based on a set of 
required substrings.  A specific use case, might be you want all log
 entries in a file related to plugin 410000.  So you might want to grep 
 for 410000 in that log file.  

That works fine until you want to start doing multiple elements with 
conditions like AND and OR.  Grep or some of its sister filtering tools 
may or may not have some modes that you can use to add AND and OR conditions 
to your searches, but they are ceraintly not easy to use, and they are not 
capable of easily doing nestings.

For example, if you want to say something like ...

give me all the lines in file in.log that contain 'hello' AND 'world' but 
only contain one of ('jack' AND 'jill'), ('bob' AND 'bill'), or ('tim', 'tam')

... this would be very hard to say in grep.

Also, complex nestings such as...

('a' & !'b') | ('a' & (('b' | 'c') & 'd'))

...might or might not be impossible in grep.

In alf, they are easy, and you can in fact type eactly "('a' & !'b') 
| ('a' & (('b' | 'c') & 'd'))", and alf will parse it and filter your file on it.  


## Modality
Alf works in 2 modes.  
1. Basic mode - non-algebraic.
  - Most people who want to filter logs only need to specifiy a few positive and 
    negative requirements.
  - Accepts quoted or non-quoted tokens.
  - Only 2 key symbols : '-' and '+'. 
  This mode has its own parser, which is far more simple than the algebraic mode 
  described above.  You can call alf like follows...
  
  alf -i in.log + a b c - d e
  
  The above would filter the file in.log and would output lines which contain 
  ('a' OR 'b' OR 'c') AND (NOT 'd' or NOT 'e').
  
  The parser looks at the '+' and '-' symbols begin a group.  Groups continue 
  until either the end of the arguments, or until the next '+' or '-'.  In this 
  mode, requirements inside of a group are considered to have an implicit 'OR' 
  between them, but groups are considered to have an implict AND between them.  
  Because of this rule, it is easy to specify a bunch of AND requirements by 
  simply placing a '+' or a '-' between everything.
  
  alf -i in.log +a +b -c -d
  
  ... which would be parsed as REQUIRE 'a' AND 'b' AND NOT 'c' AND NOT 'd'.
  
2.  Algebraic mode - full algebraic input as a string
  - Allows users to enter complex sets of requirements.
  - Accepts following special characters ...

  Type symbols 
  
    * '(' : alf::types::brackets::BRACKET_OPEN_PAREN
    * '{' : alf::types::brackets::BRACKET_OPEN_CURLY
    * '[' : alf::types::brackets::BRACKET_OPEN_SQUARE
    * ']' : alf::types::brackets::BRACKET_CLOSE_PAREN
    * '}' : alf::types::brackets::BRACKET_CLOSE_CURLY
    * ')' : alf::types::brackets::BRACKET_CLOSE_SQUARE
    * '&' : alf::types::operators::AND
    * '|' : alf::types::operators::OR
    
   Other
   
    * '+' : positive requirement
    * '-' : negative requirement (EG : require not found in this line)
    * '!' : negative requirement (EG : require not found in this line)
    * '\'' : single quote
    * '"' : double quote
    
   - anything between quotes is considered a substr of type...
   
    * alf::types::SUBSTR
   
   ... but otherwise, the above symbols are parsed specially regardless of 
   whether there is a space between them or not.  So for example. '(hello)' 
   is parsed the same as '( hello )', and both are parsed internally as 
   vector with approximately the follong structure ...
   
  ```json
  {
    "sequence": [
      {
        "type": "alf::types::brackets::BRACKET_OPEN_PAREN",
        "required": true
      },
      {
        "type": "alf::types::SUBSTR",
        "required": true
      },
      {
        "type": "alf::types::brackets::BRACKET_CLOSE_PAREN",
        "required": true
      }
    ]
  }
  ```
  
## Build Instructions
Currently, this project is tested only on linux, and build requires Cmake 
3.15 or higher. 

From this directory, 

    cmake -S . -B build
    cmake --build build

This will create an executable in the build directory named 'alf'.  Move it 
wherever you want.  I suggest /usr/local/bin and then call it with your algebraic
or non algebraic expressions.

## Examples 

Tired of those pesky "Permission denied", "Operation not permitted", "No such file 
or directory", or "Not a directory" messages when you run find?  Eliminate them with 
negative requirements ('-') combined with terse implied AND syntax. 

`time find / -name pyconfig.h  2>&1 | alf -"Permission denied" -"Operation not permitted" 
-"No such file or directory"`

results of the above command under time

5.95s user 113.66s system 61% cpu 3:13.27 total
0.00s user 0.00s system 0% cpu 3:13.28 total

Here is the same thing in grep ...
`time find / -name pyconfig.h  2>&1 | grep -v "Permission denied" | grep -v "Operation 
not permitted" | grep -v "No such file or directory" | grep  -v "Not a directory"`

5.81s user 110.95s system 62% cpu 3:08.24 total
0.00s user 0.00s system 0% cpu 3:08.24 total

Could you do the above with AWK?... yes.  But alf is sorta the first half of awk 
(the filter section) without 'do something with the results' section.  Just the
filter section is really convenient as a substitute for grep.

One other benefit over awk, is that this is a c++ library.  Technically, this could
give someone the ability to embed the filter section of alf into a c++ project.  

You can see how it works in the executable ... 
https://github.com/zkSNARK/alf/blob/master/src/main.cpp

```
  alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
  pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
  pack.tokens = alf::shunting_yard(std::move(pack.tokens));
  apply_filters(std::move(pack));
```

The above could be used inside of a user program by calling the parser with
a valid sequence directly.  See the tests https://github.com/zkSNARK/alf/blob/master/tests/test_algebraic_parser.cpp 
for examples of how to call the parser ... 

In the following code, we can see that you can simply pass the parser a string

```
std::vector<alf::types::TokenBase> result{ alf::parser::parse_algebraic("hello world") };
```

Currently, it would be used as follows ... 
```
  alf::parser::ArgPack pack { alf::parser::parse_arguments(argc, argv)};
  pack.tokens = alf::parser::fill_in_missing_AND_symbols(pack.tokens);
  pack.tokens = alf::shunting_yard(std::move(pack.tokens));
  if (alf::passes_filters(pack.tokens, some_std_string)) {
    // do whatever it is you do
  }
```

At a later time, the first three lines will be wrapped into a convenience function which
will give users the ability to call a single if statement with the target filters and a 
line to check.
