[![CI](https://github.com/zkSNARK/alf/actions/workflows/ci.yml/badge.svg)](https://github.com/zkSNARK/alf/actions/workflows/ci.yml)

# alf

**Algebraic Line Filter.** alf filters the lines of a file, or whatever you pipe into it, by a
set of required (and forbidden) substrings that you write as a boolean expression.

```sh
alf -i server.log -a "('error' | 'warn') & -'heartbeat'"
```

## Why

`grep` is great until you need "lines with this and that, but not those, unless something
else." Say you want every line in `in.log` that contains `hello` and `world`, and also one of
the pairs (`jack` and `jill`), (`bob` and `bill`) or (`tim` and `tam`). With grep that's a
pipeline of several commands. With alf it's one expression:

```sh
alf -i in.log -a "'hello' 'world' (('jack' 'jill') | ('bob' 'bill') | ('tim' 'tam'))"
```

## Two ways to write a filter

### Basic mode: `+` and `-` groups

Most of the time you only need a few strings that must appear and a few that must not.

```sh
alf -i in.log + a b c - d e
```

`+` starts a group of strings that are wanted and `-` starts a group of strings that are not
wanted. Inside a group the strings are joined with **or**; the groups are joined with **and**.
The example above keeps lines that contain `a`, `b` or `c`, **and** that are missing `d` or
missing `e`.

Put a `+` or `-` in front of every string to require them all:

```sh
alf -i in.log +a +b -c -d           # a and b, and neither c nor d
find / -name pyconfig.h 2>&1 | alf -"Permission denied" -"Operation not permitted"
```

### Algebraic mode: `-a "expression"`

| Write | Means |
| --- | --- |
| `'text'` or `"text"` | the line contains `text` (quotes are needed for anything with spaces or symbols) |
| `-'text'` or `!'text'` | the line does **not** contain `text` |
| `-( … )` or `!( … )` | the group is **not** true |
| `a & b` | and |
| `a ^ b` | exclusive or |
| <code>a &#124; b</code> | or |
| `a b` | and (an `&` is implied between neighbours) |
| `( )`, `[ ]`, `{ }` | grouping; any bracket style, as long as each pair matches |

`&` binds tighter than `^`, which binds tighter than `|`, so `'a' & 'b' | 'c'` means
`('a' & 'b') | 'c'`. A negation applies to the one string or group right after it, and two
negations cancel.

The symbols `& | ^ ! - + ( ) [ ] { }` and spaces always split unquoted text, so quote any
string that contains them: `-'error-code'`, not `-error-code`.

Malformed expressions (an unbalanced bracket, an operator with nothing on one side, an empty
group) are reported before any input is read, and alf exits with status 2.

## Options

| Option | |
| --- | --- |
| `-i FILE`, `--infile FILE` | read FILE instead of standard input |
| `-o FILE`, `--outfile FILE` | write to FILE instead of standard output |
| `-a EXPR`, `--algebra EXPR` | an algebraic expression (may be combined with basic-mode groups; everything is and-ed) |

## Build

Requires CMake 3.15+ and a C++20 compiler. Tested on Linux and macOS.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # unit tests (googletest is downloaded at configure time)
```

The executable is `build/alf`; copy it somewhere on your `PATH`. Configure with
`-DALF_BUILD_TESTS=OFF` to skip the tests and the googletest download.

## How it works

1. The expression is split into tokens: strings (each marked wanted or not wanted), operators
   and brackets. A `-`/`!` in front of a bracket becomes a unary **not** token.
2. A small state machine inserts the `&` implied between neighbouring strings and groups.
3. [Shunting-yard](https://en.wikipedia.org/wiki/Shunting-yard_algorithm) converts the tokens
   to postfix.
4. Each line is checked with a stack of booleans: a string pushes whether the line contains
   it, an operator pops its operands and pushes the result.

The tests include a randomized check that compiles thousands of generated expressions and
compares alf's answer with a straightforward recursive evaluator on every combination of
matching strings.
