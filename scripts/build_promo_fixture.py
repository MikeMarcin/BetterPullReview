#!/usr/bin/env python3
"""Writes the constructed pull request that promotional captures show.

BPR-1676. The website, README and store pages show the application on a pull
request someone might really review: a small game engine moving its frame
pacing from a sleep loop to the display's vblank. The code changes in place
(rewritten lines beside their old versions, not only added blocks), so the
comparison's link map, intraline emphasis and Line details all have something
to show, and eight review threads sit on the changed lines with Conventional
Comments labels, replies, a resolved thread and posts you have not read.

The viewer is `sam`, reviewing `mira`'s pull request, so the threads say
"you" for sam's own posts. Every name, repository and line here is made up:
`bpr-demo/lantern` names no real repository, nothing was fetched, and none of
the source is built or run.

Usage:
    build_promo_fixture.py <construction.json>
        Writes the construction and, beside it, read_state.json.
    build_promo_fixture.py --threads <contract directory>
        Replaces the served tree's empty review threads with the promo threads
        and makes sam the viewer.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_conventional_comments_fixture as conventional  # noqa: E402
import build_nested_folders_fixture as nested  # noqa: E402

# The commits build_recorded_pull_request_pack.py builds from these files.
BASE_OID = "c8d078714e30667585ceffdf7f4c20d9131cd4a2"
HEAD_OID = "069cbf66992b253bf0bd73ed3e46e4a8019a0dc8"

OWNER = "bpr-demo"
REPOSITORY = "lantern"
NUMBER = 512
AUTHOR = "mira"
VIEWER = "sam"
AUTHORS = {
    "mira": "U_promo_mira",
    "sam": "U_promo_sam",
    "devon": "U_promo_devon",
    "anand": "U_promo_anand",
    "lucia": "U_promo_lucia",
}

TITLE = "Pace frames to the display's vblank instead of sleeping"
BODY = """Frame pacing slept until a deadline and spun for the last 1.5 ms. On high-refresh displays that drifted against vblank, and we dropped a frame every few seconds.

This paces to the swap chain's vblank instead:

- `frame_pacer` asks the swap chain for the next vblank and waits on it.
- The target rate is clamped to the display's refresh rate.
- `renderer::draw` gets a present deadline, so it can skip optional passes when a frame runs late.

On the 144 Hz test rig, the ten-minute soak test went from 212 dropped frames to 3.
"""


def text(source: str) -> str:
    """Source written with four-space indents, served with tabs."""
    return "".join(line.replace("    ", "\t") + "\n" for line in source.strip("\n").split("\n"))


# (path, base, head). A base of None is an added file. Sorted by path.
FILES: tuple[tuple[str, str | None, str], ...] = (
    (
        "docs/rendering.md",
        text("""
# Rendering

## Frame pacing

`frame_pacer` keeps frames evenly spaced. It sleeps until shortly before the
next frame is due, then spins for the last 1.5 ms, because sleeping is coarse
on every desktop operating system.

Set the target with `Settings.TargetHz`. The default is 60.
"""),
        text("""
# Rendering

## Frame pacing

`frame_pacer` starts each frame on the display's vblank. It asks the swap
chain when the next vblank is due and waits on it, so frames stay locked to
the display however long a frame takes to build.

Set the target with `Settings.TargetHz`. The default is 60, and the target
never exceeds the display's refresh rate.

`present_deadline()` is the last moment a frame can be submitted and still
make its vblank. The renderer skips optional passes after it.
"""),
    ),
    (
        "src/app/game_loop.cpp",
        text("""
#include "app/game_loop.h"

#include "render/frame_pacer.h"
#include "render/renderer.h"

namespace lantern::app {
    void game_loop::run()
    {
        auto pacer = render::frame_pacer{ Clock, Settings.TargetHz };
        while (!Window.should_close()) {
            pacer.wait_for_next_frame();
            Window.poll_events();
            World.update(pacer.target_rate());
            Renderer.draw(World);
            SwapChain.present();
        }
        Log.info("dropped {} frames", pacer.frames_dropped());
    }
} // namespace lantern::app
"""),
        text("""
#include "app/game_loop.h"

#include "render/frame_pacer.h"
#include "render/renderer.h"

namespace lantern::app {
    void game_loop::run()
    {
        auto pacer = render::frame_pacer{ Clock, SwapChain, Settings.TargetHz };
        while (!Window.should_close()) {
            pacer.begin_frame();
            Window.poll_events();
            World.update(1.0 / pacer.target_rate());
            Renderer.draw(World, pacer.present_deadline());
            SwapChain.present();
        }
        Log.info("dropped {} of {} frames", pacer.frames_dropped(), FrameCount);
    }
} // namespace lantern::app
"""),
    ),
    (
        "src/render/frame_pacer.cpp",
        text("""
#include "render/frame_pacer.h"

#include <algorithm>
#include <thread>

namespace lantern::render {
    namespace {
        // Sleeping is coarse on every desktop OS; wake this early and spin.
        constexpr auto SpinMargin = std::chrono::microseconds{ 1500 };

        auto period_for(const double aTargetHz) -> std::chrono::nanoseconds
        {
            return std::chrono::nanoseconds{ static_cast<std::int64_t>(1e9 / aTargetHz) };
        }
    } // namespace

    frame_pacer::frame_pacer(core::clock& aClock, const double aTargetHz)
        : Clock{ aClock }
        , FramePeriod{ period_for(aTargetHz) }
        , NextFrame{ aClock.now() + FramePeriod }
    {
    }

    void frame_pacer::wait_for_next_frame()
    {
        const auto now = Clock.now();
        if (now > NextFrame + FramePeriod) {
            // We missed at least one whole frame; start counting from now.
            FramesDropped += static_cast<std::uint64_t>((now - NextFrame) / FramePeriod);
            NextFrame = now;
        }

        const auto wake = NextFrame - SpinMargin;
        if (now < wake) {
            std::this_thread::sleep_for(wake - now);
        }
        while (Clock.now() < NextFrame) {
            std::this_thread::yield();
        }
        NextFrame += FramePeriod;
    }

    void frame_pacer::set_target_rate(const double aTargetHz)
    {
        FramePeriod = period_for(aTargetHz);
    }

    auto frame_pacer::target_rate() const noexcept -> double
    {
        return 1e9 / static_cast<double>(FramePeriod.count());
    }

    auto frame_pacer::frames_dropped() const noexcept -> std::uint64_t
    {
        return FramesDropped;
    }
} // namespace lantern::render
"""),
        text("""
#include "render/frame_pacer.h"

#include "render/swap_chain.h"

#include <algorithm>
#include <cmath>

namespace lantern::render {
    namespace {
        // Leave this much of the frame for the GPU to finish before the flip.
        constexpr auto PresentMargin = std::chrono::microseconds{ 2000 };

        auto period_for(const double aTargetHz) -> std::chrono::nanoseconds
        {
            return std::chrono::nanoseconds{ std::llround(1e9 / aTargetHz) };
        }
    } // namespace

    frame_pacer::frame_pacer(core::clock& aClock, swap_chain& aSwapChain, const double aTargetHz)
        : Clock{ aClock }
        , SwapChain{ aSwapChain }
        , FramePeriod{ period_for(std::min(aTargetHz, aSwapChain.refresh_rate())) }
        , NextFrame{ aSwapChain.next_vblank() }
    {
    }

    void frame_pacer::begin_frame()
    {
        const auto vblank = SwapChain.next_vblank();
        if (vblank > NextFrame + FramePeriod / 2) {
            // The display moved on without us: count every refresh we missed.
            FramesDropped += static_cast<std::uint64_t>((vblank - NextFrame) / FramePeriod);
        }
        NextFrame = std::max(NextFrame, vblank);

        SwapChain.wait_until(NextFrame - FramePeriod);
        NextFrame += FramePeriod;
    }

    auto frame_pacer::present_deadline() const noexcept -> core::clock::time_point
    {
        return NextFrame - PresentMargin;
    }

    void frame_pacer::set_target_rate(const double aTargetHz)
    {
        FramePeriod = period_for(std::min(aTargetHz, SwapChain.refresh_rate()));
    }

    auto frame_pacer::target_rate() const noexcept -> double
    {
        return 1e9 / static_cast<double>(FramePeriod.count());
    }

    auto frame_pacer::frames_dropped() const noexcept -> std::uint64_t
    {
        return FramesDropped;
    }
} // namespace lantern::render
"""),
    ),
    (
        "src/render/frame_pacer.h",
        text("""
#pragma once

#include "core/clock.h"

#include <chrono>
#include <cstdint>

namespace lantern::render {
    /// Spaces frames out so the game runs at a steady rate.
    class frame_pacer
    {
    public:
        explicit frame_pacer(core::clock& aClock, double aTargetHz = 60.0);

        /// Blocks until the next frame should start.
        void wait_for_next_frame();

        void set_target_rate(double aTargetHz);
        [[nodiscard]] auto target_rate() const noexcept -> double;
        [[nodiscard]] auto frames_dropped() const noexcept -> std::uint64_t;

    private:
        core::clock&             Clock;
        std::chrono::nanoseconds FramePeriod;
        core::clock::time_point  NextFrame;
        std::uint64_t            FramesDropped = 0U;
    };
} // namespace lantern::render
"""),
        text("""
#pragma once

#include "core/clock.h"

#include <chrono>
#include <cstdint>

namespace lantern::render {
    class swap_chain;

    /// Starts each frame on the display's vblank at a steady rate.
    class frame_pacer
    {
    public:
        frame_pacer(core::clock& aClock, swap_chain& aSwapChain, double aTargetHz = 60.0);

        /// Blocks until the vblank this frame should start on.
        void begin_frame();

        /// The last moment this frame can be submitted and still make its vblank.
        [[nodiscard]] auto present_deadline() const noexcept -> core::clock::time_point;

        void set_target_rate(double aTargetHz);
        [[nodiscard]] auto target_rate() const noexcept -> double;
        [[nodiscard]] auto frames_dropped() const noexcept -> std::uint64_t;

    private:
        core::clock&             Clock;
        swap_chain&              SwapChain;
        std::chrono::nanoseconds FramePeriod;
        core::clock::time_point  NextFrame;
        std::uint64_t            FramesDropped = 0U;
    };
} // namespace lantern::render
"""),
    ),
    (
        "src/render/swap_chain.h",
        text("""
#pragma once

#include <cstdint>

namespace lantern::render {
    struct swap_chain_desc
    {
        std::uint32_t Width       = 1280U;
        std::uint32_t Height      = 720U;
        std::uint32_t BufferCount = 2U;
        bool          VSync       = true;
    };

    class swap_chain
    {
    public:
        explicit swap_chain(const swap_chain_desc& aDesc);

        void resize(std::uint32_t aWidth, std::uint32_t aHeight);
        void present();

    private:
        swap_chain_desc Desc;
    };
} // namespace lantern::render
"""),
        text("""
#pragma once

#include "core/clock.h"

#include <cstdint>

namespace lantern::render {
    struct swap_chain_desc
    {
        std::uint32_t Width       = 1280U;
        std::uint32_t Height      = 720U;
        std::uint32_t BufferCount = 3U;
    };

    class swap_chain
    {
    public:
        explicit swap_chain(const swap_chain_desc& aDesc);

        void resize(std::uint32_t aWidth, std::uint32_t aHeight);
        void present();

        /// The display's refresh rate in hertz, as the driver reports it.
        [[nodiscard]] auto refresh_rate() const noexcept -> double;
        /// When the next vblank is due on the clock frames are paced with.
        [[nodiscard]] auto next_vblank() const -> core::clock::time_point;
        /// Blocks until the vblank at or after aTime.
        void wait_until(core::clock::time_point aTime);

    private:
        swap_chain_desc Desc;
    };
} // namespace lantern::render
"""),
    ),
    (
        "tests/render/frame_pacer_tests.cpp",
        None,
        text("""
#include "render/frame_pacer.h"

#include "testing/fake_clock.h"
#include "testing/fake_swap_chain.h"

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

namespace lantern::render {
    TEST_CASE("frames start on the display's vblank")
    {
        auto clock = testing::fake_clock{ };
        auto swaps = testing::fake_swap_chain{ clock, 144.0 };
        auto pacer = frame_pacer{ clock, swaps, 60.0 };

        pacer.begin_frame();
        CHECK(swaps.waits().back() == swaps.vblank(0));
        CHECK(pacer.frames_dropped() == 0U);
    }

    TEST_CASE("a late frame counts every refresh it missed")
    {
        auto clock = testing::fake_clock{ };
        auto swaps = testing::fake_swap_chain{ clock, 60.0 };
        auto pacer = frame_pacer{ clock, swaps };

        pacer.begin_frame();
        clock.advance(50ms);
        pacer.begin_frame();
        CHECK(pacer.frames_dropped() == 2U);
    }

    TEST_CASE("the target never exceeds the refresh rate")
    {
        auto clock = testing::fake_clock{ };
        auto swaps = testing::fake_swap_chain{ clock, 60.0 };
        auto pacer = frame_pacer{ clock, swaps, 240.0 };

        CHECK(pacer.target_rate() == 60.0);
    }
} // namespace lantern::render
"""),
    ),
)

# (thread, path, line text, resolved, [(author, body, new), ...]). The line is
# the first head line of the file containing the text. Posts are oldest
# first; posts marked new are each thread's last ones.
THREADS: tuple[tuple[str, str, str, bool, tuple[tuple[str, str, bool], ...]], ...] = (
    (
        "wait", "src/render/frame_pacer.cpp", "SwapChain.wait_until(NextFrame - FramePeriod);", False, (
            ("sam", "issue (blocking): This waits on the vblank a whole period before the frame is due. On the 144 Hz rig with a 60 Hz target that is two refreshes early, so begin_frame returns before the previous frame has been shown.", False),
            ("mira", "You're right. I was thinking of the period as one refresh. It should wait on the vblank at NextFrame and let wait_until round up to the grid.", True),
            ("devon", "+1. The soak test on the 144 Hz rig shows the same thing: frame times alternate 6.9 ms and 9.7 ms.", True),
        ),
    ),
    (
        "half", "src/render/frame_pacer.cpp", "if (vblank > NextFrame + FramePeriod / 2) {", False, (
            ("devon", "question: Why half a period? The old code only counted a drop after a whole period.", False),
            ("mira", "The display flips on the vblank either way, so a frame that lands 0.6 of a period late has already missed its refresh. Half a period keeps ordinary jitter from counting.", False),
            ("sam", "praise: That matches what the frame graph shows. Worth a line in the comment above?", False),
            ("mira", "Done in the next push.", True),
        ),
    ),
    (
        "clamp", "src/render/frame_pacer.cpp", ", FramePeriod{ period_for(std::min(aTargetHz, aSwapChain.refresh_rate())) }", True, (
            ("anand", "suggestion: Clamp inside period_for, so the constructor and set_target_rate can't drift apart.", False),
            ("mira", "Good call. Moved.", False),
        ),
    ),
    (
        "dt", "src/app/game_loop.cpp", "World.update(1.0 / pacer.target_rate());", False, (
            ("lucia", "nitpick (non-blocking): Pass the measured frame time rather than the target. With the target, a dropped frame slows the simulation down instead of catching up.", False),
            ("mira", "Agreed, but the fixed step is what the physics tests assume. I'd rather change that in its own pull request.", False),
            ("lucia", "Fair. I'll open an issue for it.", True),
        ),
    ),
    (
        "count", "src/app/game_loop.cpp", 'Log.info("dropped {} of {} frames", pacer.frames_dropped(), FrameCount);', True, (
            ("sam", "chore: Nothing in this loop increments FrameCount, so this always logs \"of 0 frames\".", False),
            ("mira", "Oops. Fixed.", False),
        ),
    ),
    (
        "buffers", "src/render/swap_chain.h", "std::uint32_t BufferCount = 3U;", False, (
            ("anand", "question (non-blocking): Triple buffering adds a frame of latency. Is it meant to be the default?", False),
            ("mira", "Yes. With the present deadline the renderer can fall behind by one frame without a stall, and the extra buffer is what absorbs it. The latency setting still drops it to two.", False),
        ),
    ),
    (
        "deadline", "docs/rendering.md", "`present_deadline()` is the last moment a frame can be submitted and still", False, (
            ("lucia", "polish: Say what \"optional passes\" are, or link the list in renderer.md.", True),
        ),
    ),
    (
        "tests", "tests/render/frame_pacer_tests.cpp", "TEST_CASE(\"a late frame counts every refresh it missed\")", False, (
            ("devon", "praise: The fake swap chain makes these run in a millisecond. Could we add one for a 144 Hz display with a 60 Hz target?", False),
            ("mira", "Yes, adding it with the fix for Sam's thread.", True),
        ),
    ),
)


def head_line(path: str, needle: str) -> int:
    head = next(entry[2] for entry in FILES if entry[0] == path)
    for number, line in enumerate(head.split("\n"), start=1):
        if needle in line:
            return number
    raise ValueError(f"{path}: no line contains {needle!r}")


def construction() -> dict:
    entries = []
    for path, base, head in FILES:
        diff, additions, deletions = nested.patch(base, head)
        entry = {
            "additions": additions,
            "changes": additions + deletions,
            "deletions": deletions,
            "filename": path,
            "head_content_base64": nested.encode(head),
            "patch": diff,
            "sha": nested.blob_sha(head),
            "status": "added" if base is None else "modified",
        }
        if base is not None:
            entry["base_content_base64"] = nested.encode(base)
        entries.append(entry)
    return {
        "commits": [{
            "author": {"date": "2020-01-01T00:00:00Z", "name": AUTHOR},
            "message": TITLE,
            "parents": [BASE_OID],
            "sha": HEAD_OID,
        }],
        "constructed": "Synthetic BPR-1676 fixture written by scripts/build_promo_fixture.py; not a recording",
        "files": entries,
        "pull_request": {
            "additions": sum(entry["additions"] for entry in entries),
            "author": {"login": AUTHOR},
            "base": {"ref": "main", "sha": BASE_OID},
            "body": BODY,
            "changed_files": len(entries),
            "commits": 1,
            "deletions": sum(entry["deletions"] for entry in entries),
            "head": {"ref": "vblank-pacing", "sha": HEAD_OID},
            "number": NUMBER,
            "state": "open",
            "title": TITLE,
        },
        "repository": {"name": REPOSITORY, "owner": OWNER},
    }


def comment_id(thread: str, index: int) -> str:
    return f"PRRC_promo_{thread}_{index + 1}"


def thread_node(identity: str, path: str, needle: str, resolved: bool, posts: tuple) -> dict:
    line = head_line(path, needle)
    comments = []
    for index, (author, body, _new) in enumerate(posts):
        comments.append({
            "id": comment_id(identity, index),
            "bodyText": body,
            "path": path,
            "line": line,
            "originalLine": line,
            "outdated": False,
            "author": {"login": author, "id": AUTHORS[author]},
            "replyTo": None if index == 0 else {"id": comment_id(identity, 0)},
        })
    return {
        "id": f"PRRT_promo_{identity}",
        "isResolved": resolved,
        "isOutdated": False,
        "viewerCanResolve": not resolved,
        "viewerCanUnresolve": resolved,
        "subjectType": "LINE",
        "path": path,
        "diffSide": "RIGHT",
        "line": line,
        "originalLine": line,
        "comments": {
            "totalCount": len(comments),
            "nodes": comments,
            "pageInfo": {"hasNextPage": False, "endCursor": conventional.cursor(comments[-1]["id"])},
        },
    }


def read_state() -> dict:
    """Per thread, the last post seen: the one before its first new post."""
    threads = {}
    for identity, _path, _needle, _resolved, posts in THREADS:
        fresh = [index for index, post in enumerate(posts) if post[2]]
        if fresh and fresh != list(range(fresh[0], len(posts))):
            raise ValueError(f"{identity}: new posts are not the thread's last")
        seen = (fresh[0] if fresh else len(posts)) - 1
        threads[f"review-thread:PRRT_promo_{identity}"] = None if seen < 0 else f"review-comment:{comment_id(identity, seen)}"
    return {"threads": threads}


def write(path: Path, document: dict) -> None:
    path.write_text(json.dumps(document, indent="\t", sort_keys=True) + "\n", encoding="utf-8", newline="")


def write_threads(contract: Path) -> int:
    threads_path = contract / "responses/review_threads.json"
    document = json.loads(threads_path.read_text(encoding="utf-8"))
    nodes = [thread_node(*thread) for thread in THREADS]
    document["data"]["repository"]["pullRequest"]["reviewThreads"] = {
        "nodes": nodes,
        "pageInfo": {"endCursor": conventional.cursor(nodes[-1]["id"]), "hasNextPage": False},
        "totalCount": len(nodes),
    }
    write(threads_path, document)
    # The contract builder signs in as the pull request's author; the promo
    # reviews someone else's pull request.
    write(contract / "responses/viewer.json", {"id": 1, "login": VIEWER, "name": VIEWER, "node_id": AUTHORS[VIEWER]})
    posts = sum(len(node["comments"]["nodes"]) for node in nodes)
    print(f"PROMO_THREADS_OK:{len(nodes)}:{posts}")
    return 0


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--threads":
        return write_threads(Path(sys.argv[2]))
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    document = construction()
    write(output, document)
    write(output.parent / "read_state.json", read_state())
    print(f"CONSTRUCTION_OK:{len(document['files'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
