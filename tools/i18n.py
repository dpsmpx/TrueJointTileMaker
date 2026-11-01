#!/usr/bin/env python3
"""Строки интерфейса для перевода (план «Интерфейс», D4).

    tools/i18n.py missing   строки из кода, которых нет в Resources/Lang/en.txt
    tools/i18n.py unused    строки таблицы, которых в коде больше нет
    tools/i18n.py all       все строки с кириллицей из кода

Строка — литерал C++ с кириллицей; соседние литералы ("a" "b", как их
переносит clang-format) склеиваются в один. Шаблоны таблицы ({}) и куски
(~) в «unused» не попадают: их в коде целиком нет.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CATALOG = ROOT / "Resources" / "Lang" / "en.txt"
SOURCES = sorted((ROOT / "src" / "app").glob("*.cpp")) + [
    ROOT / "src" / "core" / f
    for f in ("shortcuts.cpp", "mapdoc.cpp", "mapobj.cpp", "gameset.cpp", "filters.cpp",
              "imageops.cpp", "model.cpp", "model_art.cpp", "model_doc.cpp", "model_layers.cpp",
              "anim.cpp", "palette.cpp", "vector.cpp", "project.cpp", "rasterfmt.cpp", "gif.cpp",
              "png.cpp", "zlib.cpp", "scheme.cpp")
]
CYR = re.compile("[А-Яа-яЁё]")
# Не переводятся: название языка пишется на нём самом, «пробел» — запасное
# имя клавиши для Config.txt, «сценарий» — сообщение для разработчика.
KEEP = {"Русский", "русский", "пробел", "сценарий %s: %s"}


def literals(text):
    """Литералы файла по порядку; соседние — склеены."""
    out = []
    i = 0
    n = len(text)
    cur = None  # склеиваемый литерал
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            i = text.find("\n", i)
            i = n if i < 0 else i
            continue
        if text.startswith("/*", i):
            i = text.find("*/", i + 2)
            i = n if i < 0 else i + 2
            continue
        if c == "'":
            j = i + 1
            while j < n and text[j] != "'":
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            continue
        if c == '"' and not (i > 0 and text[i - 1] == "R"):
            j = i + 1
            s = []
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    e = text[j + 1]
                    s.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(e, "\\" + e))
                    j += 2
                else:
                    s.append(text[j])
                    j += 1
            lit = "".join(s)
            cur = lit if cur is None else cur + lit
            i = j + 1
            continue
        if not c.isspace() and cur is not None:
            out.append(cur)
            cur = None
        i += 1
    if cur is not None:
        out.append(cur)
    return out


def without_help(text):
    """Справка переводится своим файлом (Resources/Lang/help.en.txt), а не
    строками: её абзацы склеиваются из строк исходника."""
    start = text.find("void Editor::showHelpScreen(")
    if start < 0:
        return text
    end = text.find("\n}\n", start)
    return text[:start] + text[end:]


def code_strings():
    seen = []
    for f in SOURCES:
        for s in literals(without_help(f.read_text(encoding="utf-8"))):
            if CYR.search(s) and s not in seen:
                seen.append(s)
    return seen


def escape(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t")


def unescape(s):
    return s.replace("\\n", "\n").replace("\\t", "\t").replace("\\\\", "\\")


def catalog():
    keys = {}
    if CATALOG.exists():
        for line in CATALOG.read_text(encoding="utf-8").splitlines():
            if not line or line.startswith("#") or "\t" not in line:
                continue
            k, v = line.split("\t", 1)
            keys[unescape(k[1:] if k.startswith("~") else k)] = v
    return keys


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "missing"
    have = catalog()
    if mode == "all":
        for s in code_strings():
            print(escape(s))
    elif mode == "missing":
        # Кусок склейки, который целиком входит в шаблон таблицы, — переведён им.
        patterns = [k for k in have if "{}" in k]
        for s in code_strings():
            if s in have or s in KEEP:
                continue
            if s.strip() and any(s.strip() in p for p in patterns):
                continue
            print(escape(s))
    elif mode == "unused":
        code = set(code_strings())
        for k in have:
            if "{}" not in k and not k.startswith("~") and k not in code:
                print(escape(k))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
