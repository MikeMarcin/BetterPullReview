# Overview description layout study

Source-only design evidence for BPR-0918 / RFC-BPR-0037 AC-01.
Inspected baseline: `4384e20982f6ed736a3845f1fd8e6f346b8a9ae3`.

## Current frontier

Investigation complete: the milestone integrator selects a dedicated expanded
description within the central Overview surface, with a fixed Return to Files
control. Native-library offscreen probes demonstrate full-body reachability and
expose a failed naive scroll-retention strategy. BPR-0928 owns implementation;
AC-01 runtime acceptance is not claimed. No production code changed and no
separate human decision gate is required for this milestone.

## Admitted text versus visible text

These are inspected source facts at the baseline, not a reproduction of the
owner's private PR or an assertion about what that server returned.

| Stage | Evidence | Meaning |
| --- | --- | --- |
| REST normalization | `src/adapters/github/src/github_read_normalizer.cpp:288`, `:302`, `:1100` | Optional `body` becomes the complete string, or empty for absent/null. No six-line substring operation. |
| Admission | `src/application/include/bpr/application/github_read_model.hpp:630`, `:715`, `:1155`; normalizer `:1388` | Default/hard maximum body is 262,144 bytes. Over-limit text returns validation failure; embedded NUL is rejected. Validation consumes bytes without truncating. Aggregate text bounds also apply. |
| Wire/page bounds | `src/adapters/github/src/github_paged_read.hpp:21`; `github_paged_read.cpp:1118`; `native_https_transport.hpp:26` | Separate default 4 MiB page/response and 64 MiB paged aggregate ceilings. These are admission limits, not viewport limits. |
| Accepted snapshot | `src/application/src/github_window_read_host.cpp:846`, `:880`, `:885` | Host validates and moves staged snapshot into accepted storage only on the matching core acceptance delta. |
| Overview projection | `src/app/src/github_application.cpp:9120`, `:9165`; `src/application/src/pull_request_overview.cpp:167` | Composition uses the accepted snapshot; projection copies `request.Body`. The general GitHub projection also copies it (`github_projection_mapping.cpp:323`). |
| Qt viewport | `src/adapters/qt/src/pull_request_overview_page.cpp:785` | Read-only, tab leaves editor, no frame, maximum height 110 logical pixels, Maximum vertical size policy. `:942` applies complete plain text; empty text is hidden at `:943`. |
| Surroundings | Same page `:250`, `:831`, `:835`, `:904` | Files receives vertical stretch; side column reserves 300 pixels; there is no page scroll area in this constructor. |
| Refresh | `src/adapters/qt/src/workbench_window.cpp:4568`; page `:942`, `:1012` | Identical whole projection is skipped, but any changed Overview projection calls `present`, which resets the text document. Clear empties body and Files. |

The native-library probe below confirms selection/copy reaches the last byte
of 203,412 admitted-style synthetic bytes despite the cap. It also confirms
scroll/selection reset after changing only the projected title. It bypasses
provider admission; the admission conclusions above are source inspection.
No server-side truncation can be excluded for an unobserved remote response.

## Alternatives and selected design

| Option | Narrow / large font | Long / very long text | Files and scroll ownership | Decision |
| --- | --- | --- | --- | --- |
| Inline expand/collapse with natural-height body and a page scroll area | More wrapping makes the document taller; side column still constrains width unless reflowed | Entire body is reachable, but a very long document puts Files thousands of lines away | A fixed Jump to Files and Collapse is essential. Disabling body vertical scrolling avoids one nesting layer, but a virtualized Files table inside a scrolling page still needs deliberate wheel ownership and height policy | Viable for a document-first Overview redesign; reject for this bounded slice because it changes both Files and page scrolling and still needs an escape control |
| User-resizable body/Files splitter | Same width pressure; larger fonts make both minimum heights compete | Gives space on demand but consumes Files space; very long bodies still scroll internally | Two sibling vertical scrolling regions, not necessarily a nested trap. Drag affordance and keyboard resize/reset need accessibility work | Reject as primary route: repeated resizing is work, and a thin splitter is less discoverable than a named reading action |
| Dedicated expanded description in the central Overview surface | Hides the secondary summary column while reading; uses the available central width and height | One editor scrolls the complete body; allocation follows the window rather than another fixed height | Fixed Return to Files restores existing Overview table/focus/selection. Existing global mode navigation remains reachable | **Recommend**: full reading area and one explicit return, with the least change to Files semantics |

“Dedicated” means a retained alternate presentation inside Overview, not a new
application mode, modal dialog, extra top-level window or browser. Use one
expanded reader with one vertical scroll owner. The summary preview and reader
must share accepted body content/identity rather than becoming divergent stores.
The existing core still decides file/mode navigation; the host performs effects;
Qt owns this local expansion, focus and viewport state.

Collapsed Overview shows a natural short-body preview and a named **Read full
description** action when content overflows its font-relative preview budget.
The preview is clearly a preview, without an active small nested scroll well.
Do not replace 110 with another fixed pixel ceiling and call that the fix.
Expanded reading fills the central available space, with **Return to Files**
always outside the text viewport. That action returns to the existing Files
table without changing its current row or review mode. Escape provides the same
local return when no more specific editor/menu interaction owns Escape. An
ordinary restore action may return focus to the expansion trigger; Return to
Files must focus Files. Opening a file continues through its existing dispatch.

An empty body has an explicit compact “No description” state and no expansion
action. A short body is fully visible without reserving a tall empty well.
Changing font or width recomputes preview overflow. Expanded text wraps in this
plain-text slice, including long tokens, and copying preserves literal code and
table text. RFC-BPR-0036 may later render Markdown through its own native body
boundary; this design neither parses Markdown nor grants link/image effects.

## Reviewable layout artifacts

These wireframes are proposed arrangements, not product screenshots. `S`, `L`
and `V` refer to the reproducible synthetic corpus below.

```text
Summary: S fits naturally; L/V show preview plus a named reading action
┌ Overview — PR title / branches ───────────────┬ Participants / My review ┐
│ ## Section 1                                 │ Start / Continue review  │
│ This synthetic description…                  │                          │
│ [Read full description]                      │                          │
├ Files ───────────────────────────────────────┤                          │
│ selected file, retained table position        │                          │
└──────────────────────────────────────────────┴──────────────────────────┘

Expanded L/V: same structure at narrow/wide widths; secondary column hidden
┌ [Return to Files]  Description — PR title ───────────────────────────────┐
│ ## Section 1                                                          ▲│
│ - [x] Unit checks pass                                                 │
│ wide code and table-like text wraps without eliding source             │
│ …                                                                     │
│ ## Section 500                                                        │
│ END OF ADMITTED BODY                                                  ▼│
└───────────────────────────────────────────────────────────────────────┘
```

The disposable Qt prototype reused the actual page/editor, temporarily hid the
Files and side widgets, removed the body maximum height and inserted a return
button. This measures the allocation achievable with existing Qt presentation
mechanics. It is not the recommended final retained-stack implementation.

## Synthetic corpus and measured evidence

Environment: macOS 26.6.2 (25G83), arm64, AppleClang 21.0.0, Qt 6.10.3 from
the repository dev dependency cache, `QT_QPA_PLATFORM=offscreen`, no forced
style/palette. Inputs were direct projected synthetic strings, not live data.
Requested page sizes were 800×800 and 1400×800 logical pixels; actual sizes
matched. Application-font probes used 13 and 20 points. Each combination ran
at scale factors 1.0, 1.25 and 2.0; reported DPR matched and logical geometry
was stable. Offscreen font fallback and `propagateSizeHints` warnings occurred.
These are deterministic target-native library/layout observations, **not**
macOS desktop appearance, trackpad, VoiceOver or full-window acceptance.

Corpus S is `Short description.\nEND OF ADMITTED BODY` (39 bytes). L repeats
the following numbered section 20 times (8,131 bytes); V repeats it 500 times
(203,412 bytes, 6,000 newline-terminated section lines). Append the same final
sentinel after all sections. ASCII makes bytes unambiguous; Unicode and the
exact 256 KiB boundary belong in the follow-up checks.

````text
## Section N

This synthetic description explains the change and its verification.
- [x] Unit checks pass
- [ ] Check the interactive workflow
```cpp
auto result = compare(old_revision, new_revision, exact_repository_identity, selected_file_identity, retained_comparison_identity);
```
| Case | Expected result |
| ---- | --------------- |
| Narrow | All admitted text remains selectable and reachable |

````

| Page width / font | Existing body | Expanded body | Font line spacing | PageDown movement, L/V |
| --- | --- | --- | --- | --- |
| 800 / 13 pt | 464×110 | 764×683 | 20 | 33 scrollbar units |
| 800 / 20 pt | 464×110 | 764×659 | 32 | 20 scrollbar units |
| 1400 / 13 pt | 1064×110 | 1364×683 | 20 | 33 scrollbar units |
| 1400 / 20 pt | 1064×110 | 1364×659 | 32 | 20 scrollbar units |

All 36 width/font/body/scale cases copied exactly the source bytes from both
the existing and expanded editor using the offscreen clipboard. The L/V end
cursor was visible: bottom 639–640 of 659 pixels at 20 pt, or 679 of 683 at
13 pt. S needed no scroll. Shift+Tab from the expanded reader focused Return
to Files; clicking it focused the restored Files widget. At 800/20 pt the
inspected raster showed the last section, wrapped wide code/table text, final
sentinel and fixed return control together. The provisional button used the
full row and the title/summary sizing was inherited; those are prototype
artifacts, not typography specifications.

For every L/V case a title-only projection update reset the body scrollbar to
zero and removed selection. A deliberately naive return/reopen that replayed
the old scrollbar integer failed equality in six of the eight L/V width/font
combinations, at every scale. Rewrapping changes scrollbar range/layout. This
is a negative result, not successful reading-position verification. Avoid
using raw scrollbar-value equality as the implementation's retention contract.

Wheel/trackpad momentum, screen-reader announcements, horizontal pan under a
future Markdown renderer, and wall-clock responsiveness were not measured.
The approximately 0.6-second probe process time includes all cases and startup;
it is not a scrolling performance measurement. Full-body selection/copy and
cursor-to-end prove reachability, not human reading comfort.

## Position and identity contract

Retain the expanded reader while its Overview summary is shown, preserving its
document, selection and logical text anchor. Do not replace unchanged body text
when unrelated title, receipt, participant or history facts change. On width,
font or scale reflow retain the first visible source-text position and its
within-line offset where representable, rather than replaying a scrollbar
integer. Files retains its own selected stable row and table position.

For a same-authority body edit, retain the reader state, clamp the prior source
position to the new document and announce “Description updated”; do not claim
the old text still exists or jump to a similarly worded passage. Restore the
new document's valid selection bounds. The focused implementation may refine
this to a retained unchanged block only if deterministically tested.

PR/account/host replacement or invalidation clears expansion, text, selection
and anchors before replacement content appears. Do not key retention by only
owner/repository/number: `pull_request_overview_projection` currently exposes
those fields (`pull_request_overview.hpp:162`) but not complete host/account
authority. Bind the existing exact invalidation lifecycle, adding a narrow
opaque presentation identity only if tests show it is necessary. Same PR refresh
retains state; returning to a previously departed PR starts afresh. No new
durable reading-position store is required for this slice.

## Bounded canonical implementation follow-up

Canonical follow-up: [[BPR-0928]], **Expand Overview descriptions into a
retained central reader**.
Own RFC-BPR-0037 AC-01 and depend on the accepted BPR-0918 decision. No
RFC-BPR-0036 implementation dependency. BPR-0919's Review entry semantics and
the existing authority/invalidation contracts remain unchanged.

Scope: Overview page/header, its body presentation and local expansion state;
workbench projection/invalidation integration only where needed for exact
identity; focused Qt and public-path fixture checks. Touch application projection
types only if exact identity cannot be obtained from the existing lifecycle.
Do not change provider limits, remote operations, Files ordering/selection
policy, review publication, Markdown parsing, conversation layout or persistence.

Acceptance checks:

1. S/empty/L/V plus valid Unicode, 262,144-byte admitted body and a refused
   262,145-byte input prove the full accepted text remains available. Refusal
   must not present a silently shortened new body. Include escaped markup,
   checklists, wide code and tables as literal input.
2. Activate Read full description by mouse and keyboard through the actual
   workbench. Reader fills available central space; no new fixed expanded
   height limit. At 800×800 and 1400×800 page sizes, 13/20 pt and 1/1.25/2x,
   fixed return controls remain reachable and final text is visible/selectable.
   Also exercise the shipped window's actual minimum and a short-height case.
3. PageUp/Down, native document-start/end shortcuts, arrows, selection, select
   all and copy work. Tab/Shift+Tab leave the reader. Wheel and trackpad route
   to the sole reader vertical viewport; record actual event/scroll movement
   and latency observations on the supported native target. No preview scroll
   trap; future wide-block horizontal scrolling must not consume vertical input.
4. Return to Files and local Escape restore summary and focus without changing
   file selection/order or emitting file activation, mode or publication intent.
   Reopening restores the reading anchor/selection. Existing file activation
   still dispatches exactly once through the established semantic route.
5. Title/receipt-only refresh preserves unchanged body, anchor and selection;
   body edits apply the explicit clamp/update announcement rule. Resize/font/
   scale changes retain the same visible source anchor within one wrapped line.
   Test mid-document anchors as well as the end, never only scrollbar integers.
6. Exact authority invalidation and PR/account/host replacement clear old text
   and local state before new content, including same repo/PR number on another
   host. Stale projections cannot revive it. Test clear followed by replacement.
7. Inherited light/dark/high-contrast palettes, visible native focus and accessible
   names/states remain usable. Collect target-native observations separately;
   no QSS, forced style, web view or host effect in the widget.

Verification strategy: extend `tests/adapters_qt/pull_request_overview_page_tests.cpp`
for deterministic layout/input/retention checks; use the real composed workbench
fixture for projection identity, Files return and Overview → Review navigation.
Reuse admission validation fixtures, adding boundary assertions only where
absent. Run focused tests and documentation gate, then target-native input and
appearance review of the implemented candidate. Existing passing tests below
cover the short-body baseline only and do not satisfy these new acceptance checks.

## Commands and outcomes

Executed from the repository root:

```sh
git rev-parse HEAD
ctest --test-dir build/dev --output-on-failure -R '^(application_pull_request_overview|qt_pull_request_overview_page)$' -j1
mktemp -d /tmp/bpr-0918-overview.XXXXXX
cmake -S /tmp/bpr-0918-overview.39SoYx -B /tmp/bpr-0918-overview.39SoYx/build2 -G Ninja -DCMAKE_PREFIX_PATH=/Users/jaeger/Documents/GitHub/BetterPullReview/build/.cpm-cache/bprqt/6.10.3/6.10.3/macos
cmake --build /tmp/bpr-0918-overview.39SoYx/build2
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.0 /tmp/bpr-0918-overview.39SoYx/build2/probe
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=1.25 /tmp/bpr-0918-overview.39SoYx/build2/probe
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2.0 /tmp/bpr-0918-overview.39SoYx/build2/probe
```

The two existing tests passed (0.52 seconds total). All probe processes exited
zero; reported `restored=0` cases are intentionally retained negative evidence,
not encoded assertion successes. The first CMake attempt set only `Qt6_DIR` and
failed by discovering incompatible Homebrew Qt tool packages. Selecting the
complete repository Qt prefix in the fresh `build2` directory resolved it.
No source rebuild was needed; the probe linked existing dev libraries, and the
inspected source baseline is recorded above. This does not certify library
reproducibility or the full shipped binary at that commit.

Final documentation validation: `python3 scripts/verify_documentation.py --root .`
returned `DOCUMENTATION_OK`. The disposable probe directory
`/tmp/bpr-0918-overview.39SoYx` was removed with non-forced `rm -r` after checking
its two owned source files; absence was verified. All probe/build/test commands
finished before handoff. No background process or temporary artifact remains.

## Remaining risks and uncertainties

The integrator selected this placement under the bound design milestone; that
decision does not convert the recommendation into runtime acceptance. Full-window
chrome consumes space beyond the isolated page, and the inherited fixed-width
summary sidebar remains outside the reading expansion's scope. Native desktop
input/appearance and Windows/Linux evidence remain unobserved. The later Markdown
renderer must preserve the same identity, selection/copy and single-scroll
ownership contract. Source anchors across edited text are explicitly best-effort
clamped positions, not a promise of semantic text tracking.

## Disposable probe reproduction source

The integrator authorized only this disposable native probe in addition to the
study. To repeat, create a fresh directory with `mktemp -d`, save these two
blocks there using the named filenames, replace the historical screenshot path
in `probe.cpp` with that exact directory, and substitute it in the build/run
commands above. Link paths intentionally identify the inspected local dev tree;
rebuild the focused test target first if its inputs have changed. Probe source
is retained here as source-only evidence; no binary, log or screenshot is tracked.

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.24)
project(overview_probe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(BPR /Users/jaeger/Documents/GitHub/BetterPullReview)
add_executable(probe probe.cpp)
target_include_directories(probe PRIVATE ${BPR}/src/adapters/qt/include ${BPR}/src/application/include ${BPR}/src/core/include)
target_link_libraries(probe PRIVATE ${BPR}/build/dev/src/adapters/qt/libbetter_pull_review_adapter_qt.a ${BPR}/build/dev/src/application/libbetter_pull_review_application.a ${BPR}/build/dev/src/core/libbetter_pull_review_core.a Qt6::Widgets "-framework AppKit")
```

### probe.cpp

```cpp
#include "bpr/qt/pull_request_overview_page.hpp"
#include <QApplication>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTreeView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QClipboard>
#include <QFontMetrics>
#include <QTextBlock>
#include <iostream>

QString body(int sections)
{
    QString result;
    for (int i = 1; i <= sections; ++i) {
        result += QString("## Section %1\n\nThis synthetic description explains the change and its verification.\n- [x] Unit checks pass\n- [ ] Check the interactive workflow\n```cpp\nauto result = compare(old_revision, new_revision, exact_repository_identity, selected_file_identity, retained_comparison_identity);\n```\n| Case | Expected result |\n| ---- | --------------- |\n| Narrow | All admitted text remains selectable and reachable |\n\n").arg(i);
    }
    return result + "END OF ADMITTED BODY";
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    std::cout << "qt=" << qVersion() << " platform=" << app.platformName().toStdString() << "\n";
    for (int width : {800, 1400}) for (int pt : {13, 20}) for (int sections : {0, 20, 500}) {
        bpr::qt::pull_request_overview_page page;
        auto font = page.font(); font.setPointSize(pt); page.setFont(font);
        page.resize(width, 800);
        bpr::application::pull_request_overview_projection projection;
        projection.Title = "Synthetic description study";
        projection.Body = (sections ? body(sections) : QString("Short description.\nEND OF ADMITTED BODY")).toStdString();
        page.present(projection); page.show(); app.processEvents();
        auto* edit = page.body_view();
        edit->setFocus();
        auto cursor = edit->textCursor(); cursor.movePosition(QTextCursor::End); edit->setTextCursor(cursor); edit->ensureCursorVisible();
        auto endScroll = edit->verticalScrollBar()->value();
        edit->selectAll(); edit->copy();
        bool copy = app.clipboard()->text() == QString::fromStdString(projection.Body);
        projection.Title += " refreshed"; page.present(projection); app.processEvents();
        std::cout << "baseline w=" << width << " pt=" << pt << " sections=" << sections << " bytes=" << projection.Body.size()
                  << " actual=" << page.width() << "x" << page.height() << " dpr=" << page.devicePixelRatioF()
                  << " body=" << edit->width() << "x" << edit->height() << " line=" << QFontMetrics(edit->font()).lineSpacing()
                  << " max=" << edit->verticalScrollBar()->maximum() << " end=" << endScroll << " copy=" << copy
                  << " refresh=" << edit->verticalScrollBar()->value() << " selection=" << edit->textCursor().hasSelection() << "\n";
        page.findChild<QWidget*>("overviewSideColumn")->hide(); page.files_view()->hide(); page.files_heading()->hide();
        edit->setMaximumHeight(QWIDGETSIZE_MAX); edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* column = page.findChild<QWidget*>("overviewMainColumn");
        auto* back = new QPushButton("Return to Files", column); qobject_cast<QVBoxLayout*>(column->layout())->insertWidget(0, back);
        app.processEvents(); edit->setFocus(); edit->verticalScrollBar()->setValue(0);
        QKeyEvent down(QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier); app.sendEvent(edit, &down);
        const auto pageDown = edit->verticalScrollBar()->value();
        cursor = edit->textCursor(); cursor.movePosition(QTextCursor::End); edit->setTextCursor(cursor); edit->ensureCursorVisible();
        std::cout << "expanded body=" << edit->width() << "x" << edit->height() << " max=" << edit->verticalScrollBar()->maximum()
                  << " pageDown=" << pageDown << " end=" << edit->verticalScrollBar()->value() << " returnVisible=" << back->isVisible()
                  << " endRect=" << edit->cursorRect().bottom() << "/" << edit->viewport()->height();
        QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier); app.sendEvent(edit, &tab);
        std::cout << " shiftTabReturn=" << back->hasFocus();
        const auto retainedCursor = edit->textCursor();
        const auto retainedScroll = edit->verticalScrollBar()->value();
        edit->selectAll(); edit->copy();
        std::cout << " expandedCopy=" << (app.clipboard()->text() == QString::fromStdString(projection.Body));
        edit->setTextCursor(retainedCursor); edit->verticalScrollBar()->setValue(retainedScroll);
        if (sections == 500 && pt == 20) page.grab().save(QString("/tmp/bpr-0918-overview.39SoYx/expanded-%1.png").arg(width));
        QObject::connect(back, &QPushButton::clicked, [&] {
            back->hide(); page.findChild<QWidget*>("overviewSideColumn")->show();
            page.files_view()->show(); page.files_heading()->show();
            edit->setMaximumHeight(110); edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
            page.files_view()->setFocus();
        });
        back->click(); app.processEvents();
        std::cout << " filesFocus=" << page.files_view()->hasFocus();
        page.findChild<QWidget*>("overviewSideColumn")->hide(); page.files_view()->hide(); page.files_heading()->hide();
        back->show(); edit->setMaximumHeight(QWIDGETSIZE_MAX); edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        app.processEvents(); edit->setTextCursor(retainedCursor); edit->verticalScrollBar()->setValue(retainedScroll);
        std::cout << " restored=" << (edit->textCursor().position() == retainedCursor.position() && edit->verticalScrollBar()->value() == retainedScroll) << "\n";
        page.hide();
    }
}
```
