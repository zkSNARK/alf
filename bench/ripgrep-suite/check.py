#!/usr/bin/env python3
"""Before timing: alf must print byte for byte what rg and GNU grep print on each benchmark."""
import hashlib, os, subprocess, sys

data = sys.argv[1]
en = os.path.join(data, "subtitles", "en.sample.txt")
ru = os.path.join(data, "subtitles", "ru.txt")
cases = {
    "subtitles_en_literal": (en, ["Sherlock Holmes"]),
    "subtitles_en_alternate": (en, ["Sherlock Holmes", "John Watson", "Irene Adler", "Inspector Lestrade", "Professor Moriarty"]),
    "subtitles_ru_literal": (ru, ["Шерлок Холмс"]),
    "subtitles_ru_alternate": (ru, ["Шерлок Холмс", "Джон Уотсон", "Ирен Адлер", "инспектор Лестрейд", "профессор Мориарти"]),
}
env = dict(os.environ, LC_ALL="C")
failed = False
for name, (corpus, strings) in cases.items():
    expr = " | ".join("'%s'" % s for s in strings)
    runs = {
        "alf": ["alf", "-i", corpus, "-a", expr],
        "alf (1 thread)": ["alf", "--threads", "1", "-i", corpus, "-a", expr],
        "rg": ["rg", "|".join(strings), corpus],
        "grep": ["grep", "-E", "|".join(strings), corpus],
    }
    digests = {}
    for tool, cmd in runs.items():
        out = subprocess.run(cmd, env=env, capture_output=True, check=False).stdout
        digests[tool] = (hashlib.sha256(out).hexdigest(), out.count(b"\n"))
    same = len({d for d, _ in digests.values()}) == 1
    failed |= not same
    print("%-24s %s  %d lines" % (name, "same output" if same else "DIFFERENT: %s" % digests, digests["rg"][1]))
sys.exit(1 if failed else 0)
