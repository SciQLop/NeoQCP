#!/usr/bin/env python3
"""Link arguments of a static Qt (Qt for WebAssembly), one per line, for meson.

Qt's CMake reads the same .prl files: they list each library with the bundled libraries and
the resource objects it needs. A static plugin also needs its registration object
(<type>/objects-Release/<Class>_init/<Class>_init.cpp.o), which no .prl lists.

Usage: qt_static_link.py <qt prefix> <Module>... --plugins <type>/<name>...
"""
import re
import sys
from pathlib import Path


def prl_libs(prl: Path, prefix: Path) -> list[str]:
    text = prl.read_text()
    libs = re.search(r"^QMAKE_PRL_LIBS = (.*)$", text, re.M)
    if not libs:
        return [str(prl.with_suffix(".a"))]
    variables = {"QT_INSTALL_PREFIX": prefix, "QT_INSTALL_LIBS": prefix / "lib",
                 "QT_INSTALL_PLUGINS": prefix / "plugins"}
    expand = lambda word: re.sub(r"\$\$\[(\w+)\]", lambda m: str(variables[m.group(1)]), word)
    return [expand(word) for word in libs.group(1).split()]


def plugin_registration(prefix: Path, plugin: str) -> list[str]:
    kind, name = plugin.split("/")
    inits = sorted((prefix / "plugins" / kind / "objects-Release").glob("*_init/*_init.cpp.o"))
    return [str(o) for o in inits if plugin_class_matches(o.parent.name, name)]


def plugin_class_matches(init_dir: str, name: str) -> bool:
    # QGifPlugin_init <-> qgif, QWasmIntegrationPlugin_init <-> qwasm, QSvgIconPlugin_init <-> qsvgicon
    known = {"qwasm": "QWasmIntegrationPlugin", "qsvgicon": "QSvgIconPlugin"}
    expected = known.get(name, "Q" + name[1:].capitalize() + "Plugin")
    return init_dir.lower() == (expected + "_init").lower()


def main(argv: list[str]) -> None:
    prefix = Path(argv[1]).resolve()
    split = argv.index("--plugins") if "--plugins" in argv else len(argv)
    modules, plugins = argv[2:split], argv[split + 1:]
    args = [o for p in plugins for o in plugin_registration(prefix, p)]
    for p in plugins:   # some plugin .prl files leave out their own archive
        kind, name = p.split("/")
        prl = prefix / "plugins" / kind / f"lib{name}.prl"
        args += [str(prl.with_suffix(".a"))] + prl_libs(prl, prefix)
    for m in modules:
        args += prl_libs(prefix / "lib" / f"libQt6{m}.prl", prefix)
    seen = set()
    print("\n".join(a for a in args if not (a in seen or seen.add(a))))


if __name__ == "__main__":
    main(sys.argv)
