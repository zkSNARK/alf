[![Build Status](https://travis-ci.com/zkSNARK/alf.svg?branch=master)](https://travis-ci.com/zkSNARK/alf)

https://zksnark.github.io/alf/

# alf
Algebraic Line Filter - alf is a command line tool which allows users to filter logs or other files by creating sets of requirements which can be specified in an algebraic form.

Alf parses your input and then applies it to either a file that you specify or whatever you pipe in, and then sends the output to stdout or to a file based on command line options.

## What is the problem this solves?
Sometimes you want to filter the lines in a file based on a set of required substrings.  A specific use case, might be you want all log entries in a file related to plugin 410000.  So you might want to grep for 410000 in that log file.  

That works fine until you want to start doing multiple elements with conditions like AND and OR.  Grep or some of its sister filtering tools may or may not have some modes that you can use to add AND and OR conditions to your searches, but they are ceraintly not easy to use, and they are not capable of easily doing nestings.

For example, if you want to say something like ...

give me all the lines in file in.log that contain 'hello' AND 'world' but only contain one of ('jack' AND 'jill'), ('bob' AND 'bill'), or ('tim', 'tam')

... this would be very hard to say in grep.

Also, complex nestings such as...

('a' & !'b') | ('a' & (('b' | 'c') & 'd'))

...might or might not be impossible in grep.

In alf, they are easy, and you can in fact type eactly "('a' & !'b') | ('a' & (('b' | 'c') & 'd'))", and alf will parse it and filter your file on it.  


## Modality
Alf works in 2 modes.  
1. Basic mode - non-algebraic.
  - Most people who want to filter logs only need to specifiy a few positive and negative requirements.
  - Accepts quoted or non-quoted tokens.
  - Only 2 key symbols : '-' and '+'. 
  This mode has its own parser, which is far more simple than the algebraic mode described above.  You can call alf like follows...
  
  alf -i in.log + a b c - d e
  
  The above would filter the file in.log and would output lines which contain ('a' OR 'b' OR 'c') AND (NOT 'd' or NOT 'e').
  
  The parser looks at the '+' and '-' symbols begin a group.  Groups continue until either the end of the arguments, or until the next '+' or '-'.  In this mode, requirements inside of a group are considered to have an implicit 'OR' between them, but groups are considered to have an implict AND between them.  Because of this rule, it is easy to specify a bunch of AND requirements by simply placing a '+' or a '-' between everything.
  
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
   
   ... but otherwise, the above symbols are parsed specially regardless of whether there is a space between them or not.  So for example. '(hello)' is parsed the same as '( hello )', and both are parsed internally as vector with approximately the follong structure ...
   
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
  
 
## Examples 
