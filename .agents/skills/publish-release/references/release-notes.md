# Release notes

Every release body has the shape below, stable or prerelease. Start from the
template, not from a published body: those written before this standard differ
from one another and are not models.

The body has two readers. On the GitHub release page, someone came to download
the app or to learn what changed. In the app's update panel on macOS, someone
is deciding whether to update: the same Markdown shows in a box 120 to 180 px
tall, where a link opens nothing, an image never loads and a single newline is
a line break. The app keeps only the first 32 KiB. So the body is short, every
sentence makes sense without following its link, and no paragraph is wrapped
by hand.

## Template

Fill every `{{slot}}` and delete the optional lines that do not apply. The
publisher refuses a notes file that still holds `{{`.

```markdown
**{{notice-label}}:** {{notice}}

| System | Download |
|---|---|
| macOS {{macos-minimum}}+ · Universal | [Download DMG]({{dmg-url}}) |
| Windows · x64 | [Download MSI]({{msi-url}}) |
| Ubuntu {{ubuntu-release}} · x64 | [Download DEB]({{deb-url}}) |

On Ubuntu, [verify the package signature]({{verification-url}}) before you install; `dpkg` does not check it.

## Changes since [{{previous-version}}]({{previous-release-url}})

### New

- {{entry}}

### Changed

- {{entry}}

### Fixed

- {{entry}}
```

The body has no title. GitHub shows the release title, which the publisher
sets to `BetterPullReview TAG`, directly above the body, so a `#` heading only
repeats it. A prerelease body has the same shape; the build is in its tag and
title.

The download table opens the body, so the links are the first thing a reader
sees. Only a notice may come before it, because a notice can change whether
the reader should download at all. The publisher refuses a body that opens
with anything else.

## What each part holds

| Part | Holds | Never holds |
|---|---|---|
| Notice (optional) | What a user must know before downloading: one paragraph for each, the worst first. | Routine signing or testing assurances, standing policy, anything true of every release. |
| Download table | One row for each system being published, in the order macOS, Windows, Ubuntu, with one link to the installer most people want. | Rows for systems not in this release, archives, checksums, source or signature files (they stay in Assets), a heading above it, prose inside it. |
| Ubuntu line (only with an Ubuntu row) | The one fixed sentence, linked to the verification instructions for this exact release. | The key block, the fingerprint, commands, any other platform's signing status. |
| Changes | Every change a user would notice since the previous stable release, in up to three groups. | Anything in "What stays out" below. |

The body ends with the last change. It has no closing summary, thanks,
verification section or link list.

### Notice

Write a notice only for a security issue, a breaking change, a required
installation or migration step, or a known limitation that belongs to this
release and changes what the user should do. Start it with the matching label
in bold: `Security`, `Breaking change`, `Action required` or `Known
limitation`. Say the effect and the action in at most two sentences, then link
the details. A release with nothing of the kind has no notice, which is the
usual case.

An invented example:

```markdown
**Action required:** Sign in to GitHub again after you update. Saved sign-ins from 1.2.0 are not carried over. [Details](https://github.com/MikeMarcin/BetterPullReview/blob/1.3.0/docs/releases/v1.3.0.md#sign-in)
```

### Download table

Use the real minimum OS versions and the exact uploaded asset URLs:
`https://github.com/MikeMarcin/BetterPullReview/releases/download/TAG/FILE`,
with the tag being published and the file name as uploaded (a numbered build
keeps `-build.N` in the file name, also on the stable release). Label a
universal macOS build **Universal**.

### Changes

`{{previous-version}}` is the latest stable release before this one, and the
heading links its release page by its exact tag. A prerelease lists the same
changes its stable release will: everything since the previous stable release,
not since the previous build.

Sort each entry into one group. Keep the order New, Changed, Fixed and leave
out a group that has no entry.

- **New**: something a user could not do or see before.
- **Changed**: something that was already there and now behaves, looks or
  performs differently.
- **Fixed**: something that went wrong and now works as intended.

Within a group, put first what the most people will notice. Keep the whole
list to about fifteen entries, roughly one screen. Past that, cut the least
noticeable entry instead of packing several into one.

`docs/releases/vX.Y.Z.md` carries the same list under the same heading. Write
it once and keep the two the same.

## Writing an entry

An entry tells a user what is different when they use the app. It is not a
record of the work.

- One entry for each outcome a user would notice, not for each commit, card or
  component. Join the work behind one outcome; split an entry that reports two
  unrelated ones.
- One sentence, two at most, ending in a full stop. If it needs a third comma,
  it is two entries.
- Describe the app as it is now, in the present tense, starting with the place
  or thing the user sees. Under Fixed, say what used to go wrong in the user's
  terms. Do not write a command ("Open pull requests…") and do not repeat the
  group's verb ("Added…", "Fixed…").
- Name things as the interface names them, capitalised as it shows them: Home,
  Overview, Discuss, Find, Mark ready for review.
- Start an entry that applies to one system with it: "On macOS, …".
- Leave out what a user cannot notice: refactoring, tests, build and tooling
  work, documentation, release process. Mention speed only where a user would
  feel it, and say where.
- Use no internal vocabulary: issue or RFC IDs, commit hashes, file, class or
  test names, or words such as bounded, scoped, retained, host, lane, port,
  projection, journal, fixture, cohort and candidate. A change that can only
  be described that way is not user-visible; leave it out.
- No filler: "various improvements", "enhanced", "seamless", "we're excited".

Published entries, and the same changes written to this standard:

| Instead of | Write |
|---|---|
| Open pull requests progressively, prioritize the file on screen during refresh, and keep Home usable when returning. | Pull requests open progressively, and Home is usable straight away when you go back to it. |
| Receive configurable pull request activity notifications and authenticated GitHub Enterprise avatars, including CLI connections. | Desktop notifications tell you about activity on your pull requests. A preference controls them. |
| See agent failures as errors, wait for setup before suggested prompts, and copy or wrap code blocks. | Code blocks in agent replies have Copy and Wrap controls. |

Each published entry above holds two or three outcomes. The rewrite keeps one
and the others become entries of their own, in their own groups.

## Conventions

- **Versions.** Write a bare version in text: `0.4.2`, never `v0.4.2`. A tag
  form such as `v0.4.2-build.60` appears only inside a URL. Write `build 60`
  and `prerelease` in lower case, `prerelease` as one word.
- **Dates.** Give no release date; GitHub shows it. Write a date that a notice
  needs as `2026-11-19`.
- **Links.** Use full `https://` URLs, since a relative link does not resolve
  in a release body. Point at an exact tag or commit, never at `main`, a
  branch or `releases/latest`. Link only the previous release, the downloads,
  the Ubuntu instructions and a notice's details.
- **Markdown.** Use only the one table, the `##` and `###` headings
  shown, paragraphs and flat lists. No images, HTML, emoji, horizontal rules,
  nested lists or other headings. Write each paragraph and each entry on one
  line: a newline inside one shows as a line break.

## What stays out

These are release obligations with their own records. Compact notes relax none
of them.

- Qualification reports, source and build identities, commit hashes, test
  counts and failures, timing policies and waivers, vulnerability inventories
  and dispositions: in the release records.
- Source, licence and retention explanations, installation steps and signature
  verification procedures: in linked, versioned documentation or approved
  public release assets. The Linux public key and its full fingerprint are
  published there too, never pasted into the body.
- Routine signing assurances, tested-environment statements and standing
  policy disclaimers: nowhere in the body.

## From prerelease to stable

The stable body is the body of the prerelease it is copied from, with one
edit: each download URL takes the stable tag in place of the build tag. The file names and the change list stay
as they were.

## Example

The shape, filled in with changes adapted from 0.4.1:

```markdown
| System | Download |
|---|---|
| macOS 14.4+ · Universal | [Download DMG](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.1/BetterPullReview-0.4.1-build.8-macos-universal.dmg) |
| Windows · x64 | [Download MSI](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.1/BetterPullReview-0.4.1-build.8-windows-x64.msi) |
| Ubuntu 26.04 · x64 | [Download DEB](https://github.com/MikeMarcin/BetterPullReview/releases/download/0.4.1/BetterPullReview-0.4.1-build.8-linux-ubuntu-26.04-x64.deb) |

On Ubuntu, [verify the package signature](https://github.com/MikeMarcin/BetterPullReview/blob/2486075437653a60ddefdd27f73f71c46352c415/docs/releases/v0.4.1.md#linux-signature-verification) before you install; `dpkg` does not check it.

## Changes since [0.4.0](https://github.com/MikeMarcin/BetterPullReview/releases/tag/0.4.0)

### New

- Overview has a searchable reviewer picker, and asks you to confirm before it requests a review.
- Overview's comments can be filtered to new replies, and resolved conversations can be hidden.
- People appear with their GitHub profile pictures. An Avatars preference controls them.
- A link to lines in the pull request's repository shows that source as a snippet.

### Changed

- Tour stops read as rendered prose and code. The pencil edits them.
- Threads can be resolved from Discuss, and on closed or merged pull requests.

### Fixed

- Your chosen agent stays selected when you open a pull request or refresh setup.
```
