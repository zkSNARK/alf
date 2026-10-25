#!/usr/bin/env python3
"""End-to-end check of the alf command line: the same bytes from every input and output path.

    python3 bench/cli_check.py ALF [LOG]

Generates a log if none is given, then for several expressions checks that alf prints exactly
the same output reading a file, reading a pipe, writing to a file with -o, on one thread and on
all of them, and with the scalar SIMD backend (ALF may be several words, e.g. "node build/alf.js").
"""
import hashlib, os, shlex, subprocess, sys, tempfile

alf = shlex.split(sys.argv[1])
here = os.path.dirname(os.path.abspath(__file__))
tmp = tempfile.mkdtemp()
log = sys.argv[2] if len(sys.argv) > 2 else os.path.join(tmp, "log.txt")
if len(sys.argv) <= 2:
    subprocess.run([sys.executable, os.path.join(here, "gen_log.py"), log, "300000"], check=True)
data = open(log, "rb").read()

exprs = ["'ERROR'", "('ERROR' | 'WARN') & !'heartbeat'",
         "('ERROR' & ('[db]' | '[cache]')) | ('WARN' & 'latency' & !'retry')",
         r"r'code=5\d\d' & r'latency=\d{4}ms'",
         "!'heartbeat' !'DEBUG' !'cache miss' !'token refreshed' !'code=200' !'code=304' !'[scheduler]' !'job '"]

def run(args, stdin=None, env=None):
    r = subprocess.run(alf + args, input=stdin, capture_output=True, env=env)
    if r.returncode != 0:
        sys.exit(f"alf {args} failed ({r.returncode}): {r.stderr.decode()[:300]}")
    return r.stdout

failed = False
for e in exprs:
    out_file = os.path.join(tmp, "out.txt")
    run(["-i", log, "-o", out_file, "-a", e])
    variants = {
        "file": run(["-i", log, "-a", e]),
        "file, 1 thread": run(["--threads", "1", "-i", log, "-a", e]),
        "pipe": run(["-a", e], stdin=data),
        "-o file": open(out_file, "rb").read(),
        "scalar backend": run(["-i", log, "-a", e], env={**os.environ, "ALF_SIMD": "scalar"}),
    }
    digests = {k: hashlib.sha256(v).hexdigest()[:12] for k, v in variants.items()}
    lines = variants["file"].count(b"\n")
    if len(set(digests.values())) != 1:
        failed = True
        print(f"MISMATCH {e}: {digests}")
    else:
        print(f"ok  {lines:7} lines  {e}")
sys.exit(1 if failed else 0)
