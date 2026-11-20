#!/usr/bin/env python3
"""Rewrite a design written before DS-18 the way DS-18 reads it: types as the code writes them.

    tools/migrate_types.py [--language cpp|python] [--dry-run] <design dir or .ds file> ...

What it does, in place, file by file:

- deletes every `extern module X { ... }` block: a name nobody designs is text, so nothing outside the
  design is declared any more;
- rewrites the neutral types in attributes, methods, functions and locals as the code writes them (an
  interface's operations are left as they are: a command line or a protocol has no code types):
  python  X<A, B> -> X[A, B], optional<T> -> T | None, union<A, B> -> A | B, tuple<variadic<T>> ->
          tuple[T, ...], string -> str, none -> None, path -> Path, a qualifier cut (plan::Plan -> Plan: the code
          imports the name, and ds finds it in the same package), a parameter `x: variadic<T>` -> `*x: T`;
  cpp     string and path -> std::string, none -> void, list<T> -> std::vector<T>, set<T> -> std::set<T>,
          map<K, V> -> std::map<K, V>, optional<T> -> std::optional<T>;
- writes `many` or `optional` in front of an attribute whose type held a class of the design through a
  collection or an optional, which the type no longer says to ds.

Each line's language is the one given with --language, else the language of the box it is written in as
`ds export` reads it (the `language:` field, filled down from the box around), else cpp.
It prints what it changed and the attributes it gave a multiplicity; check the result with `ds check`
and argos: the code is the reference, and a rewrite is a guess where the design said less than the code.
Not done for you: a box whose language is C++ but whose types were written in Python's names (str,
dict<K, V>) keeps them; an extern block's comments, and a package it leaves empty, stay behind.
"""

import argparse
import bisect
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

NAME = r"[A-Za-z_][A-Za-z0-9_]*"
EXTERN = re.compile(r"^\s*(?:[+\-#]\s*)?extern\s+module\s+" + NAME + r"\s*\{")
MEMBER = re.compile(r"^(\s*[+\-#]\s*(?:(?:owns|refs|shares|many|optional|abstract|static|const)\s+)*)([~*\w]+)(\s*(?:<[^>()]*>)?\s*)(\(|:)")
FUNCTION = re.compile(r"^(\s*(?:[+\-#]\s*)?function\s+[~\w]+\s*(?:<[^>()]*>)?\s*)\(")
LOCAL = re.compile(r"^(\s*(?:local|participant)\s+" + NAME + r"\s*:\s*)")
MANY_PY = ("list[", "set[", "frozenset[", "dict[", "tuple[", "deque[", "Sequence[", "Iterator[", "Iterable[", "Mapping[")
MANY_CPP = ("std::vector<", "std::set<", "std::map<", "std::unordered_map<", "std::unordered_set<", "std::list<", "std::deque<",
            "std::array<", "std::multimap<")


def split_top(text):
    """`a, map<b, c>` -> [`a`, ` map<b, c>`]: commas at the top level only."""
    out, depth, current = [], 0, ""
    for ch in text:
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(current)
            current = ""
        else:
            current += ch
    out.append(current)
    return out


def generic(text):
    """`X<A, B>` -> (`X`, [`A`, `B`]); None for anything else."""
    text = text.strip()
    at = text.find("<")
    if at < 0 or not text.endswith(">"):
        return None
    return text[:at].strip(), [a.strip() for a in split_top(text[at + 1:-1])]


def variadic(text):
    found = generic(text)
    return found[1][0] if found and found[0] == "variadic" and len(found[1]) == 1 else None


def python(text, ambiguous=frozenset()):
    """`ambiguous`: the names two classes of the design share, whose qualifier stays (ds would find neither bare)."""
    text = text.strip()
    found = generic(text)
    if found is None:
        word = text.rsplit("::", 1)[-1]
        if word in ambiguous:
            return text
        return {"string": "str", "none": "None", "path": "Path"}.get(word, word)
    head, raw = found
    args = [f"{python(v, ambiguous)}, ..." if (v := variadic(a)) else python(a, ambiguous) for a in raw]
    if head == "optional" and len(args) == 1:
        return f"{args[0]} | None"
    if head == "union":
        return " | ".join(args)
    return f"{head.rsplit('::', 1)[-1]}[{', '.join(args)}]"


def cpp(text):
    text = text.strip()
    text = re.sub(r"(?<![:\w])string\b", "std::string", text)
    text = re.sub(r"(?<![:\w])none\b", "void", text)
    text = re.sub(r"(?<![:\w])path\b(?!\s*:)", "std::string", text)
    for neutral, written in (("list", "std::vector"), ("set", "std::set"), ("map", "std::map"), ("optional", "std::optional")):
        text = re.sub(r"(?<![:\w])" + neutral + r"<", written + "<", text)
    return text


def type_end(s, i):
    """Where a type that starts at i ends: balanced, at a top-level , ) = { // newline, ` fails` or a lone `:`."""
    depth, j = 0, i
    while j < len(s):
        ch = s[j]
        if ch in "<([":
            depth += 1
        elif ch in ">)]":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (ch in ",={" or s.startswith("//", j) or s.startswith(" fails", j) or
                             (ch == ":" and not s.startswith("::", j) and not s.startswith("::", j - 1))):
            break
        j += 1
    while j > i and s[j - 1] == " ":
        j -= 1
    return j


def languages(ds, workspace):
    """{file: ([line, ...], [language, ...], {member line, ...})}: the language of the box each entity is written
    in, by its line, and the lines where a member or a function starts (a line of prose never does)."""
    try:
        out = subprocess.run([ds, "export", str(workspace)], capture_output=True, text=True, check=False).stdout
        entities = json.loads(out)["entities"]
    except (OSError, ValueError, KeyError):
        return {}
    by_id = {e["id"]: e for e in entities}

    def language_of(entity):
        seen = set()
        while entity is not None and entity["id"] not in seen:
            seen.add(entity["id"])
            if entity["kind"] in ("module", "package") and entity.get("fields", {}).get("language"):
                return entity["fields"]["language"]
            entity = by_id.get(entity.get("scope", ""))
        return "cpp"

    found, members = {}, {}
    for e in entities:
        where = e.get("location") or {}
        if not where.get("file"):
            continue
        path = Path(where["file"])  # as ds writes it: absolute, or from here, or from the workspace
        path = path if path.is_absolute() or path.exists() else Path(workspace) / path
        path = path.resolve()
        found.setdefault(str(path), []).append((where["line"], language_of(e)))
        if e["kind"] in ("attribute", "method", "function", "operator"):
            members.setdefault(str(path), set()).add(where["line"])
    return {f: ([l for l, _ in sorted(v)], [g for _, g in sorted(v)], members.get(f, set())) for f, v in found.items()}


class Migration:
    def __init__(self, language, designed, lines=None, ambiguous=frozenset()):
        self.language = language
        self.lines = lines  # ([line, ...], [language, ...]) of the file, from ds; None: the language given
        self.designed = designed
        self.ambiguous = ambiguous
        self.edits = 0
        self.multiplicities = []

    def at(self, number):
        """The language of the box line `number` is written in."""
        if self.lines and self.lines[0]:
            i = bisect.bisect_right(self.lines[0], number) - 1
            if i >= 0:
                self.language = self.lines[1][i]
        self.convert = (lambda text: python(text, self.ambiguous)) if self.language == "python" else cpp

    def type_at(self, text, start):
        end = type_end(text, start)
        old = text[start:end]
        new = self.convert(old)
        if new != old.strip():
            self.edits += 1
        return text[:start] + new + text[end:], start + len(new)

    def parameters(self, text, start):
        """Rewrite the parameters from `start` (just inside the parenthesis) to the closing one, or to the end of
        the line when the list goes on below. Returns the text and where the list closed (-1: still open)."""
        depth, close = 0, -1
        for k in range(start, len(text)):
            if text[k] in "<([":
                depth += 1
            elif text[k] in ">)]":
                if depth == 0:
                    close = k
                    break
                depth -= 1
        stop = close if close >= 0 else len(text.split("//", 1)[0].rstrip())
        inner = text[start:stop]
        parts = []
        for p in split_top(inner):
            if ":" in p and p.strip():
                name, _, written = p.partition(":")
                lead = name[:len(name) - len(name.lstrip())]
                each = variadic(written.strip()) if self.language == "python" else None
                if each is not None:
                    self.edits += 1
                    parts.append(f"{lead}*{name.strip()}: {self.convert(each)}")
                    continue
                new = self.convert(written)
                if new != written.strip():
                    self.edits += 1
                parts.append(f"{name}: {new}")
            else:
                parts.append(p)
        joined = ",".join(parts)
        text = text[:start] + joined + text[stop:]
        return text, (start + len(joined) if close >= 0 else -1)

    def returns(self, text, after):
        arrow = text.find("->", after)
        if arrow < 0:
            return text
        start = arrow + 2
        while start < len(text) and text[start] == " ":
            start += 1
        return self.type_at(text, start)[0]

    def multiplicity(self, line, member):
        """`- owns items: list[Item]` -> `- owns many items: list[Item]` when the type holds a class of the design."""
        m = MEMBER.match(line)
        if not m or m.group(4) != ":" or re.search(r"\b(many|optional)\s", m.group(1)):
            return line
        written = line[m.end():].split("//", 1)[0].split("=", 1)[0]
        names = set(re.findall(NAME, written))
        if not names & self.designed:
            return line
        stripped = written.strip()
        if stripped.endswith("| None") or stripped.startswith(("Optional[", "std::optional<")):
            word = "optional"
        elif stripped.startswith(MANY_PY + MANY_CPP) and not (stripped.startswith("tuple[") and not stripped.endswith(", ...]")):
            word = "many"
        else:
            return line
        self.multiplicities.append(f"{member}: {word} {stripped}")
        return line[:m.end(1)] + word + " " + line[m.end(1):]

    def file(self, text, path):
        out, open_list, skip = [], False, 0
        for number, line in enumerate(text.split("\n"), start=1):
            self.at(number)
            if skip:
                code = line.split("//", 1)[0]
                skip += code.count("{") - code.count("}")
                continue
            if EXTERN.match(line):
                code = line.split("//", 1)[0]
                skip = code.count("{") - code.count("}")
                continue
            code, sep, comment = line.partition("//")
            if open_list:  # a parameter list that goes on from the line above
                code, close = self.parameters(code, 0)
                open_list = close < 0
                if not open_list:
                    code = self.returns(code, close)
            elif (m := LOCAL.match(code)):
                code = self.type_at(code, len(m.group(1)))[0]
            elif self.lines and number not in self.lines[2]:
                pass  # ds says no member starts here: prose that looks like one
            elif (f := FUNCTION.match(code)) or (m := MEMBER.match(code)):
                if f or m.group(4) == "(":
                    start = f.end() if f else m.end()
                    code, close = self.parameters(code, start)
                    open_list = close < 0
                    if not open_list:
                        code = self.returns(code, close)
                else:
                    start = m.end()
                    while start < len(code) and code[start] == " ":
                        start += 1
                    code = self.type_at(code, start)[0]
                    code = self.multiplicity(code, f"{path}:{m.group(2)}")
            out.append(code + sep + comment)
        return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--language", choices=["cpp", "python"])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--ds", default=shutil.which("ds"), help="the ds that tells each box's language (default: ds on PATH)")
    args = parser.parse_args()
    files = []
    for p in args.paths:
        files += sorted(p.rglob("*.ds")) if p.is_dir() else [p]
    designed, seen = set(), {}  # the classes the design declares itself, not those of its extern modules
    for f in files:
        depth = 0
        for line in f.read_text().split("\n"):
            code = line.split("//", 1)[0]
            if depth or EXTERN.match(line):
                depth += code.count("{") - code.count("}")
                continue
            declared = re.match(r"\s*(?:[+\-#]\s*)?(?:class|dataclass|enum)\s+(" + NAME + r")", code)
            if declared:
                designed.add(declared.group(1))
                seen[declared.group(1)] = seen.get(declared.group(1), 0) + 1
    ambiguous = frozenset(n for n, k in seen.items() if k > 1)
    boxes = {}
    if not args.language and args.ds:
        for p in args.paths:
            boxes.update(languages(args.ds, p if p.is_dir() else p.parent))
    total, touched, many = 0, 0, []
    for f in files:
        text = f.read_text()
        migration = Migration(args.language or "cpp", designed, None if args.language else boxes.get(str(f.resolve())), ambiguous)
        migration.at(1)
        new = migration.file(text, f)
        if new == text:
            continue
        touched += 1
        total += migration.edits
        many += migration.multiplicities
        removed = text.count("\n") - new.count("\n")
        print(f"{f}: {migration.edits} type(s) rewritten, {removed} extern line(s) gone")
        if not args.dry_run:
            f.write_text(new)
    for line in many:
        print(f"  multiplicity: {line}")
    print(f"{touched} file(s), {total} type(s) rewritten, {len(many)} multiplicity(ies) written")


if __name__ == "__main__":
    sys.exit(main())
