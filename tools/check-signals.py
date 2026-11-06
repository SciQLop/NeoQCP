#!/usr/bin/env python3
"""Guards the NeoQCP signal rules (plan M4a, call 9).

Fails when:
  - a class in a header declares two signals with one name, which makes `&Class::signal` ambiguous;
  - an `emit`/`Q_EMIT` in a `.cpp` is not inside a `qcpEmit(...)`/`deliver(...)` call and its file is
    not allow-listed below.

Usage: check-signals.py <src dir>
"""

import re
import sys
from pathlib import Path

# Direct emits that stay direct because they have no QCustomPlot to deliver through, or run on
# another thread. file -> (class, reason, number of direct emits in that file); the count is checked
# so a new direct emit cannot hide in an allowed file.
ALLOWED = {
    "theme.cpp": ("QCPTheme", "no plot", 16),
    "lane-layout.cpp": ("QCPLaneLayout", "no plot", 1),
    "async-pipeline.cpp": ("QCPAsyncPipelineBase", "other thread", 5),
}

CLASS_RE = re.compile(r"^class\s+(?:\w+_DECL\s+)?(\w+)")
ACCESS_RE = re.compile(r"^\s*(public|protected|private)(\s+slots)?\s*:")
SIGNALS_RE = re.compile(r"^\s*(signals|Q_SIGNALS)\s*:")
SIGNAL_NAME_RE = re.compile(r"(\w+)\s*\(")
EMIT_RE = re.compile(r"\b(emit|Q_EMIT)\b")
DELIVERY_RE = re.compile(r"\b(deliver|qcpEmit)\s*\(")
DEFINITION_RE = re.compile(r"(\w+)::(~?\w+)\s*\(")
LAMBDA_ASSIGN_RE = re.compile(r"(\w+)\s*=\s*\[")


def strip_comments(text: str) -> str:
    """Blanks comments and keeps newlines, so line numbers still match the source."""
    out = []
    i = 0
    state = None  # '"', "'" or "/*"
    while i < len(text):
        char = text[i]
        if state == "/*":
            if text.startswith("*/", i):
                out.append("  ")
                i += 2
                state = None
            else:
                out.append("\n" if char == "\n" else " ")
                i += 1
        elif state in ('"', "'"):
            out.append(char)
            if char == "\\" and i + 1 < len(text):
                out.append(text[i + 1])
                i += 2
                continue
            if char == state:
                state = None
            i += 1
        elif text.startswith("/*", i):
            out.append("  ")
            i += 2
            state = "/*"
        elif text.startswith("//", i):
            while i < len(text) and text[i] != "\n":
                out.append(" ")
                i += 1
        elif char in ('"', "'"):
            # A digit separator (1'000) is not a character literal.
            if char == "'" and i > 0 and text[i - 1].isdigit():
                out.append(char)
            else:
                state = char
                out.append(char)
            i += 1
        else:
            out.append(char)
            i += 1
    return "".join(out)


def duplicated(names: dict[str, list[int]], owner: str) -> list[tuple[int, str]]:
    return [
        (lines[0], f"{owner}::{name} is declared {len(lines)} times at lines {lines}")
        for name, lines in names.items()
        if len(lines) > 1
    ]


def overloaded_signals(text: str) -> list[tuple[int, str]]:
    findings = []
    owner = None
    names: dict[str, list[int]] = {}
    depth = 0
    body_started = False
    in_signals = False
    declaration = ""
    declared_at = 0
    for number, line in enumerate(text.splitlines(), start=1):
        class_match = CLASS_RE.match(line)
        if class_match:
            findings.extend(
                duplicated(names, owner)
            )  # a class whose closing brace the scan missed
            owner, names = class_match.group(1), {}
            depth = 0
            body_started = in_signals = False
            declaration = ""
        if owner is not None:
            if in_signals:
                if not declaration:
                    declared_at = number
                declaration += " " + line
                if ";" in line:
                    name_match = SIGNAL_NAME_RE.search(declaration)
                    if name_match:
                        names.setdefault(name_match.group(1), []).append(declared_at)
                    declaration = ""
            if SIGNALS_RE.match(line):
                in_signals = True
            elif ACCESS_RE.match(line):
                in_signals = False
            depth += line.count("{") - line.count("}")
            body_started |= "{" in line
            if body_started and depth <= 0:
                findings.extend(duplicated(names, owner))
                owner, names, in_signals = None, {}, False
    findings.extend(duplicated(names, owner))
    return findings


def delivery_spans(text: str) -> list[tuple[int, int]]:
    spans = []
    for match in DELIVERY_RE.finditer(text):
        open_at = match.end() - 1
        depth = 0
        for index in range(open_at, len(text)):
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
                if depth == 0:
                    spans.append((open_at, index))
                    break
    return spans


def defined_function(text: str, offset: int) -> str:
    """The `Class::method` whose body holds offset, from the nearest definition above it."""
    for line in reversed(text[:offset].splitlines()):
        matches = list(DEFINITION_RE.finditer(line))
        if matches and not line[:1].isspace():
            return f"{matches[-1].group(1)}::{matches[-1].group(2)}"
    return "?"


def delivered_by_named_lambda(text: str, offset: int) -> bool:
    """A lambda stored in a variable and handed to deliver later is delivered, not direct."""
    assigned = None
    for assigned in LAMBDA_ASSIGN_RE.finditer(text, 0, offset):
        pass
    if assigned is None:
        return False
    return (
        re.search(
            rf"\b(?:deliver|qcpEmit)\s*\([^;]*\b{assigned.group(1)}\b", text[offset:]
        )
        is not None
    )


def direct_emits(text: str) -> list[tuple[int, str]]:
    """The emit sites outside any qcpEmit/deliver call."""
    spans = delivery_spans(text)
    findings = []
    offset = 0
    for number, line in enumerate(text.splitlines(keepends=True), start=1):
        for match in EMIT_RE.finditer(line):
            at = offset + match.start()
            if any(open_at < at < close_at for open_at, close_at in spans):
                continue
            if delivered_by_named_lambda(text, at):
                continue
            name = re.match(r"\s*(\w+)", text[at + len(match.group(0)) :])
            emitted = f"{match.group(0)} {name.group(1)}" if name else match.group(0)
            findings.append((number, f"{defined_function(text, at)}: {emitted}"))
        offset += len(line)
    return findings


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: check-signals.py <src dir>", file=sys.stderr)
        return 2
    source_dir = Path(argv[1])
    findings: list[tuple[Path, int, str]] = []
    emitted: dict[str, int] = {}
    for path in sorted(source_dir.rglob("*")):
        if path.suffix == ".h":
            findings.extend(
                (path, line, message)
                for line, message in overloaded_signals(
                    strip_comments(path.read_text())
                )
            )
        elif path.suffix == ".cpp":
            sites = direct_emits(strip_comments(path.read_text()))
            if path.name in ALLOWED:
                emitted[path.name] = len(sites)
            else:
                findings.extend((path, line, message) for line, message in sites)
    for name, (owner, reason, expected) in ALLOWED.items():
        found = emitted.get(name)
        if found is not None and found != expected:
            findings.append(
                (
                    source_dir / name,
                    0,
                    f"{owner}: {found} direct emits, the allow-list has {expected} ({reason})",
                )
            )
    for path, line, message in sorted(findings):
        print(f"{path}:{line}: {message}")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
