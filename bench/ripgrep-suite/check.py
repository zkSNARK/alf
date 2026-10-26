#!/usr/bin/env python3
"""Before timing: alf must print byte for byte what rg and GNU grep print on each benchmark.

Case-insensitive (i'...') and whole-word (w'...') strings follow Unicode (simple case folding,
Unicode word characters), like rg -i / -w and GNU grep -i / -w in a UTF-8 locale.  GNU grep's
ASCII answer (LC_ALL=C) is shown next to them for comparison."""
import hashlib, os, subprocess, sys

data = sys.argv[1]
en = os.path.join(data, "subtitles", "en.sample.txt")
ru = os.path.join(data, "subtitles", "ru.txt")
NAMES_EN = ["Sherlock Holmes", "John Watson", "Irene Adler", "Inspector Lestrade", "Professor Moriarty"]
NAMES_RU = ["Шерлок Холмс", "Джон Уотсон", "Ирен Адлер", "инспектор Лестрейд", "профессор Мориарти"]


UTF8 = ["env", "LC_ALL=en_US.UTF-8"]  # GNU grep with Unicode word characters


def alt(names, prefix=""):
    return " | ".join("%s'%s'" % (prefix, n) for n in names)


# name: (corpus, alf expression, commands that must print the same bytes, commands only reported)
cases = {
    "subtitles_en_literal": (en, alt(NAMES_EN[:1]), [["rg", NAMES_EN[0], en], ["grep", NAMES_EN[0], en]], []),
    "subtitles_en_alternate": (en, alt(NAMES_EN), [["rg", "|".join(NAMES_EN), en], ["grep", "-E", "|".join(NAMES_EN), en]], []),
    "subtitles_ru_literal": (ru, alt(NAMES_RU[:1]), [["rg", NAMES_RU[0], ru], ["grep", NAMES_RU[0], ru]], []),
    "subtitles_ru_alternate": (ru, alt(NAMES_RU), [["rg", "|".join(NAMES_RU), ru], ["grep", "-E", "|".join(NAMES_RU), ru]], []),
    "subtitles_en_literal_casei": (en, alt(NAMES_EN[:1], "i"), [["rg", "-i", NAMES_EN[0], en], UTF8 + ["grep", "-i", NAMES_EN[0], en]],
                                   [["grep", "-i", NAMES_EN[0], en]]),
    "subtitles_en_alternate_casei": (en, alt(NAMES_EN, "i"), [["rg", "-i", "|".join(NAMES_EN), en], UTF8 + ["grep", "-E", "-i", "|".join(NAMES_EN), en]],
                                     [["grep", "-E", "-i", "|".join(NAMES_EN), en]]),
    "subtitles_ru_literal_casei": (ru, alt(NAMES_RU[:1], "i"), [["rg", "-i", NAMES_RU[0], ru], UTF8 + ["grep", "-i", NAMES_RU[0], ru]],
                                   [["grep", "-i", NAMES_RU[0], ru]]),
    "subtitles_ru_alternate_casei": (ru, alt(NAMES_RU, "i"), [["rg", "-i", "|".join(NAMES_RU), ru], UTF8 + ["grep", "-E", "-i", "|".join(NAMES_RU), ru]],
                                     [["grep", "-E", "-i", "|".join(NAMES_RU), ru]]),
    "subtitles_en_literal_word": (en, alt(NAMES_EN[:1], "w"), [["rg", "-w", NAMES_EN[0], en], UTF8 + ["grep", "-w", NAMES_EN[0], en]],
                                  [["grep", "-w", NAMES_EN[0], en]]),
    "subtitles_ru_literal_word": (ru, alt(NAMES_RU[:1], "w"), [["rg", "-w", NAMES_RU[0], ru], UTF8 + ["grep", "-w", NAMES_RU[0], ru]],
                                  [["grep", "-w", NAMES_RU[0], ru]]),
}
env = dict(os.environ, LC_ALL="C")


def digest(cmd):
    out = subprocess.run(cmd, env=env, capture_output=True, check=False).stdout
    return hashlib.sha256(out).hexdigest(), out.count(b"\n")


failed = False
for name, (corpus, expr, same, report) in cases.items():
    mine = [digest(["alf", "-i", corpus, "-a", expr]), digest(["alf", "--threads", "1", "-i", corpus, "-a", expr])]
    refs = [digest(c) for c in same]
    ok = len({d for d, _ in mine + refs}) == 1
    failed |= not ok
    extra = "".join("; %s (LC_ALL=C): %d lines" % (" ".join(c[:2]), digest(c)[1]) if c[0] == "grep" else "; %s: %d lines" % (" ".join(c[:2]), digest(c)[1]) for c in report)
    print("%-30s %s  %d lines%s" % (name, "same output" if ok else "DIFFERENT %s %s" % (mine, refs), refs[0][1], extra))
sys.exit(1 if failed else 0)
