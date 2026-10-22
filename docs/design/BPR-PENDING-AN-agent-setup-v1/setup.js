/* BPR-PENDING-AN design round v1: first-run agent setup in the Agent tab.
   Static mockups, no product code. Builds on BPR-1574's accepted kit (tokens,
   window chrome, drawer, icons); nothing here adds a hue. Paths and versions
   are examples of what detection would report, not readings of anyone's Mac. */
"use strict";

/* As the landed drawer draws them in a 375 px drawer: glyphs, and the open tab's name. */
const tabs = (on = "Agent") => [
  ["", '<span class="cnt">2</span>', on === "Comments", "comments"],
  ["", "", on === "Notes", "notes"],
  ["", "", on === "Tour", "tour"],
  ["Agent", "", on === "Agent", "agent"],
];

/* One step of the setup: a number (the steps are a real sequence), its title,
   its state glyph and its body. */
function step(n, title, state, body = "") {
  const glyph = state === "done" ? `<span class="sg ok">${ic("check", "xs")}</span>`
    : state === "attention" ? `<span class="sg att">${n}</span>`
    : state === "busy" ? `<span class="sg busy"><span class="spin"></span></span>`
    : state === "later" ? `<span class="sg later">${n}</span>`
    : `<span class="sg now">${n}</span>`;
  return `<div class="step ${state}"><div class="sh">${glyph}<b>${title}</b></div>${body ? `<div class="sb">${body}</div>` : ""}</div>`;
}

/* A found agent: its plain name, where it was found, and how. */
function agentRow(o) {
  return `<label class="arow ${o.on ? "on" : ""} ${o.off ? "off" : ""}"><span class="radio ${o.on ? "on" : ""}"></span>
    <span class="aw"><span class="an">${o.name}${o.badge ? `<span class="tag">${o.badge}</span>` : ""}</span>
    <span class="ap">${o.path ? `<code class="path">${o.path}</code>` : ""}<span class="how">${o.how}</span></span></span></label>`;
}

const CLAUDE = { name: "Claude Code", path: "/opt/homebrew/bin/claude", how: "Found on PATH." };
const CODEX = { name: "Codex", path: "~/.local/bin/codex", how: "Found in a usual install folder." };

const intro = `<p class="lead">Ask an agent you already use about this pull request. It runs on this Mac with its own sign-in and reads a private copy of the code. Nothing it says is published unless you make it a comment.</p>`;

function setupBody(o) {
  return `<div class="setup"><h3 class="st">Set up an agent</h3>${o.intro === false ? "" : intro}
    <div class="steps">${o.steps.join("")}</div>
    ${o.foot ? `<div class="sfoot">${o.foot}</div>` : ""}</div>`;
}
const footLinks = `<span class="pill quiet sm">${ic("refresh", "xs")}Look again</span><span class="sp"></span><span class="pill quiet sm">${ic("gear", "xs")}Agent settings</span>`;

/* ---------- the candidate states ---------- */
const S = {};
/* The check is two things a first question needs: the agent answers, and the
   private copy of this pull request can be made (owner's first run, 2026-10-22:
   workspace_unavailable before any agent started). */
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
const CHECK_COPY = "A private copy of this pull request at 7f3a9c1";
S.one = setupBody({
  steps: [
    step(1, "Agent", "done", agentRow({ ...CLAUDE, on: true }) + `<span class="minor">Use another program…</span>`),
    step(2, "Check it works", "now", `<p>Before your first question, BetterPullReview checks both things it needs. It runs <code class="c">claude auth status</code>, which sends no question and no code, and makes the agent's private copy of this pull request once.</p>${checks([["wait", CHECK_AGENT], ["wait", CHECK_COPY]])}<div class="acts"><span class="pill primary sm">Check</span></div>`),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});
/* Q3's alternative B: the check is one short real request instead of a status command. */
S.oneAsk = setupBody({
  steps: [
    step(1, "Agent", "done", agentRow({ ...CLAUDE, on: true }) + `<span class="minor">Use another program…</span>`),
    step(2, "Check it works", "now", `<p>Before your first question, BetterPullReview asks Claude Code one short question with no code, “Reply with OK”, which counts as one small request on your Claude plan, and makes the agent's private copy of this pull request once.</p>${checks([["wait", "Claude Code answers"], ["wait", CHECK_COPY]])}<div class="acts"><span class="pill primary sm">Check</span></div>`),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});
S.two = setupBody({
  steps: [
    step(1, "Choose an agent", "now", agentRow({ ...CLAUDE, on: true }) + agentRow(CODEX) + `<span class="minor">Use another program…</span>`),
    step(2, "Check it works", "later"),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});
S.none = setupBody({
  steps: [
    step(1, "Agent", "attention", `<div class="callout"><div><div class="t">No agent found on this Mac</div><div class="s">BetterPullReview looked for <code class="c">claude</code> and <code class="c">codex</code> on PATH and in the usual install folders.</div></div></div>
      <p>Install one, sign in to it in Terminal, then look again.</p>
      <div class="acts"><span class="pill outline sm">${ic("ext", "xs")}Install Claude Code</span><span class="pill outline sm">${ic("ext", "xs")}Install Codex</span></div>
      <div class="acts"><span class="pill primary sm">${ic("refresh", "xs")}Look again</span><span class="pill quiet sm">Use a program I choose…</span></div>`),
    step(2, "Check it works", "later"),
    step(3, "Ready", "later"),
  ],
  foot: `<span class="sp"></span><span class="pill quiet sm">${ic("gear", "xs")}Agent settings</span>`,
});
S.signin = setupBody({
  steps: [
    step(1, "Agent", "done", agentRow({ ...CLAUDE, on: true })),
    step(2, "Check it works", "attention", `${checks([["att", "Claude Code isn't signed in", "Sign in once in Terminal. BetterPullReview never sees your sign-in."], ["ok", CHECK_COPY]])}
      <div class="cmd"><code>claude auth login</code><span class="pill quiet sm">Copy</span></div>
      <div class="acts"><span class="pill primary sm">Check again</span></div>`),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});
S.checking = setupBody({
  steps: [
    step(1, "Agent", "done", agentRow({ ...CLAUDE, on: true })),
    step(2, "Check it works", "busy", `${checks([["ok", CHECK_AGENT, "Version 2.1.296."], ["busy", "Making a private copy of this pull request at 7f3a9c1…"]])}<div class="acts"><span class="pill outline sm">Stop</span></div>`),
    step(3, "Ready", "later"),
  ],
});
/* The owner's first run, in words: the copy could not be made, before any agent ran. */
S.workspace = setupBody({
  steps: [
    step(1, "Agent", "done", agentRow({ ...CLAUDE, on: true })),
    step(2, "Check it works", "attention", `${checks([["ok", CHECK_AGENT], ["att", "The private copy of this pull request couldn't be made", "This Mac's saved copy of the repository is missing commit 984772c, which comes before the head 7f3a9c1. No agent was started."]])}
      <div class="acts"><span class="pill primary sm">Get the pull request again</span><span class="pill quiet sm">Use my working copy instead</span></div>
      <span class="minor">Show details</span>`),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});
/* Ready: the steps fold to one line above the conversation, and the question field takes the keyboard. */
const readyLine = `<div class="ready"><span class="sg ok">${ic("check", "xs")}</span><div><b>Claude Code is ready</b><div class="s">Signed in. Version 2.1.296 at <code class="path">/opt/homebrew/bin/claude</code>.</div></div><span class="sp"></span><span class="ib" title="Dismiss">${ic("close", "s")}</span></div>`;
const composer = `<div class="ask"><div class="field focus"><span class="ph">Ask about this pull request…</span>
  <div class="arow2"><span class="ph s">Pull request</span><span class="sp"></span><span class="pill quiet sm">Context</span><span class="pill primary sm">Ask</span></div></div></div>`;
const ctxRow = (name = "Claude Code") => `<div class="ctxrow"><span class="chip">${ic("agent", "xs")} ${name} ${ic("down", "xs")}</span><span class="chip">7f3a9c1</span><span class="chip">Pull request ${ic("down", "xs")}</span><span class="sp"></span><span class="pill quiet sm">History</span></div>`;
S.ready = `${ctxRow()}<div class="convo">${readyLine}<p class="quietp pad">Ask a question below, or let the agent review the pull request for findings.</p><div class="acts pad"><span class="pill outline sm">Review for findings</span></div></div>${composer}`;
/* Partly configured: the saved agent is no longer where it was. */
S.moved = `${ctxRow()}<div class="convo"><div class="callout warnc"><div><div class="t">Claude Code isn't at /opt/homebrew/bin/claude any more</div><div class="s">It may have been moved or uninstalled. Your question is kept.</div><div class="acts"><span class="pill primary sm">Set up again</span><span class="pill quiet sm">Show the log</span></div></div></div></div>${composer}`;
/* After setup, an Ask that fails for a setup reason sends you back to that step, not to Ask again. */
S.askfail = `${ctxRow()}<div class="convo"><div class="callout warnc"><div><div class="t">Claude Code didn't get your question</div><div class="s">The private copy of this pull request couldn't be made: commit 984772c is missing from this Mac's copy of the repository. Your question is kept.</div><div class="acts"><span class="pill primary sm">Check again</span><span class="pill quiet sm">Use my working copy instead</span><span class="pill quiet sm">Show details</span></div></div></div></div>${composer}`;
/* Re-run: History's menu (and Settings) offer Set up agent… at any time. */
S.rerun = `${ctxRow()}<div class="convo"><p class="quietp pad">Ask a question below, or let the agent review the pull request for findings.</p></div>
  <div class="menu" style="right:10px;top:40px"><div class="mi">Start a new session</div><div class="mi dim">Continue on the old head</div><div class="ms"></div><div class="mi">Private history and runs</div><div class="mi dim">Sessions: 2 on this head</div><div class="ms"></div><div class="mi on">Set up agent…</div></div>${composer}`;
/* An enterprise host that forbids Codex says so in the row. */
S.ghes = setupBody({
  steps: [
    step(1, "Choose an agent", "now", agentRow({ ...CLAUDE, on: true }) + agentRow({ ...CODEX, off: true, how: "Not allowed for github.example.com." }) + `<span class="minor">Use another program…</span>`),
    step(2, "Check it works", "later"),
    step(3, "Ready", "later"),
  ],
  foot: footLinks,
});

/* ---------- the alternatives the owner can choose instead ---------- */
const ALT = {};
ALT.empty = `<div class="empty" style="height:100%"><span class="av agent lg">&gt;_</span>
  <div class="say" style="max-width:34ch">Ask an agent about this pull request. It runs on this Mac with its own sign-in, and nothing it says is published unless you make it a comment.</div>
  <ol class="next"><li>Install Claude Code or Codex.</li><li>Sign in to it in Terminal: <code class="c">claude auth login</code> or <code class="c">codex login</code>.</li><li>Choose it in Settings, then come back here.</li></ol>
  <span class="pill primary sm">Open agent settings</span></div>`;
ALT.sheet = `<div class="sheet"><div class="shh"><b>Set up an agent</b><span class="sp"></span><span class="minor">Step 2 of 3</span></div>
  <div class="shb">${step(2, "Check it works", "now", `<p>Claude Code was found at <code class="path">/opt/homebrew/bin/claude</code>. BetterPullReview runs <code class="c">claude auth status</code> and makes the agent's private copy of this pull request once.</p>${checks([["wait", CHECK_AGENT], ["wait", CHECK_COPY]])}<div class="acts"><span class="pill primary sm">Check</span></div>`)}</div>
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
  ["one", "First visit, one agent found", S.one],
  ["one-ask", "Check by one short request", S.oneAsk],
  ["two", "Two agents found", S.two],
  ["none", "No agent found", S.none],
  ["checking", "Checking", S.checking],
  ["signin", "Found, not signed in", S.signin],
  ["workspace", "The private copy cannot be made", S.workspace],
  ["askfail", "A later Ask fails for a setup reason", S.askfail],
  ["ready", "Ready", S.ready],
  ["moved", "Partly set up: the agent moved", S.moved],
  ["rerun", "Set up again, any time", S.rerun],
  ["ghes", "A host that forbids Codex", S.ghes],
];

function render(o) {
  const q = new URLSearchParams(location.search);
  if (q.get("theme") === "dark") document.body.classList.add("dark");
  const still = q.get("still");
  if (still) {
    document.body.classList.add("drawerstill");
    const found = [...STATES, ["alt-empty", "", ALT.empty]].find(([id]) => id === still);
    if (still === "window") { document.body.innerHTML = `<div class="winstill">${windowWith(S.one)}</div>`; return; }
    if (still === "alt-sheet") { document.body.innerHTML = `<div class="winstill">${win({ centre: codeCol({ from: 420 }), drawer: agentDrawer(""), status: "Actionable file 3 of 18." })}<div class="scrim"></div>${ALT.sheet}</div>`; return; }
    document.body.innerHTML = drawerStill(found ? found[2] : S.one);
    return;
  }
  o.build();
  document.getElementById("themeBtn").onclick = () => document.body.classList.toggle("dark");
  const fit = () => document.querySelectorAll(".framebox").forEach((b) => {
    const s = b.clientWidth / 1440; b.querySelector(".scaler").style.transform = `scale(${s})`; b.style.height = `${900 * s}px`;
  });
  addEventListener("resize", fit); fit();
}
