/* BPR-PENDING-AN design round v1, revision 2: first-run agent setup in the Agent tab.
   Static mockups, no product code. Builds on BPR-1574's accepted kit (tokens,
   window chrome, drawer, icons); nothing here adds a hue. Paths, versions and
   commits are examples of what detection would report, not readings of
   anyone's Mac. Revision 2 adds the Repository step (owner, 2026-10-22: an
   agentic review needs the real repository, not just the diff). */
"use strict";

/* As the landed drawer draws them in a 375 px drawer: glyphs, and the open tab's name. */
const tabs = (on = "Agent") => [
  ["", '<span class="cnt">2</span>', on === "Comments", "comments"],
  ["", "", on === "Notes", "notes"],
  ["", "", on === "Tour", "tour"],
  ["Agent", "", on === "Agent", "agent"],
];

/* One step of the setup: a number (the steps are a real sequence), its title,
   its state glyph, a one-line summary once done, and its body while open. */
function step(n, title, state, body = "", summary = "") {
  const glyph = state === "done" ? `<span class="sg ok">${ic("check", "xs")}</span>`
    : state === "attention" ? `<span class="sg att">${n}</span>`
    : state === "busy" ? `<span class="sg busy"><span class="spin"></span></span>`
    : state === "later" ? `<span class="sg later">${n}</span>`
    : `<span class="sg now">${n}</span>`;
  const sum = summary ? `<span class="ssum">${summary}</span><span class="sp"></span><span class="minor">Change</span>` : "";
  return `<div class="step ${state}"><div class="sh">${glyph}<b>${title}</b>${sum}</div>${body ? `<div class="sb">${body}</div>` : ""}</div>`;
}

/* A choice row: a found agent or a found clone, with where it is and how it was found. */
function choiceRow(o) {
  return `<label class="arow ${o.on ? "on" : ""} ${o.off ? "off" : ""}"><span class="radio ${o.on ? "on" : ""}"></span>
    <span class="aw"><span class="an">${o.name}${o.badge ? `<span class="tag">${o.badge}</span>` : ""}</span>
    <span class="ap">${o.path ? `<code class="path">${o.path}</code>` : ""}<span class="how">${o.how}</span></span></span></label>`;
}

const CLAUDE = { name: "Claude Code", path: "/opt/homebrew/bin/claude", how: "Found on PATH." };
const CODEX = { name: "Codex", path: "~/.local/bin/codex", how: "Found in a usual install folder." };
const CLONE = { name: "~/Documents/GitHub/engine", path: "origin  github.com/bpr-fixtures/engine", how: "In ~/Documents/GitHub. On branch main." };
const CLONE2 = { name: "~/src/engine-old", path: "upstream  github.com/bpr-fixtures/engine", how: "In ~/src. On branch spike/retry." };

/* The two checks a first question needs. */
function checks(rows) {
  return `<ul class="checks">${rows.map(([state, text, sub]) => {
    const g = state === "ok" ? `<span class="cg ok">${ic("check", "xs")}</span>`
      : state === "busy" ? `<span class="cg"><span class="spin"></span></span>`
      : state === "att" ? `<span class="cg att">${ic("warn", "xs")}</span>`
      : `<span class="cg wait"></span>`;
    return `<li class="${state}">${g}<span><span class="ct">${text}</span>${sub ? `<span class="cs">${sub}</span>` : ""}</span></li>`;
  }).join("")}</ul>`;
}
const CHECK_AGENT = "Claude Code is signed in";
const CHECK_TREE = "A worktree of ~/Documents/GitHub/engine at 7f3a9c1";

const intro = `<p class="lead">Ask an agent you already use about this pull request. It runs on this Mac with its own sign-in, in its own worktree of your repository at the pull request's head. Nothing it says is published unless you make it a comment.</p>`;

function setupBody(o) {
  return `<div class="setup"><h3 class="st">Set up an agent</h3>${o.intro === false ? "" : intro}
    <div class="steps">${o.steps.join("")}</div>
    ${o.foot ? `<div class="sfoot">${o.foot}</div>` : ""}</div>`;
}
const footLinks = `<span class="pill quiet sm">${ic("refresh", "xs")}Look again</span><span class="sp"></span><span class="pill quiet sm">${ic("gear", "xs")}Agent settings</span>`;

const agentDone = step(1, "Agent", "done", "", "Claude Code");
const repoDone = step(2, "Repository", "done", "", "~/Documents/GitHub/engine");
const repoLater = step(2, "Repository", "later");
const checkLater = step(3, "Check it works", "later");
const readyLater = step(4, "Ready", "later");
const checkIntro = `<p>Before your first question, BetterPullReview runs <code class="c">claude auth status</code>, which sends no question and no code, and makes the agent's worktree once.</p>`;

/* ---------- the candidate states ---------- */
const S = {};
S.one = setupBody({
  steps: [
    step(1, "Agent", "done", choiceRow({ ...CLAUDE, on: true }) + `<span class="minor">Use another program…</span>`),
    repoLater, checkLater, readyLater,
  ],
  foot: footLinks,
});
S.two = setupBody({
  steps: [
    step(1, "Choose an agent", "now", choiceRow({ ...CLAUDE, on: true }) + choiceRow(CODEX) + `<span class="minor">Use another program…</span>`),
    repoLater, checkLater, readyLater,
  ],
  foot: footLinks,
});
S.none = setupBody({
  steps: [
    step(1, "Agent", "attention", `<div class="callout"><div><div class="t">No agent found on this Mac</div><div class="s">BetterPullReview looked for <code class="c">claude</code> and <code class="c">codex</code> on PATH and in the usual install folders.</div></div></div>
      <p>Install one, sign in to it in Terminal, then look again.</p>
      <div class="acts"><span class="pill outline sm">${ic("ext", "xs")}Install Claude Code</span><span class="pill outline sm">${ic("ext", "xs")}Install Codex</span></div>
      <div class="acts"><span class="pill primary sm">${ic("refresh", "xs")}Look again</span><span class="pill quiet sm">Use a program I choose…</span></div>`),
    repoLater, checkLater, readyLater,
  ],
  foot: `<span class="sp"></span><span class="pill quiet sm">${ic("gear", "xs")}Agent settings</span>`,
});
/* Repository: the clone the reviewer already has, remembered for this repository. */
S.repo = setupBody({
  intro: false,
  steps: [
    agentDone,
    step(2, "Repository", "now", `<p>The agent reads more than the diff: callers, tests, history. Two clones of <b>bpr-fixtures/engine</b> are in your clone folders. The one you choose is remembered for this repository; the agent gets its own worktree of it at the head, and your checkout and branches are not touched.</p>
      ${choiceRow({ ...CLONE, on: true })}${choiceRow(CLONE2)}
      <div class="acts"><span class="pill primary sm">Use this clone</span><span class="pill quiet sm">Choose a folder…</span></div>
      <span class="minor">Use a private copy instead</span>`),
    checkLater, readyLater,
  ],
  foot: footLinks,
});
/* No clone folder listed yet: the setting, offered in place, with folders that exist as suggestions. */
S.nofolders = setupBody({
  intro: false,
  steps: [
    agentDone,
    step(2, "Repository", "now", `<p>Where do your clones live? BetterPullReview looks in these folders for a clone of <b>bpr-fixtures/engine</b>, reading each clone's <code class="c">.git/config</code> and running nothing.</p>
      <div class="acts"><span class="pill outline sm">${ic("plus", "xs")}~/Documents/GitHub</span><span class="pill outline sm">${ic("plus", "xs")}~/src</span><span class="pill quiet sm">Add a folder…</span></div>
      <p class="quietp">The list is in Settings › Repositories › Clone folders.</p>
      <span class="minor">Use a private copy instead</span>`),
    checkLater, readyLater,
  ],
  foot: footLinks,
});
S.noclone = setupBody({
  intro: false,
  steps: [
    agentDone,
    step(2, "Repository", "attention", `<div class="callout"><div><div class="t">No clone of bpr-fixtures/engine in your clone folders</div><div class="s">BetterPullReview looked in ~/Documents/GitHub and ~/src, two levels down, for a remote naming github.com/bpr-fixtures/engine.</div></div></div>
      <div class="acts"><span class="pill primary sm">Choose its folder…</span><span class="pill outline sm">Clone into ~/Documents/GitHub</span></div>
      <p class="quietp">Or use a private copy: the head and its history, without your branches, tags, LFS files or submodules.</p>
      <div class="acts"><span class="pill quiet sm">Use a private copy</span><span class="pill quiet sm">Edit clone folders…</span></div>`),
    checkLater, readyLater,
  ],
  foot: footLinks,
});

/* ---------- Settings: the two owner decisions (2026-10-22) ---------- */
const settingsPane = (title, body) => `<div class="spane"><div class="snav">${["General", "Accounts & hosts", "Repositories", "Home & queue", "Review", "Comments", "Appearance", "Notifications", "Agents & context", "Storage & privacy"].map((n) => `<div class="${n === title ? "on" : ""}">${n}</div>`).join("")}</div><div class="sbody"><h3>${title}</h3>${body}</div></div>`;
S["settings-folders"] = settingsPane("Repositories", `<h4>Clone folders</h4>
  <p class="quietp">Where your existing clones live. To link a pull request's repository to your clone, BetterPullReview looks in each folder and two levels below it for a clone whose remote names that repository. It reads <code class="c">.git/config</code> only, follows no links and runs nothing.</p>
  <div class="slist"><div class="si"><code class="path">~/Documents/GitHub</code><span class="sp"></span><span class="minor">Remove</span></div><div class="si"><code class="path">~/src</code><span class="sp"></span><span class="minor">Remove</span></div></div>
  <div class="acts"><span class="pill outline sm">Add a folder…</span></div>
  <h4>Linked clones</h4>
  <div class="slist"><div class="si"><span><b>bpr-fixtures/engine</b><br><code class="path">~/Documents/GitHub/engine</code></span><span class="sp"></span><span class="minor">Change…</span><span class="minor">Forget</span></div>
  <div class="si"><span><b>MikeMarcin/BPR-Test</b><br><code class="path">~/src/BPR-Test</code></span><span class="sp"></span><span class="minor">Change…</span><span class="minor">Forget</span></div></div>
  <p class="quietp">Agent worktrees: 2, using 184 MB. They are removed when their session ends.</p>`);
S["settings-skill"] = settingsPane("Agents & context", `<h4>Review skill</h4>
  <p class="quietp">How the agent reviews a pull request: read the brief, follow the change through the worktree, verify each finding, and say what it could not check. Sent with every review request.</p>
  <div class="sstat"><span class="tag">Edited</span><span>Based on the shipped skill, version 3. Version 4 is available.</span></div>
  <div class="seditor"><pre>---
name: bpr-pr-review
description: Review one pull request in its worktree…
metadata:
  bpr-version: 3
---

# Review this pull request

1. Read the brief: description, base, head, threads.
2. Follow each changed function to its callers,
   tests and configuration in the worktree.
3. Verify a finding before reporting it: a failure
   path, a focused test, or a comparison with base.
4. Say what you could not verify.</pre></div>
  <div class="acts"><span class="pill primary sm">Save</span><span class="pill outline sm">Open in editor</span><span class="pill outline sm">Compare with version 4</span><span class="sp"></span><span class="pill quiet sm">Restore default</span></div>`);
S.check = setupBody({
  intro: false,
  steps: [
    agentDone, repoDone,
    step(3, "Check it works", "now", `${checkIntro}${checks([["wait", CHECK_AGENT], ["wait", CHECK_TREE]])}<div class="acts"><span class="pill primary sm">Check</span></div>`),
    readyLater,
  ],
  foot: footLinks,
});
/* Q3's alternative B: the check is one short real request instead of a status command. */
S.oneAsk = setupBody({
  intro: false,
  steps: [
    agentDone, repoDone,
    step(3, "Check it works", "now", `<p>Before your first question, BetterPullReview asks Claude Code one short question with no code, “Reply with OK”, which counts as one small request on your Claude plan, and makes the agent's worktree once.</p>${checks([["wait", "Claude Code answers"], ["wait", CHECK_TREE]])}<div class="acts"><span class="pill primary sm">Check</span></div>`),
    readyLater,
  ],
  foot: footLinks,
});
S.checking = setupBody({
  intro: false,
  steps: [
    agentDone, repoDone,
    step(3, "Check it works", "busy", `${checks([["ok", CHECK_AGENT, "Version 2.1.296."], ["busy", "Fetching 7f3a9c1 into ~/Documents/GitHub/engine and making the worktree…"]])}<div class="acts"><span class="pill outline sm">Stop</span></div>`),
    readyLater,
  ],
});
S.signin = setupBody({
  intro: false,
  steps: [
    agentDone, repoDone,
    step(3, "Check it works", "attention", `${checks([["att", "Claude Code isn't signed in", "Sign in once in Terminal. BetterPullReview never sees your sign-in."], ["ok", CHECK_TREE]])}
      <div class="cmd"><code>claude auth login</code><span class="pill quiet sm">Copy</span></div>
      <div class="acts"><span class="pill primary sm">Check again</span></div>`),
    readyLater,
  ],
  foot: footLinks,
});
/* The owner's first run, in words: the copy could not be made, before any agent ran. */
S.workspace = setupBody({
  intro: false,
  steps: [
    agentDone, step(2, "Repository", "done", "", "Private copy"),
    step(3, "Check it works", "attention", `${checks([["ok", CHECK_AGENT], ["att", "The private copy of this pull request couldn't be made", "This Mac's saved copy of the repository is missing commit 984772c, which comes before the head 7f3a9c1. No agent was started."]])}
      <div class="acts"><span class="pill primary sm">Get the pull request again</span><span class="pill quiet sm">Link your clone instead</span></div>
      <span class="minor">Show details</span>`),
    readyLater,
  ],
  foot: footLinks,
});
/* Ready: the steps fold to one line above the conversation, and the question field takes the keyboard. */
const readyLine = `<div class="ready"><span class="sg ok">${ic("check", "xs")}</span><div><b>Claude Code is ready</b><div class="s">Signed in, version 2.1.296. Works in a worktree of <code class="path">~/Documents/GitHub/engine</code> at 7f3a9c1.</div></div><span class="sp"></span><span class="ib" title="Dismiss">${ic("close", "s")}</span></div>`;
const composer = `<div class="ask"><div class="field focus"><span class="ph">Ask about this pull request…</span>
  <div class="arow2"><span class="ph s">Pull request</span><span class="sp"></span><span class="pill quiet sm">Context</span><span class="pill primary sm">Ask</span></div></div></div>`;
const ctxRow = (name = "Claude Code") => `<div class="ctxrow"><span class="chip">${ic("agent", "xs")} ${name} ${ic("down", "xs")}</span><span class="chip">7f3a9c1</span><span class="chip">Pull request ${ic("down", "xs")}</span><span class="sp"></span><span class="pill quiet sm">History</span></div>`;
/* What the agent is given: the review brief, as a disclosure the reviewer can open. */
const brief = `<details class="brief" open><summary>What the agent gets</summary><dl>
  <dt>Pull request</dt><dd>bpr-fixtures/engine #22, “Clamp device scale and bound frame retries”, and its description</dd>
  <dt>Base and head</dt><dd><code class="path">main</code> at 51d2dad, head 7f3a9c1</dd>
  <dt>Changes</dt><dd>18 files; the diff is <code class="path">git diff 51d2dad...7f3a9c1</code> in its worktree</dd>
  <dt>Threads</dt><dd>6 review threads, 2 unresolved</dd>
  <dt>Worktree</dt><dd><code class="path">~/Library/…/BetterPullReview/worktrees/engine-22-7f3a9c1</code></dd>
  <dt>It may not</dt><dd>Push, comment or change your checkout</dd></dl></details>`;
S.ready = `${ctxRow()}<div class="convo">${readyLine}<div class="pad">${brief}</div><div class="acts pad"><span class="pill outline sm">Review for findings</span></div></div>${composer}`;
/* Partly configured: the saved agent is no longer where it was. */
S.moved = `${ctxRow()}<div class="convo"><div class="callout warnc"><div><div class="t">Claude Code isn't at /opt/homebrew/bin/claude any more</div><div class="s">It may have been moved or uninstalled. Your question is kept.</div><div class="acts"><span class="pill primary sm">Set up again</span><span class="pill quiet sm">Show the log</span></div></div></div></div>${composer}`;
/* After setup, an Ask that fails for a setup reason sends you back to that step, not to Ask again. */
S.askfail = `${ctxRow()}<div class="convo"><div class="callout warnc"><div><div class="t">Claude Code didn't get your question</div><div class="s">Its worktree couldn't be made: ~/Documents/GitHub/engine is no longer a clone of bpr-fixtures/engine, or it moved. No agent was started. Your question is kept.</div><div class="acts"><span class="pill primary sm">Choose its folder…</span><span class="pill quiet sm">Use a private copy</span><span class="pill quiet sm">Show details</span></div></div></div></div>${composer}`;
/* A new head: the session stays on what it read; a new session gets a new worktree. */
S.newhead = `${ctxRow()}<div class="convo"><div class="callout blue warnc"><div><div class="t">The pull request moved to 9c04e1b</div><div class="s">This session read 7f3a9c1 in its own worktree, which stays until the session ends. A new session gets a new worktree at 9c04e1b.</div><div class="acts"><span class="pill primary sm">Start a new session</span><span class="pill quiet sm">Continue on 7f3a9c1</span></div></div></div></div>${composer}`;
/* Re-run: History's menu (and Settings) offer Set up agent… at any time. */
S.rerun = `${ctxRow()}<div class="convo"><p class="quietp pad">Ask a question below, or let the agent review the pull request for findings.</p></div>
  <div class="menu" style="right:10px;top:40px"><div class="mi">Start a new session</div><div class="mi dim">Continue on the old head</div><div class="ms"></div><div class="mi">Private history and runs</div><div class="mi dim">Sessions: 2 on this head</div><div class="ms"></div><div class="mi">Change repository folder…</div><div class="mi on">Set up agent…</div></div>${composer}`;
/* An enterprise host that forbids Codex says so in the row. */
S.ghes = setupBody({
  steps: [
    step(1, "Choose an agent", "now", choiceRow({ ...CLAUDE, on: true }) + choiceRow({ ...CODEX, off: true, how: "Not allowed for github.example.com." }) + `<span class="minor">Use another program…</span>`),
    repoLater, checkLater, readyLater,
  ],
  foot: footLinks,
});

/* ---------- the alternatives the owner can choose instead ---------- */
const ALT = {};
ALT.empty = `<div class="empty" style="height:100%"><span class="av agent lg">&gt;_</span>
  <div class="say" style="max-width:34ch">Ask an agent about this pull request. It runs on this Mac with its own sign-in, and nothing it says is published unless you make it a comment.</div>
  <ol class="next"><li>Install Claude Code or Codex.</li><li>Sign in to it in Terminal: <code class="c">claude auth login</code> or <code class="c">codex login</code>.</li><li>Choose it and your repository folder in Settings, then come back here.</li></ol>
  <span class="pill primary sm">Open agent settings</span></div>`;
ALT.sheet = `<div class="sheet"><div class="shh"><b>Set up an agent</b><span class="sp"></span><span class="minor">Step 2 of 4</span></div>
  <div class="shb">${step(2, "Repository", "now", `<p>Link <b>bpr-fixtures/engine</b> to your clone. The agent gets its own worktree at the head; your checkout and branches are not touched.</p>${choiceRow({ ...CLONE, on: true })}<div class="acts"><span class="pill quiet sm">Choose a folder…</span></div>`)}</div>
  <div class="shf"><span class="pill quiet sm">Cancel</span><span class="sp"></span><span class="pill outline sm">Back</span><span class="pill primary sm">Continue</span></div></div>`;

/* ---------- frames ---------- */
function agentDrawer(body, width = 375) {
  return drawer({ tabs: tabs("Agent"), width, tools: tool("max", false, "Wider") + tool("close", false, "Close"), body: `<div class="agentbody">${body}</div>` });
}
function windowWith(body, o = {}) {
  return win({ centre: codeCol({ from: 420 }), drawer: agentDrawer(body), status: "Actionable file 3 of 18.", ...o });
}
/* A single drawer still at the size the drawer has in a 1440 by 900 window. */
function drawerStill(body, width = 375) {
  return `<div class="dstill" style="width:${width}px">${agentDrawer(body, width)}</div>`;
}

const STATES = [
  ["one", S.one], ["two", S.two], ["none", S.none], ["repo", S.repo], ["nofolders", S.nofolders], ["noclone", S.noclone],
  ["check", S.check], ["one-ask", S.oneAsk], ["checking", S.checking], ["signin", S.signin],
  ["workspace", S.workspace], ["ready", S.ready], ["moved", S.moved], ["askfail", S.askfail],
  ["newhead", S.newhead], ["rerun", S.rerun], ["ghes", S.ghes], ["alt-empty", ALT.empty],
];

function render() {
  const q = new URLSearchParams(location.search);
  if (q.get("theme") === "dark") document.body.classList.add("dark");
  const still = q.get("still") || "one";
  document.body.classList.add("drawerstill");
  if (still === "window") { document.body.innerHTML = `<div class="winstill">${windowWith(S.repo)}</div>`; return; }
  if (still === "alt-sheet") { document.body.innerHTML = `<div class="winstill">${win({ centre: codeCol({ from: 420 }), drawer: agentDrawer(""), status: "Actionable file 3 of 18." })}<div class="scrim"></div>${ALT.sheet}</div>`; return; }
  if (still.startsWith("settings-")) { document.body.innerHTML = S[still]; return; }
  const found = STATES.find(([id]) => id === still);
  document.body.innerHTML = drawerStill(found ? found[1] : S.one);
}
