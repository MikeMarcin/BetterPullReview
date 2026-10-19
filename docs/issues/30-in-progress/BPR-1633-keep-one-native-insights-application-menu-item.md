---
schema: "implement-rfc.card.v1"
id: "BPR-1633"
title: "Keep one native Insights application-menu item"
status: "In Progress"
rfcs: ["RFC-BPR-0038", "RFC-BPR-0086"]
depends_on: []
priority: "P1"
platforms: ["mac"]
metadata: {"assigned_implementer":"/root/release_repairs","mandate":"release-v030-coordinated-r1","baseline":"f6ba8f43e4477f60941dd24cdac4450ca68de911","milestone_plan":{"id":"release-v030-repairs","path":"docs/rfcs/plans/release-v030-repairs/milestone-plan.json"}}
---

# BPR-1633: Keep one native Insights application-menu item

The frozen Mac baseline fails the exact windowed/resident application-menu
parity assertion: after sibling windows and last-window close, two Insights
items remain in Apple's application menu. Both per-workbench and recreated
resident actions use `ApplicationSpecificRole`. Existing native update-menu
code records the same Qt retention/duplicate hazard. RFC0086 and accepted
BPR1606 require Insights in the macOS application menu; moving it to Help is
not an eligible repair.

## Acceptance

- AC1: Establish source-backed cause and preserve the original exact menu failure; load frontend-design and retain approved title, application-menu location, shortcut, active-profile dispatch and disabled no-window behavior.
- AC2: Repair only the measured menu lifetime/ownership defect; meaningful focused offscreen or isolated native evidence proves one item, correct dispatch, sibling/close/reopen lifetime and no-window disabled state without driving or capturing the owner's live desktop. Retain human/native observations that cannot safely run unattended as pending.
- AC3: Pass focused format/tidy/architecture/documentation checks, self-review and commit explicit paths, reconcile commands and report candidate identities and any remaining native acceptance.

## Design boundary

The canonical official frontend-design plugin skill is read before UI edits.
This repair follows the established native platform palette, system menu type,
existing command order and label; it adds no visual tokens, copy or layout.
The design is the single existing Insights command, with ownership matching
its application-menu lifetime and existing per-profile action semantics.

## Focused engineering evidence

One application-owned Cocoa item now follows the existing QAction whose
QMenuBar is the process's actual current NSMenu. QPointer donors prevent use
after action/bar destruction. The donor is removed from its QMenu rather than
hidden: the first hidden-action attempt was rejected because Qt disables it.
Native label, location, shortcut and per-profile callbacks remain approved.
Windows/Linux and offscreen keep the existing ordinary QAction composition.

The actual new native course passes (0.55 seconds; final guard rerun 0.58),
with activation policy Prohibited before and after QApplication, no shown
window, and only process-local AppKit menu inspection. It supplies completed
menu arbitration without activating a window, and checks one item, distinct
donor dispatch, a real native key-equivalent event dispatching exactly once,
sibling retirement, disabled resident state, reopening and destruction. All
new C++/Objective-C++ pass focused tidy22 with warnings as errors. Offscreen
Help, reset and Option courses pass. The original foreground activation course
is not run on the owner's live desktop; its exact-candidate isolated/native
qualification remains pending. This card stays In Progress for that obligation.


## Composed fixture mechanism alignment

The coordinator identifies the still-old Insights public-path assertion that
requires ApplicationSpecificRole on each donor. The singleton repair instead
keeps the enabled window shortcut donor at NoRole and removes its QMenu
associations on Cocoa; the native item is application-owned. This fixture now
asserts that exact Cocoa donor boundary, including absence from any per-window
QMenu. Offscreen retains and asserts its ordinary ApplicationSpecificRole
action because the native helper intentionally returns false there. Public
dispatch/shortcut assertions remain unchanged. The separate nonactivating native
course proves the real AppKit item; the original foreground native course
remains pending, rather than being certified by an enum. Format22 and diff
checks pass; parent owns the next safe Mac integration at the committed source.


## Retained offscreen expectation failure

The parent source1b1316a66 Mac offscreen continuation fails the new offscreen
ApplicationSpecificRole expectation. That expectation was an implementer
mistake: the caller creates the action directly in Help and no longer sets a
role; the helper returns false off Cocoa. The fixture now checks the actual
required offscreen Help-menu association, rather than guessing a default enum,
and reports platform/role/association count if that contract fails. Cocoa still
requires NoRole with no QMenu donor; actual native item/profile/empty-window
qualification remains separate. The failing Mac result is retained and the
corrected assertion's actual Mac result remains pending; no Mac command is run
by the repair owner while the parent gate is active.

## Verified offscreen fixture alignment

Mac source4e0a complete Insights public course passes1.57s under explicitglobal
offscreen. Actual marker is platform=offscreen, role=1, associations=2; its
required Help QMenu association is present. The same frozen course's native
Prohibited unit passes0.20s and retains exactly one process-local native item,
profile donor dispatch, native shortcut and empty/reopen lifecycle assertions.
The changed public-course TU passes scoped clang-tidy22 warnings-as-errors.
The earlier1b1316 enum-assumption failure remains retained. No foreground
activation course is run; that required isolated or attended qualification
keeps this card In Progress.
