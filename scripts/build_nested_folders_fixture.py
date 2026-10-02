#!/usr/bin/env python3
"""Writes the constructed nested-folders pull request the product can serve.

BPR-1213. The Overview groups a pull request's files into flat folder rows, and
the owner could not judge hiding and showing files on the recorded UIforETW#74
fixture: it has one folder and no subfolder. This builds a small synthetic pull
request with several folders, folders nested inside folders, intermediate
folders that hold no file of their own, and a root file.

Nothing here was fetched. The file contents are written below, so the
`construction.json` this produces is the same bytes on every run and every
platform. It has the shape `build_recorded_pull_request_contract.py` and
`build_recorded_pull_request_pack.py` read, without the `recorded_from` and
`recorded_on` claims a recording carries. The commit identifiers are the ones
the pack builder makes from these blobs with its fixed identity and date.

Usage:
    build_nested_folders_fixture.py <construction.json>
"""

from __future__ import annotations

import base64
import difflib
import hashlib
import json
import sys
from pathlib import Path

# The commits `build_recorded_pull_request_pack.py` builds from the files below.
# Pinned rather than computed so this script needs no Git; the pack builder
# prints them as `PACK_OK:<base>:<head>:<bytes>`, and the fixture test checks
# the committed pack holds exactly these two commits.
BASE_OID = "c58160c2ffe27d7cf5e6759d78b80f023547c789"
HEAD_OID = "b20cb443428212543f5911e091a0ffeb407d23cf"

LOGIN = "fixture-author"


def lines(*text: str) -> str:
    return "".join(f"{line}\n" for line in text)


# (path, base, head). A base of None is an added file. Sorted the way Git and
# GitHub order a comparison's files, by path.
FILES: tuple[tuple[str, str | None, str], ...] = (
    (
        "README.md",
        lines("# Nested Folders", "", "A synthetic project for review fixtures.", "", "Build with `tools/build/scripts/package.py`."),
        lines("# Nested Folders", "", "A synthetic project for review fixtures.", "", "Build with `tools/build/scripts/package.py --release`.", "Run the tests from `tests/`."),
    ),
    (
        "docs/api/reference.md",
        lines("# API reference", "", "## Renderer", "", "`Renderer::draw()` draws one frame."),
        lines("# API reference", "", "## Renderer", "", "`Renderer::draw()` draws one frame.", "`Renderer::resize(width, height)` changes the target size."),
    ),
    (
        "docs/api/v2/endpoints.md",
        None,
        lines("# Version 2 endpoints", "", "- `GET /frames` lists recent frames.", "- `GET /frames/{id}` returns one frame."),
    ),
    (
        "docs/guide.md",
        lines("# Guide", "", "Open the main window and choose a scene."),
        lines("# Guide", "", "Open the main window and choose a scene.", "Press F to show the frame counter."),
    ),
    (
        "src/engine/renderer.cpp",
        lines('#include "renderer.h"', "", "void Renderer::draw()", "{", "\tclear();", "\tpresent();", "}"),
        lines('#include "renderer.h"', "", "void Renderer::draw()", "{", "\tclear();", "\tdraw_scene();", "\tpresent();", "}", "",
              "void Renderer::resize(const int aWidth, const int aHeight)", "{", "\tWidth  = aWidth;", "\tHeight = aHeight;", "}"),
    ),
    (
        "src/engine/renderer.h",
        lines("#pragma once", "", "class Renderer", "{", "public:", "\tvoid draw();", "", "private:", "\tvoid clear();", "\tvoid present();", "};"),
        lines("#pragma once", "", "class Renderer", "{", "public:", "\tvoid draw();", "\tvoid resize(int aWidth, int aHeight);", "", "private:",
              "\tvoid clear();", "\tvoid draw_scene();", "\tvoid present();", "", "\tint Width  = 0;", "\tint Height = 0;", "};"),
    ),
    (
        "src/engine/shaders/basic.frag",
        lines("#version 330 core", "in vec4 vColour;", "out vec4 fragColour;", "void main() { fragColour = vColour; }"),
        lines("#version 330 core", "in vec4 vColour;", "uniform float uExposure;", "out vec4 fragColour;", "void main() { fragColour = vColour * uExposure; }"),
    ),
    (
        "src/engine/shaders/basic.vert",
        lines("#version 330 core", "layout(location = 0) in vec3 aPosition;", "void main() { gl_Position = vec4(aPosition, 1.0); }"),
        lines("#version 330 core", "layout(location = 0) in vec3 aPosition;", "uniform mat4 uProjection;", "void main() { gl_Position = uProjection * vec4(aPosition, 1.0); }"),
    ),
    (
        "src/engine/shaders/post/bloom.frag",
        None,
        lines("#version 330 core", "uniform sampler2D uScene;", "in vec2 vUv;", "out vec4 fragColour;", "void main() { fragColour = texture(uScene, vUv) * 1.2; }"),
    ),
    (
        "src/engine/shaders/post/tonemap.frag",
        None,
        lines("#version 330 core", "uniform sampler2D uScene;", "in vec2 vUv;", "out vec4 fragColour;",
              "void main() { vec4 c = texture(uScene, vUv); fragColour = c / (c + vec4(1.0)); }"),
    ),
    (
        "src/main.cpp",
        lines('#include "engine/renderer.h"', "", "int main()", "{", "\tRenderer renderer;", "\trenderer.draw();", "\treturn 0;", "}"),
        lines('#include "engine/renderer.h"', "", "int main()", "{", "\tRenderer renderer;", "\trenderer.resize(1280, 720);", "\trenderer.draw();", "\treturn 0;", "}"),
    ),
    (
        "src/ui/widgets/button.cpp",
        lines('#include "button.h"', "", "void Button::click()", "{", "\tif (Handler) {", "\t\tHandler();", "\t}", "}"),
        lines('#include "button.h"', "", "void Button::click()", "{", "\tif (Enabled && Handler) {", "\t\tHandler();", "\t}", "}"),
    ),
    (
        "src/ui/widgets/button.h",
        lines("#pragma once", "", "#include <functional>", "", "class Button", "{", "public:", "\tvoid click();", "", "\tstd::function<void()> Handler;", "};"),
        lines("#pragma once", "", "#include <functional>", "", "class Button", "{", "public:", "\tvoid click();", "", "\tstd::function<void()> Handler;", "\tbool                  Enabled = true;", "};"),
    ),
    (
        "src/ui/widgets/slider.cpp",
        None,
        lines('#include "slider.h"', "", "void Slider::set_value(const int aValue)", "{", "\tValue = aValue < Minimum ? Minimum : (aValue > Maximum ? Maximum : aValue);", "}"),
    ),
    (
        "tests/engine/renderer_tests.cpp",
        lines('#include "engine/renderer.h"', "", "int main()", "{", "\tRenderer renderer;", "\trenderer.draw();", "\treturn 0;", "}"),
        lines('#include "engine/renderer.h"', "", "int main()", "{", "\tRenderer renderer;", "\trenderer.resize(1, 1);", "\trenderer.draw();", "\treturn 0;", "}"),
    ),
    (
        "tests/ui/widgets/button_tests.cpp",
        None,
        lines('#include "ui/widgets/button.h"', "", "int main()", "{", "\tauto clicked = false;", "\tButton button;",
              "\tbutton.Handler = [&] { clicked = true; };", "\tbutton.Enabled = false;", "\tbutton.click();", "\treturn clicked ? 1 : 0;", "}"),
    ),
    (
        "tools/build/scripts/package.py",
        lines("import sys", "", "def main():", "    print('packaging')", "    return 0", "", "sys.exit(main())"),
        lines("import sys", "", "def main():", "    release = '--release' in sys.argv", "    print('packaging', 'release' if release else 'debug')", "    return 0", "", "sys.exit(main())"),
    ),
)


def encode(text: str) -> str:
    return base64.b64encode(text.encode("utf-8")).decode("ascii")


def blob_sha(text: str) -> str:
    data = text.encode("utf-8")
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def patch(base: str | None, head: str) -> tuple[str, int, int]:
    before = [] if base is None else base.splitlines()
    after = head.splitlines()
    hunks = list(difflib.unified_diff(before, after, n=3, lineterm=""))[2:]
    additions = sum(1 for line in hunks if line.startswith("+"))
    deletions = sum(1 for line in hunks if line.startswith("-"))
    return "\n".join(hunks), additions, deletions


def construction() -> dict:
    files = []
    for path, base, head in FILES:
        text, additions, deletions = patch(base, head)
        entry = {
            "additions": additions,
            "changes": additions + deletions,
            "deletions": deletions,
            "filename": path,
            "head_content_base64": encode(head),
            "patch": text,
            "sha": blob_sha(head),
            "status": "added" if base is None else "modified",
        }
        if base is not None:
            entry["base_content_base64"] = encode(base)
        files.append(entry)
    return {
        "commits": [
            {
                "author": {"date": "2020-01-01T00:00:00Z", "name": LOGIN},
                "message": "Move renderer, shaders and widgets into nested folders",
                "parents": [BASE_OID],
                "sha": HEAD_OID,
            }
        ],
        "constructed": (
            "Synthetic BPR-1213 fixture written by "
            "scripts/build_nested_folders_fixture.py; not a recording"
        ),
        "files": files,
        "pull_request": {
            "additions": sum(entry["additions"] for entry in files),
            "author": {"login": LOGIN},
            "base": {"ref": "main", "sha": BASE_OID},
            "body": "Constructed fixture: several folders, nested subfolders and a root file.\n",
            "changed_files": len(files),
            "commits": 1,
            "deletions": sum(entry["deletions"] for entry in files),
            "head": {"ref": "nested-folders", "sha": HEAD_OID},
            "number": 21,
            "state": "open",
            "title": "Move renderer, shaders and widgets into nested folders",
        },
        "repository": {"name": "nested-folders", "owner": "bpr-fixtures"},
    }


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(construction(), indent="\t", sort_keys=True) + "\n",
        encoding="utf-8",
        newline="",
    )
    print(f"CONSTRUCTION_OK:{len(FILES)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
