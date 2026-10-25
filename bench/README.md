# Benchmarks

alf against GNU grep, BSD grep, ripgrep, GNU and BSD sed, gawk, BSD awk and mawk on five
filtering tasks over the same generated 2,000,000-line (142 MB) server log.

| Task | The question | alf expression |
| --- | --- | --- |
| substring | lines containing `ERROR` | `ERROR` |
| and-not | ERROR or WARN lines that aren't heartbeats | `('ERROR' \| 'WARN') & !'heartbeat'` |
| nested | db or cache errors, plus slow warnings that aren't retries | `('ERROR' & ('[db]' \| '[cache]')) \| ('WARN' & 'latency' & !'retry')` |
| regex | 5xx responses that took over a second | `r'code=5\d\d' & r'latency=\d{4}ms'` |
| exclude | everything except eight kinds of noise | `!'heartbeat' !'DEBUG' !'cache miss' …` |

Each tool gets the closest equivalent it has (a pipeline for grep, an awk program, a sed
script). `run.py` lists every command. Where a tool can't express a task at all, it is
recorded as unsupported (BSD grep has no `-P` for the nested task).

## Method

- **Correctness first.** Before anything is timed, every command in a task must print exactly
  the same bytes as alf. A mismatch stops the run.
- [hyperfine](https://github.com/sharkdp/hyperfine): 2 warm-up runs, then 10 timed runs per
  command, with the log in the page cache. Every command runs under `sh`.
- **Output goes to a pipe, not `/dev/null`.** GNU grep checks whether its output is
  `/dev/null` and, if so, stops at the first match, which makes it look 20× faster than it is.
- The log is generated with a fixed seed, so every machine benchmarks the same file.

## Running it

macOS (with GNU tools from Homebrew: `brew install hyperfine grep gnu-sed gawk ripgrep`):

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DALF_BUILD_TESTS=OFF
cmake --build build-release
python3 bench/gen_log.py bench/data/server.log
python3 bench/run.py --alf build-release/alf --log bench/data/server.log --out bench/results/macos.json
```

Linux, in Docker (Debian trixie: GNU grep, ripgrep, GNU sed, gawk, mawk):

```sh
docker build -f bench/Dockerfile --build-arg ALF_REV=$(git rev-parse --short HEAD) -t alf-bench .
docker run --rm -v "$PWD/bench/results:/results" alf-bench
```

`results/` holds the JSON from the runs written up at
<https://goebel-home.net/projects/alf/>: an Apple M4 Max under macOS, and the same machine
running the Linux container in a colima VM (8 vCPUs). The container numbers show the Linux
tools' relative speed; they are not a bare-metal Linux measurement.
