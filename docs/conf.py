import re
from pathlib import Path

ROOT = Path(__file__).parent.parent

project = "NeoQCP"
author = "Alexis Jeandet"
copyright = "SciQLop contributors; QCustomPlot by Emanuel Eichhammer"
version = release = re.search(r"version\s*:\s*'([^']+)'", (ROOT / "meson.build").read_text()).group(1)

extensions = ["myst_parser", "sphinxcontrib.mermaid", "sphinx_design"]
source_suffix = {".md": "markdown"}
myst_heading_anchors = 3
myst_enable_extensions = ["colon_fence"]
myst_fence_as_directive = ["mermaid"]

# docs/ also holds internal notes (plans, specs, agent notes) that are not published.
exclude_patterns = ["_build", "_api", "plans", "specs", "superpowers", "snippets", "architecture/README.md"]

html_theme = "sphinx_rtd_theme"
html_title = f"NeoQCP {release}"
html_static_path = []
# The Doxygen reference (doxygen docs/Doxyfile writes docs/_api/api/) is served at <site>/api/.
html_extra_path = ["_api"] if (Path(__file__).parent / "_api").is_dir() else []
# Unmarked blocks are diagrams or pseudo-code; C++ blocks say so.
highlight_language = "none"
