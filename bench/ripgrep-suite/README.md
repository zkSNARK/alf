# ripgrep's benchmarks, with alf

`bench/run.py` measures tasks written by alf's author, on a log generated for it. This directory
runs someone else's: ripgrep's own benchmark suite
([`benchsuite/benchsuite`](https://github.com/BurntSushi/ripgrep/tree/master/benchsuite)),
pinned to the commit used for ripgrep's published run of 2022-12-16, on the OpenSubtitles 2016
corpora it downloads.

`alf.patch` adds alf to the single-file benchmarks it can answer and changes nothing else: same
commands for the other tools, same corpora, same runner, same CSV. alf runs twice, with its
default (one thread per performance core) and with `--threads 1`, because ripgrep and grep
search a single file on one core.

| Benchmark | Pattern | alf expression |
| --- | --- | --- |
| `subtitles_en_literal` | `Sherlock Holmes` | `'Sherlock Holmes'` |
| `subtitles_en_alternate` | five names joined by `\|` | `'Sherlock Holmes' \| 'John Watson' \| …` |
| `subtitles_ru_literal` | `Шерлок Холмс` | `'Шерлок Холмс'` |
| `subtitles_ru_alternate` | five Russian names joined by `\|` | `'Шерлок Холмс' \| …` |

The rest of the suite needs features alf doesn't have: case-insensitive strings ([#9](https://github.com/zkSNARK/alf/issues/9)), whole
words ([#12](https://github.com/zkSNARK/alf/issues/12)), real regexes (`std::regex` would be measured, and replacing it is planned), or
searching a directory tree ([#7](https://github.com/zkSNARK/alf/issues/7)).

Before anything is timed, `check.py` requires alf (both ways), `rg` and GNU `grep` to print
exactly the same bytes for each benchmark.

## Running it

macOS (`brew install ripgrep grep the_silver_searcher ugrep`; `grep` must be GNU grep):

```sh
PATH="$PWD/build-release:$(brew --prefix grep)/libexec/gnubin:$PATH" \
  bench/ripgrep-suite/run.sh bench/data/rgsuite bench/results/rg-suite-macos
```

Linux, in Docker (Debian trixie: ripgrep, GNU grep, ag, ugrep). The corpora go in a volume, on
the VM's own disk:

```sh
docker build -f bench/ripgrep-suite/Dockerfile -t alf-rg-suite .
docker volume create alf-rg-data
docker run --rm -v alf-rg-data:/data -v "$PWD/bench/results:/results" alf-rg-suite
```

The first run downloads about 3.4 GB and unpacks about 12 GB. Only `subtitles/en.sample.txt`
(the first 55 million lines of `en.txt`) and `subtitles/ru.txt` are searched; if both are already
in the data directory, the download is skipped.

Each run writes `<prefix>.csv` (every sample: benchmark, command, duration, line count, environment),
`<prefix>.summary` (the suite's own table: mean ± standard deviation, line counts, `*` for the
fastest) and `<prefix>.versions`.
