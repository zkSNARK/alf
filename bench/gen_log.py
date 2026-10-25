#!/usr/bin/env python3
"""Write a deterministic, server-style log for the benchmarks.

    python3 bench/gen_log.py OUT [LINES]      (default 2,000,000 lines, about 230 MB)

Same seed, same file, so results from different machines are comparable.
"""

import random
import sys

LEVELS = ["INFO"] * 70 + ["DEBUG"] * 15 + ["WARN"] * 10 + ["ERROR"] * 5
COMPONENTS = ["api", "db", "cache", "auth", "worker", "scheduler", "gateway", "billing"]
MESSAGES = [
    "GET /v1/orders/{n} code={code} latency={ms}ms",
    "POST /v1/payments code={code} latency={ms}ms user={user}",
    "heartbeat ok seq={n}",
    "connection reset by peer, retry {r}/5",
    "query took {ms}ms rows={n}",
    "cache miss key=session:{user}",
    "token refreshed for user={user}",
    "job {n} done in {ms}ms",
    "upstream timeout after {ms}ms, retry {r}/5",
    "slow request latency={ms}ms path=/v1/search",
]
CODES = [200] * 80 + [201, 204, 301, 304, 400, 401, 403, 404, 404, 429] + [500, 502, 503, 503, 504]


def main() -> None:
    out = sys.argv[1]
    lines = int(sys.argv[2]) if len(sys.argv) > 2 else 2_000_000
    rng = random.Random(20260424)
    t = 1_761_300_000
    with open(out, "w") as f:
        for i in range(lines):
            t += rng.randint(0, 3)
            msg = rng.choice(MESSAGES).format(
                n=rng.randint(1, 999_999), code=rng.choice(CODES), ms=rng.randint(1, 4000),
                user=rng.randint(1, 50_000), r=rng.randint(1, 5))
            hh, mm, ss = (t // 3600) % 24, (t // 60) % 60, t % 60
            f.write(f"2026-10-24T{hh:02}:{mm:02}:{ss:02}Z {rng.choice(LEVELS):5} "
                    f"[{rng.choice(COMPONENTS)}] {msg}\n")


if __name__ == "__main__":
    main()
