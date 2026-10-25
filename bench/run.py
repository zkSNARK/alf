#!/usr/bin/env python3
"""Benchmark alf against grep, ripgrep, sed and awk on the same filtering tasks.

    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DALF_BUILD_TESTS=OFF
    cmake --build build-release
    python3 bench/gen_log.py bench/data/server.log
    python3 bench/run.py --alf build-release/alf --log bench/data/server.log --out bench/results/RESULT.json

Needs hyperfine. Tools that aren't installed are skipped. Before anything is timed, every
command in a task must print byte-for-byte the same output as alf; a mismatch stops the run.
"""

import argparse
import hashlib
import json
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# name -> (executable to look for, label)
TOOLS = {
    "alf": ("{alf}", "alf"),
    "gnu-grep": ("ggrep", "GNU grep"),
    "bsd-grep": ("/usr/bin/grep", "BSD grep"),
    "ripgrep": ("rg", "ripgrep"),
    "gnu-sed": ("gsed", "GNU sed"),
    "bsd-sed": ("/usr/bin/sed", "BSD sed"),
    "gawk": ("gawk", "GNU awk"),
    "bsd-awk": ("/usr/bin/awk", "BSD awk"),
}

# On Linux the GNU tools have their plain names, there are no BSD ones, and mawk (Debian's
# default awk) joins in.
if platform.system() == "Linux":
    TOOLS.update({"gnu-grep": ("grep", "GNU grep"), "gnu-sed": ("sed", "GNU sed"), "gawk": ("gawk", "GNU awk"),
                  "mawk": ("mawk", "mawk")})
    for k in ("bsd-grep", "bsd-sed", "bsd-awk"):
        TOOLS.pop(k)

NOISE = ["heartbeat", "DEBUG", "cache miss", "token refreshed", "code=200", "code=304", "[scheduler]", "job "]
NOISE_RE = [n.replace("[", r"\[").replace("]", r"\]") for n in NOISE]

# Each task: what it asks, then one command per tool ({alf}, {gnu-grep}, ... are executables, {log} the
# file). Every command runs under sh, so pipelines are allowed.
# None = the tool has no way to express it as a filter.
TASKS = [
    {
        "id": "substring",
        "title": "One string",
        "question": "lines containing ERROR",
        "alf_expr": "ERROR",
        "cmds": {
            "alf": "{alf} -i {log} ERROR",
            "gnu-grep": "{gnu-grep} -F ERROR {log}",
            "bsd-grep": "{bsd-grep} -F ERROR {log}",
            "ripgrep": "{ripgrep} -N --no-filename -F ERROR {log}",
            "gnu-sed": "{gnu-sed} -n '/ERROR/p' {log}",
            "bsd-sed": "{bsd-sed} -n '/ERROR/p' {log}",
            "gawk": "{gawk} '/ERROR/' {log}",
            "bsd-awk": "{bsd-awk} '/ERROR/' {log}",
        },
    },
    {
        "id": "and-not",
        "title": "Either, but not",
        "question": "ERROR or WARN lines that aren't heartbeats",
        "alf_expr": "('ERROR' | 'WARN') & !'heartbeat'",
        "cmds": {
            "alf": "{alf} -i {log} -a \"('ERROR' | 'WARN') & !'heartbeat'\"",
            "gnu-grep": "{gnu-grep} -E 'ERROR|WARN' {log} | {gnu-grep} -v heartbeat",
            "bsd-grep": "{bsd-grep} -E 'ERROR|WARN' {log} | {bsd-grep} -v heartbeat",
            "ripgrep": "{ripgrep} -N --no-filename -e ERROR -e WARN {log} | {ripgrep} -v heartbeat",
            "gnu-sed": "{gnu-sed} -n '/heartbeat/d;/ERROR\\|WARN/p' {log}",
            "bsd-sed": "{bsd-sed} -E -n '/heartbeat/d;/ERROR|WARN/p' {log}",
            "gawk": "{gawk} '(/ERROR/ || /WARN/) && !/heartbeat/' {log}",
            "bsd-awk": "{bsd-awk} '(/ERROR/ || /WARN/) && !/heartbeat/' {log}",
        },
    },
    {
        "id": "nested",
        "title": "Nested",
        "question": "db or cache errors, plus slow warnings that aren't retries",
        "alf_expr": "('ERROR' & ('[db]' | '[cache]')) | ('WARN' & 'latency' & !'retry')",
        "cmds": {
            "alf": "{alf} -i {log} -a \"('ERROR' & ('[db]' | '[cache]')) | ('WARN' & 'latency' & !'retry')\"",
            # grep has no and/or across conditions; PCRE lookaheads are the only single-pass way
            "gnu-grep": "{gnu-grep} -P '^(?=.*ERROR)(?=.*\\[(db|cache)\\])|^(?=.*WARN)(?=.*latency)(?!.*retry)' {log}",
            "bsd-grep": None,
            "ripgrep": "{ripgrep} -N --no-filename -P '^(?=.*ERROR)(?=.*\\[(db|cache)\\])|^(?=.*WARN)(?=.*latency)(?!.*retry)' {log}",
            "gnu-sed": "{gnu-sed} -n -e '/ERROR/{/\\[db\\]\\|\\[cache\\]/p;d}' -e '/WARN/{/retry/d;/latency/p}' {log}",
            "bsd-sed": "{bsd-sed} -E -n -e '/ERROR/{/\\[(db|cache)\\]/p;d;}' -e '/WARN/{/retry/d;/latency/p;}' {log}",
            "gawk": "{gawk} '(/ERROR/ && (/\\[db\\]/ || /\\[cache\\]/)) || (/WARN/ && /latency/ && !/retry/)' {log}",
            "bsd-awk": "{bsd-awk} '(/ERROR/ && (/\\[db\\]/ || /\\[cache\\]/)) || (/WARN/ && /latency/ && !/retry/)' {log}",
        },
    },
    {
        "id": "regex",
        "title": "Two regexes",
        "question": "5xx responses that took over a second",
        "alf_expr": "r'code=5\\d\\d' & r'latency=\\d{4}ms'",
        "cmds": {
            "alf": "{alf} -i {log} -a \"r'code=5\\d\\d' & r'latency=\\d{4}ms'\"",
            "gnu-grep": "{gnu-grep} -E 'code=5[0-9]{2}' {log} | {gnu-grep} -E 'latency=[0-9]{4}ms'",
            "bsd-grep": "{bsd-grep} -E 'code=5[0-9]{2}' {log} | {bsd-grep} -E 'latency=[0-9]{4}ms'",
            "ripgrep": "{ripgrep} -N --no-filename 'code=5[0-9]{2}' {log} | {ripgrep} 'latency=[0-9]{4}ms'",
            "gnu-sed": "{gnu-sed} -E -n '/code=5[0-9]{2}/{/latency=[0-9]{4}ms/p}' {log}",
            "bsd-sed": "{bsd-sed} -E -n '/code=5[0-9]{2}/{/latency=[0-9]{4}ms/p;}' {log}",
            "gawk": "{gawk} '/code=5[0-9][0-9]/ && /latency=[0-9][0-9][0-9][0-9]ms/' {log}",
            "bsd-awk": "{bsd-awk} '/code=5[0-9][0-9]/ && /latency=[0-9][0-9][0-9][0-9]ms/' {log}",
        },
    },
    {
        "id": "exclude",
        "title": "Drop the noise",
        "question": "everything except eight kinds of noise",
        "alf_expr": " ".join(f"!'{n}'" for n in NOISE),
        "cmds": {
            "alf": "{alf} -i {log} -a " + shlex.quote(" ".join(f"!'{n}'" for n in NOISE)),
            "gnu-grep": "{gnu-grep} -v -F " + " ".join(f"-e {shlex.quote(n)}" for n in NOISE) + " {log}",
            "bsd-grep": "{bsd-grep} -v -F " + " ".join(f"-e {shlex.quote(n)}" for n in NOISE) + " {log}",
            "ripgrep": "{ripgrep} -N --no-filename -v -F " + " ".join(f"-e {shlex.quote(n)}" for n in NOISE) + " {log}",
            "gnu-sed": "{gnu-sed} " + " ".join(f"-e {shlex.quote(f'/{n}/d')}" for n in NOISE_RE) + " {log}",
            "bsd-sed": "{bsd-sed} " + " ".join(f"-e {shlex.quote(f'/{n}/d')}" for n in NOISE_RE) + " {log}",
            "gawk": "{gawk} " + shlex.quote(" && ".join(f"!/{n}/" for n in NOISE_RE)) + " {log}",
            "bsd-awk": "{bsd-awk} " + shlex.quote(" && ".join(f"!/{n}/" for n in NOISE_RE)) + " {log}",
        },
    },
]


for _task in TASKS:  # mawk takes the same programs as the other awks
    _awk = _task["cmds"]["gawk"]
    _task["cmds"]["mawk"] = _awk.replace("{gawk}", "{mawk}") if _awk else None


def tool_version(name: str, exe: str) -> str:
    if name == "alf":  # alf has no --version; it would read "--version" as a filter and wait on stdin
        rev = os.environ.get("ALF_REV") or subprocess.run(["git", "-C", str(Path(__file__).parent), "rev-parse", "--short", "HEAD"],
                             capture_output=True, text=True).stdout.strip()
        return f"alf {rev}" if rev else "alf"
    flag = {"bsd-grep": "-V", "mawk": "-W version"}.get(name, "--version")
    if name in ("bsd-sed", "bsd-awk"):
        return "macOS " + platform.mac_ver()[0] if name == "bsd-sed" else subprocess.run(
            [exe, "--version"], capture_output=True, text=True).stdout.strip()
    out = subprocess.run([exe, *flag.split()], capture_output=True, text=True, stdin=subprocess.DEVNULL)
    return (out.stdout or out.stderr).splitlines()[0].strip() if (out.stdout or out.stderr) else "?"


def expand(cmd: str, log: str, exes: dict) -> str:
    """Fill in {log} and {tool} placeholders. Not str.format: sed programs have their own { }."""
    cmd = cmd.replace("{log}", shlex.quote(log))
    for name, exe in exes.items():
        cmd = cmd.replace("{" + name + "}", shlex.quote(exe))
    return cmd


def timing(r: dict) -> dict:
    """One hyperfine result -> seconds (+ peak memory), for both the 1.x and 2.x JSON layouts."""
    if "summary" in r:  # hyperfine 2.x
        w, m = r["summary"]["time_wall_clock"], r["summary"].get("memory_peak_resident")
        peak = m["max"] if m else None
    else:
        w = r
        peak = max(r["memory_usage_byte"]) if r.get("memory_usage_byte") else None
    return {"mean_s": w["mean"], "stddev_s": w["stddev"], "median_s": w["median"],
            "min_s": w["min"], "max_s": w["max"], "peak_rss_bytes": peak}


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--alf", required=True)
    ap.add_argument("--log", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--runs", type=int, default=10)
    ap.add_argument("--only", help="comma-separated task ids")
    args = ap.parse_args()
    if not shutil.which("hyperfine"):
        sys.exit("hyperfine is required")

    exes = {}
    for name, (exe, _) in TOOLS.items():
        exe = exe.replace("{alf}", str(Path(args.alf).resolve()))
        if shutil.which(exe):
            exes[name] = exe
        else:
            print(f"skipping {name}: {exe} not found")
    log = str(Path(args.log).resolve())
    size = Path(log).stat().st_size
    with open(log, "rb") as f:
        lines = sum(1 for _ in f)

    result = {
        "machine": {"system": platform.platform(), "cpu": platform.processor() or platform.machine(),
                    "python": platform.python_version()},
        "log": {"bytes": size, "lines": lines},
        "tools": {n: {"label": TOOLS[n][1], "version": tool_version(n, e)} for n, e in exes.items()},
        "tasks": [],
    }
    if platform.system() == "Darwin":
        cpu = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True).stdout.strip()
        result["machine"]["cpu"] = cpu

    tasks = [t for t in TASKS if not args.only or t["id"] in args.only.split(",")]
    for task in tasks:
        cmds = {n: expand(c, log, exes) for n, c in task["cmds"].items() if c is not None and n in exes}
        # correctness first: every command must print exactly what alf prints
        digests = {}
        for n, c in cmds.items():
            out = subprocess.run(["sh", "-c", c], capture_output=True, check=False)
            if out.returncode not in (0, 1):
                sys.exit(f"{task['id']}/{n} failed ({out.returncode}): {out.stderr.decode()[:300]}")
            digests[n] = (hashlib.sha256(out.stdout).hexdigest(), out.stdout.count(b"\n"))
        want = digests["alf"]
        bad = [n for n, d in digests.items() if d != want]
        if bad:
            sys.exit(f"{task['id']}: output differs from alf for {bad}: { {n: digests[n][1] for n in digests} } lines")
        print(f"\n== {task['id']}: {task['question']} -> {want[1]} lines, all {len(cmds)} tools agree")

        with tempfile.NamedTemporaryFile(suffix=".json") as tmp:
            hf = ["hyperfine", "--warmup", "2", "--runs", str(args.runs), "--style", "basic", "--export-json", tmp.name, "--shell", "sh",
                  # pipe, not /dev/null: GNU grep notices /dev/null and stops at the first match
                  "--output", "pipe"]
            for n, c in cmds.items():
                hf += ["--command-name", n, c]
            subprocess.run(hf, check=True)
            runs = json.load(open(tmp.name))["results"]
        task_out = {k: task[k] for k in ("id", "title", "question", "alf_expr")}
        task_out["matching_lines"] = want[1]
        task_out["unsupported"] = [n for n, c in task["cmds"].items() if c is None and n in exes]
        # hyperfine reports results in the order the commands were given
        # record what a reader would type, not this machine's paths
        shown = {n: expand(c, "server.log", {**exes, "alf": "alf"}) for n, c in task["cmds"].items() if n in cmds}
        task_out["results"] = [{"tool": n, "command": shown[n], **timing(r)} for (n, _), r in zip(cmds.items(), runs)]
        result["tasks"].append(task_out)

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(result, indent=2) + "\n")
    print(f"\nwrote {args.out}")


if __name__ == "__main__":
    main()
