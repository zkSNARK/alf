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
| `r'pattern'` or `r"pattern"` | the regular expression matches somewhere in the line |
| `-r'pattern'` or `!r'pattern'` | the regular expression does **not** match |
| `-( … )` or `!( … )` | the group is **not** true |
| `a & b` | and |
| `a ^ b` | exclusive or |
| <code>a &#124; b</code> | or |
| `a b` | and (an `&` is implied between neighbours) |
| `( )`, `[ ]`, `{ }` | grouping; any bracket style, as long as each pair matches |

`&` binds tighter than `^`, which binds tighter than `|`, so `'a' & 'b' | 'c'` means
`('a' & 'b') | 'c'`. A negation applies to the one string or group right after it, and two
negations cancel.

### Regular expressions

Write a regex like a Python raw string: an `r` right against the opening quote.

```sh
alf -i server.log -a "r'^\[(WARN|ERROR)\]' & -r'code=2\d\d'"
alf -i build.log -a "r'warning: unused' | r\"error C\d{4}\""
```

Patterns use the ECMAScript grammar of `std::regex` and are matched with `std::regex_search`,
so a pattern matches anywhere unless you anchor it with `^` or `$`. The line ending is not part
of the line, so `r'done$'` works on files with `\n` or `\r\n` endings. There are no escapes
for the delimiting quote; to use `'` inside a pattern, delimit it with `"`, and the other way
round. An invalid pattern is an error before any input is read. Regexes are only available in
algebraic mode: in basic mode the shell has already removed the quotes.

`std::regex` is not fast: on a million-line log a regex filter takes about five times as long
as a substring filter. Prefer plain strings when they say the same thing.

### Quoting

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
| `--threads N` | filter with N threads (default: one per performance core); output order is always preserved |
| `--simd-info` | list the SIMD backends this binary has, which run on this CPU and which is in use |

Setting `ALF_SIMD=<name>` (for example `ALF_SIMD=scalar`) forces one SIMD backend; see [SIMD](#simd).

## Build

Requires CMake 3.15+ and a C++20 compiler (GCC, Clang or MSVC). CI builds and tests on Linux
(x86-64 and Arm), macOS, Windows (MSVC), RISC-V and POWER (cross-compiled, run under QEMU with
`cmake/toolchains/`), and WebAssembly (Emscripten, run under Node.js).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # unit tests (googletest is downloaded at configure time)
```

The executable is `build/alf`; copy it somewhere on your `PATH`. Configure with
`-DALF_BUILD_TESTS=OFF` to skip the tests and the googletest download.

Other targets, as CI builds them:

```sh
# Windows (MSVC): the build type is chosen at build time
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release

# RISC-V or POWER: a cross compiler (g++-riscv64-linux-gnu / g++-powerpc64le-linux-gnu) and
# qemu-user; ctest runs the tests through QEMU (-DALF_QEMU_VLEN=128 emulates narrower RVV
# vectors than the default 256 bits).
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-linux-gnu.cmake
cmake --build build && ctest --test-dir build

# WebAssembly: Emscripten; the result runs under Node.js with access to local files
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build && node build/alf.js --simd-info
```

## Performance

alf memory-maps its input, searches the whole buffer for the strings an expression requires
(one string by its rarest byte or rarest pair of bytes, many in one pass with Teddy shuffle
fingerprints), decides most lines without evaluating the whole expression, writes kept lines
without copying long runs, and splits large inputs across every performance core (`--threads N`
to change that). On a 142 MB log on an Apple M4 Max it finds one string in 7 ms (ripgrep 21,
GNU grep 33), answers a nested and/or/not query in 9 ms (GNU grep 238) and drops eight kinds of
noise in 21 ms (ripgrep 119). Pinned to one core it still beats grep and ripgrep on every
string task, and on exclude lists of up to 64 strings on macOS and Linux. Where ripgrep is still
faster on one core: regexes without fixed text, and lists of more than about 100 strings (at
256, 272 ms against 146), where ripgrep switches to Aho-Corasick and alf keeps using Teddy
([#10](https://github.com/zkSNARK/alf/issues/10)).

### ripgrep's own benchmarks

[`bench/ripgrep-suite/`](bench/ripgrep-suite/) runs ripgrep's benchmark suite, unchanged except
for added alf commands, on its 1.6 GB English and 1.7 GB Russian subtitle corpora. On the four
benchmarks alf can express (one name, or any of five, in each language), alf on one core is
faster than ripgrep on all four, on macOS (0.087 s vs 0.142 for `Sherlock Holmes`) and Linux
(0.090 vs 0.095); with every core it takes 27 to 81 ms. The rest of the suite needs
case-insensitive search, whole words, regexes or directory trees, which alf doesn't have yet.
Raw samples: [`bench/results/rg-suite-*.csv`](bench/results/).

[`bench/`](bench/) has the harness, the results and how to rerun them; the write-up with charts
is at <https://goebel-home.net/projects/alf/>.

### SIMD

The byte scanning uses vector instructions where the CPU has them: SSE2, SSSE3, AVX2 and AVX-512
on x86-64, NEON and SVE on AArch64, the V extension on RISC-V, VSX on POWER8+, and SIMD128 on
WebAssembly, with a scalar fallback. One binary contains every backend
for its architecture and picks the best at run time; `alf --simd-info` shows which, and
`ALF_SIMD=<name>` forces one. See [`libalf/src/simd/README.md`](libalf/src/simd/README.md).

### Memory-mapped input

A file given with `-i` (or redirected with `<`) is memory-mapped, and kept lines are written
straight from the mapping. If another process truncates the file while alf is reading it
(log rotation with `copytruncate`, for example), the operating system stops the process with
`SIGBUS`. To filter a log that may be truncated underneath you, pipe it in instead
(`cat app.log | alf ...`): piped input is read into alf's own buffer.

## How it works

1. The expression is split into tokens: strings and regexes (each marked wanted or not wanted,
   regexes compiled once here), operators
   and brackets. A `-`/`!` in front of a bracket becomes a unary **not** token.
2. A small state machine inserts the `&` implied between neighbouring strings and groups.
3. [Shunting-yard](https://en.wikipedia.org/wiki/Shunting-yard_algorithm) converts the tokens
   to postfix.
4. The postfix tokens become a small tree that each line is evaluated against, stopping as
   soon as the answer is known: in `'a' & 'b'`, a line without `a` is never searched for `b`.
5. Before looking at lines one by one, alf works out from the tree which strings every kept
   line must contain, and searches the whole buffer for those at once (a single string with
   `memchr` on its rarest byte, a few with a vector search for any of their rare bytes, many
   with Teddy). Only lines where something turned up are evaluated in full; for exclude lists
   like `!'a' & !'b'`, the lines in between are kept without being looked at.
6. Large inputs are split at line boundaries into one piece per thread, and the pieces are
   written out in order. Kept lines are written as views into the input, so long runs of kept
   lines are never copied.

The tests include a randomized check that compiles thousands of generated expressions and
compares alf's answer with a straightforward recursive evaluator on every combination of
matching strings.
