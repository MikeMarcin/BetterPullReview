"use strict";
/* BPR-1914 follow-up design round v1: three places for the tour's side chat.
   frame.html?dir=A&f=1 draws one 1440x900 window; index.html shows them all.
   Content is a fixture pull request, not private repository content. */

const I = {
  chat: '<svg class="i s" viewBox="0 0 16 16"><path d="M3 3.5h10a1 1 0 0 1 1 1v6a1 1 0 0 1-1 1H8l-3 2.5v-2.5H3a1 1 0 0 1-1-1v-6a1 1 0 0 1 1-1z"/></svg>',
  spark: '<svg class="i s" viewBox="0 0 16 16"><path d="M8 1.8l1.4 3.9 3.9 1.4-3.9 1.4L8 12.4 6.6 8.5 2.7 7.1l3.9-1.4z"/><path d="M13 11.2l.5 1.3 1.3.5-1.3.5-.5 1.3-.5-1.3-1.3-.5 1.3-.5z"/></svg>',
  x: '<svg class="i xs" viewBox="0 0 12 12"><path d="M3 3l6 6M9 3l-6 6"/></svg>',
  plus: '<svg class="i s" viewBox="0 0 16 16"><path d="M8 3v10M3 8h10"/></svg>',
  copy: '<svg class="i s" viewBox="0 0 16 16"><rect x="5" y="5" width="8.5" height="8.5" rx="1.5"/><path d="M3 10.5V3.5a1 1 0 0 1 1-1h6.5"/></svg>',
  up: '<svg class="i s" viewBox="0 0 16 16"><path d="M8 13V3M4 7l4-4 4 4"/></svg>',
  down: '<svg class="i xs" viewBox="0 0 12 12"><path d="M3 4.5l3 3 3-3"/></svg>',
  right: '<svg class="i xs" viewBox="0 0 12 12"><path d="M4.5 3l3 3-3 3"/></svg>',
  left: '<svg class="i xs" viewBox="0 0 12 12"><path d="M7.5 3l-3 3 3 3"/></svg>',
  code: '<svg class="i s" viewBox="0 0 16 16"><path d="M5.5 4.5L2 8l3.5 3.5M10.5 4.5L14 8l-3.5 3.5"/></svg>',
  check: '<svg class="i xs" viewBox="0 0 12 12"><path d="M2.5 6.5l2.5 2.5 4.5-6"/></svg>',
};

/* ---------- fixture content ---------- */
const PROBLEM = "Every frame redrew every layer, even when nothing changed, so an idle window kept the GPU busy.";
const SOLUTION = "Keep the last frame and redraw only the layers that changed. The renderer asks the frame pool for the retained frame and paints the dirty layers over it.";
const STOPS = [
  { n:1, t:"Where the change starts", src:"renderer.cpp 3–8", ex:"The draw loop starts here. The order of the first two calls changed." },
  { n:2, t:"The pool hands back the last frame", src:"frame_pool.cpp 30–52", ex:"<code>acquire()</code> returns the retained frame when its size still matches." },
  { n:3, t:"Marking layers dirty", src:"layer.cpp 12–27", ex:"Setters mark the layer dirty; nothing else does." },
  { n:4, t:"What the test pins down", src:"renderer_test.cpp 40–71", ex:"An idle second frame paints no layers." },
];
const NEW_EX = "Clear the target before <code>begin_frame()</code>. It now returns the retained frame, so clearing after it would erase what was kept.";

/* ---------- shared chrome ---------- */
const titlebar = () => `
  <div class="tbar"><div class="lights"><i></i><i></i><i></i></div>
    <span class="prid"><b>MikeMarcin/BPR-Test</b> &nbsp;#42 Render retained frames before presenting</span>
    <span class="sp"></span><span class="modes"><span>Overview<kbd>⌘1</kbd></span><span class="on">Review<kbd>⌘2</kbd></span><span>Discuss<kbd>⌘3</kbd></span><span>Threads<kbd>⌘4</kbd></span></span></div>`;
const tree = () => `<div class="tree"><div class="tt"><span class="on">Files</span><span>Search</span></div><div style="padding-top:6px">
  <div class="r dir">⌄ src/engine</div>
  <div class="r sel" style="padding-left:26px"><i class="o part"></i>renderer.cpp<span class="d">M +7</span></div>
  <div class="r" style="padding-left:26px"><i class="o"></i>frame_pool.cpp<span class="d">M +22</span></div>
  <div class="r" style="padding-left:26px"><i class="o"></i>frame_pool.hpp<span class="d">M +6</span></div>
  <div class="r" style="padding-left:26px"><i class="o"></i>layer.cpp<span class="d">M +9</span></div>
  <div class="r dir">⌄ tests/engine</div>
  <div class="r" style="padding-left:26px"><i class="o"></i>renderer_test.cpp<span class="d">M +31</span></div></div></div>`;
const status = () => `<div class="status">Actionable file 1 of 5. Review history is ready.<span class="sp"></span>Updated just now</div>`;

const CODE = [
  ["hunk", "@@ -1,9 +1,10 @@ void renderer::draw(const scene& aScene)"],
  ["", 1, '<span class="k">#include</span> <span class="str">"engine/renderer.hpp"</span>'],
  ["", 2, ""],
  ["stop first", 3, '<span class="k">void</span> <span class="ty">renderer</span>::<span class="f">draw</span>(<span class="k">const</span> <span class="ty">scene</span>&amp; aScene)'],
  ["stop", 4, "{"],
  ["stop add", 5, '\ttarget.<span class="f">clear</span>(background);'],
  ["stop", 6, '\t<span class="k">auto</span>&amp; frame = pool.<span class="f">begin_frame</span>();'],
  ["stop del", 6, '\ttarget.<span class="f">clear</span>(background);'],
  ["stop", 7, '\t<span class="k">for</span> (<span class="k">const auto</span>&amp; layer : aScene.<span class="f">dirty_layers</span>())'],
  ["stop", 8, '\t\t<span class="f">paint</span>(frame, layer);'],
  ["", 9, '\t<span class="f">present</span>(frame);'],
  ["", 10, "}"],
  ["", 11, ""],
  ["hunk", "@@ -24,6 +25,12 @@ void renderer::resize(size aSize)"],
  ["", 25, '<span class="k">void</span> <span class="ty">renderer</span>::<span class="f">resize</span>(<span class="ty">size</span> aSize)'],
  ["", 26, "{"],
  ["add", 27, '\t<span class="c">// A new size cannot reuse the retained frame.</span>'],
  ["add", 28, '\tpool.<span class="f">forget</span>();'],
  ["", 29, '\ttarget.<span class="f">resize</span>(aSize);'],
  ["", 30, "}"],
];
const codeArea = (opt = {}) => {
  const rows = CODE.map(r => {
    if (r[0] === "hunk") return `<div class="hunk">${r[1]}</div>`;
    const cls = r[0].split(" ");
    const showStop = opt.stop !== false;
    const c = ["ln", cls.includes("add") ? "add" : "", cls.includes("del") ? "del" : "",
      showStop && cls.includes("stop") ? "inroute" : "", showStop && opt.wash && cls.includes("stop") ? "agentq" : ""].join(" ");
    const flag = showStop && cls.includes("first") ? '<span class="stopflag">1</span>' : "";
    return `<div class="${c}">${flag}<span class="g">${r[1]}</span><span class="m">${cls.includes("add") ? "+" : cls.includes("del") ? "−" : ""}</span><span class="t">${r[2]}</span></div>`;
  }).join("");
  return `<div class="codearea"><div class="dtabs">
      <div class="dtab on"><span>renderer.cpp</span><span class="x">${I.x}</span></div>
      <div class="dtab"><span>frame_pool.cpp</span><span class="x">${I.x}</span></div></div>
    <div style="padding-top:6px">${rows}</div></div>`;
};

const drawerTabs = (on, badge) => `<div class="dwtabs">${["Comments", "Notes", "Tour", "Agent"].map(t =>
  `<span class="${t === on ? "on" : ""}">${t}${t === "Tour" && badge ? `<i class="badge">${badge}</i>` : ""}</span>`).join("")}</div>`;
const tourFoot = () => `<div class="dwfoot"><span>Private draft. Nothing published.</span><span class="sp"></span><span class="btn dim">Preview and publish</span></div>`;

const glyph = (n, idle) => `<span class="glyph ${idle ? "idle" : ""}">${I.chat}${n ? n : ""}</span>`;

/* ---------- conversation pieces ---------- */
const Q1 = "Why does <code>clear()</code> come before <code>begin_frame()</code> now?";
const A1 = `<p><code>begin_frame()</code> now hands back the retained frame (<code class="cite">frame_pool.cpp:41</code>). Clearing after it would wipe the frame it just kept, and every layer would have to paint again.</p>
  <p>So the target is cleared first (<code class="cite">renderer.cpp:5</code>), then only <code>dirty_layers()</code> paint over the kept frame (<code class="cite">renderer.cpp:7–8</code>).</p>`;
const Q2 = "Good. Say that in the stop, shorter.";
const worked = s => `<div class="worked">${I.spark}Worked for ${s}s ${I.right}</div>`;
const turnA = (a, s) => `<div class="ag">${worked(s)}${a}</div>`;
const conversationStop1 = () => `
  <div class="me">${Q1}</div>${turnA(A1, 2)}
  <div class="me">${Q2}</div>`;
const proposalCard = (title) => `
  <div class="prop"><div class="ph">${I.spark}${title}</div>
    <div class="pt">${NEW_EX}</div>
    <div class="pn">Accepting replaces what is there now.</div>
    <div class="pa"><span class="btn agentfill">Accept</span><span class="btn">Edit text</span><span class="btn quiet">Discard</span></div></div>`;
const composer = (ph, hot) => `<div class="composer"><span class="sp" style="align-self:center">${ph}</span><span class="send ${hot ? "" : "off"}">${I.up}</span></div>`;

const comments = () => `
  <div class="thread"><div class="loc">renderer.cpp, line 6</div>
    <div class="who"><span class="av" style="background:#7FB0E8">JL</span>jlee <span style="color:var(--ink-3);font-weight:400">2h</span></div>
    <p>Does <code>begin_frame()</code> still allocate when the pool is empty?</p>
    <div class="who"><span class="av" style="background:#E3B357">MM</span>MikeMarcin <span style="color:var(--ink-3);font-weight:400">1h</span></div>
    <p>Once, on the first frame. After that it reuses.</p></div>
  <div class="thread"><div class="loc">renderer.cpp, line 28</div>
    <div class="who"><span class="av" style="background:#3FB950">AK</span>akumar <span style="color:var(--ink-3);font-weight:400">40m</span></div>
    <p>nitpick: <code>forget()</code> reads like it drops the pool. <code>discard_retained()</code>?</p></div>
  <div class="thread"><div class="loc">layer.cpp, line 19</div>
    <div class="who"><span class="av" style="background:#BC8CFF">JL</span>jlee <span style="color:var(--ink-3);font-weight:400">35m</span></div>
    <p>question: what marks a layer dirty after a theme change?</p></div>`;

const note = (cls, x, y, text, w) => `<div class="note ${cls}" style="left:${x}px;top:${y}px;${w ? `max-width:${w}px` : ""}">${text}</div>`;

/* =====================================================================
   A. Chat inside the Tour tab: the Tour tab widens into two columns.
   ===================================================================== */
const A = {};
/* The Tour tab as shipped: Draft the tour, Problem and Solution as summary rows
   with Change, then the stops. `onStyle` is how a direction marks the part
   under discussion; an expanded part shows its whole text. */
const tourParts = (active, counts, onStyle) => {
  const part = (key, name, body) => {
    const on = active === key;
    return on ? `<div class="sect" style="${onStyle}"><div class="sh"><span class="sg">${I.check}</span><b>${name}</b>${glyph(counts[key])}</div><div class="prose">${body}</div></div>`
      : `<div class="sum"><span class="sg">${I.check}</span><b>${name}</b><span class="tx">${body}</span>${counts[key] ? glyph(counts[key]) : ""}<span class="chg">Change</span></div>`;
  };
  const stops = STOPS.map(s => {
    const on = active === "s" + s.n;
    return `<div class="stop ${s.n === 1 ? "open" : ""}" style="${on ? onStyle : ""}">
      <span class="ring">${s.n}</span><div class="st"><b>${s.t}</b><code>${s.src}</code>${s.n === 1 ? `<div class="ex">${s.ex}</div>` : ""}</div>
      ${glyph(counts["s" + s.n], !counts["s" + s.n] && !on)}</div>`;
  }).join("");
  return `<div class="tourtop"><span class="btn agent">${I.spark}Draft the tour</span><span class="sp"></span>${glyph(0, true)}</div>
    ${part("p", "Problem", PROBLEM)}${part("sol", "Solution", SOLUTION)}
    <div class="sect"><div class="sh"><span class="sg" style="background:var(--signal)">3</span><b>Stops</b></div><div class="stops">${stops}</div></div>`;
};
const aTour = (active, counts) => tourParts(active, counts,
  "background:var(--agent-wash-2);box-shadow:inset 3px 0 0 var(--agent), 0 0 0 1px var(--agent-line);border-radius:0 7px 7px 0;padding:8px 10px 10px 12px;margin:0 -4px 14px -4px");
const aChatCol = (head, quote, body, ph) => `
  <div style="width:400px;flex:none;display:flex;flex-direction:column;border-left:1px solid var(--agent-line);background:linear-gradient(90deg, rgba(163,160,255,.05), transparent 40%)">
    <div style="padding:12px 16px 10px;border-bottom:1px solid var(--rule)">
      <div class="subject"><span style="color:var(--agent)">${I.chat}</span><b>${head}</b><span class="sp"></span><span class="btn quiet">${I.code}Show in code</span><span class="btn quiet">${I.copy}</span><span class="btn quiet">${I.x}</span></div>
      <div class="quote" style="margin:3px 0 0 21px">${quote}</div></div>
    <div style="flex:1;min-height:0;overflow:hidden;padding:14px 16px;display:flex;flex-direction:column;gap:12px">${body}</div>
    <div style="padding:0 16px 14px">${composer(ph, false)}</div></div>`;
A[1] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ wash: true })}
  <div class="drawer" style="width:740px">${drawerTabs("Tour")}
    <div style="flex:1;min-height:0;display:flex">
      <div style="flex:1;min-width:0;display:flex;flex-direction:column"><div class="dwbody">${aTour("s1", { s1: 3, sol: 2 })}</div>${tourFoot()}</div>
      ${aChatCol("Stop 1, Where the change starts", "renderer.cpp, lines 3 to 8",
        `<div class="msgs">${conversationStop1()}</div>
         <div class="updrow"><span class="btn agent">${I.spark}Update this stop from this chat</span></div>
         ${proposalCard("Proposed explanation for Where the change starts")}`, "Ask about this stop…")}
    </div></div></div>
  ${note("t", 730, 500, "The part under discussion carries the agent rule. Its chat is the right half of the Tour tab, which widens while a chat is open.", 250)}
  ${note("t", 1040, 742, "Update proposes here; Accept writes it into the stop on the left.", 220)}</div>`;
A[2] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ stop: false })}
  <div class="drawer" style="width:740px">${drawerTabs("Tour")}
    <div style="flex:1;min-height:0;display:flex">
      <div style="flex:1;min-width:0;display:flex;flex-direction:column"><div class="dwbody">${aTour("sol", { s1: 3, sol: 2 })}</div>${tourFoot()}</div>
      ${aChatCol("Solution", "“Keep the last frame and redraw only the layers that changed…”",
        `<div class="msgs"><div class="me">Is “retained frame” the right term? The code says <code>kept</code> in places.</div>
         ${turnA("<p>The code uses both. <code>frame_pool</code> names it <code>retained_</code> (<code class=\"cite\">frame_pool.cpp:12</code>); the comments say “kept”. The Solution follows the type, so “retained frame” matches what a reader will search for.</p>", 2)}</div>`, "Ask about the Solution…")}
    </div></div></div>
  ${note("r", 320, 160, "Switch by choosing another part's chat glyph. Counts show which parts already have a chat.", 240)}</div>`;
A[3] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ stop: false })}
  <div class="drawer" style="width:420px">${drawerTabs("Comments", 2)}<div class="dwbody">${comments()}</div></div></div>
  ${note("r", 760, 50, "On another tab the chat is gone and the drawer returns to its usual width. The badge says two parts have chats; Tour brings them back as left.", 250)}</div>`;

/* =====================================================================
   B. A focused part: the chat lives inside the part's own card.
   ===================================================================== */
const B = {};
const bRow = (n, label, sub, chats) => `<div style="display:flex;align-items:center;gap:10px;padding:7px 8px;opacity:.5">
  ${n ? `<span class="ring" style="width:20px;height:20px;font-size:10px">${n}</span>` : `<span class="sg" style="width:20px;height:20px">${I.check}</span>`}
  <div style="flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis"><b style="font-weight:600">${label}</b> <span style="color:var(--ink-3)">${sub}</span></div>
  ${chats ? `<span class="glyph" style="opacity:1">${I.chat}${chats}</span>` : ""}</div>`;
const bCard = (s, inner) => `<div style="margin:6px 0;border-radius:10px;background:var(--panel);box-shadow:0 0 0 1.5px var(--agent), 0 12px 32px rgba(0,0,0,.45)">
  <div style="display:flex;gap:10px;padding:12px 14px 4px"><span class="ring">${s.n}</span><div class="st"><b style="font-size:13px">${s.t}</b><code>${s.src}</code></div>
    <span class="btn quiet">${I.code}Show in code</span></div>${inner}</div>`;
B[1] = () => {
  const ex = `<div style="padding:4px 14px 12px 46px;line-height:1.55">
      <span style="color:var(--ink-3);text-decoration:line-through;text-decoration-color:rgba(255,123,114,.6)">The draw loop starts here. The order of the first two calls changed.</span>
      <span style="background:var(--agent-wash-2);box-shadow:inset 0 -1.5px 0 var(--agent);border-radius:2px">${NEW_EX}</span>
      <div style="display:flex;align-items:center;gap:6px;margin-top:9px"><span style="color:var(--agent);font-weight:600;display:inline-flex;gap:5px;align-items:center">${I.spark}Proposed from this chat</span><span class="sp"></span>
        <span class="btn agentfill">Accept</span><span class="btn">Edit text</span><span class="btn quiet">Discard</span></div></div>`;
  const chat = `<div style="border-top:1px solid var(--rule);padding:8px 14px 12px">
      <div style="display:flex;align-items:center;gap:6px;color:var(--ink-2);font-weight:600;margin-bottom:10px">${I.down}<span style="color:var(--agent)">${I.chat}</span>Chat about this stop<span style="color:var(--ink-3);font-weight:400">3 turns</span><span class="sp"></span><span class="btn quiet">${I.copy}</span></div>
      <div class="msgs">${conversationStop1()}</div>
      <div style="margin:12px 0 10px"><span class="btn agent">${I.spark}Update this stop from this chat</span></div>
      ${composer("Ask about this stop…", false)}</div>`;
  return `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ wash: true })}
  <div class="drawer" style="width:520px">${drawerTabs("Tour")}<div class="dwbody" style="padding-top:10px">
    ${bRow(0, "Problem", PROBLEM)}${bRow(0, "Solution", SOLUTION, 2)}
    <div style="height:1px;background:var(--rule);margin:6px 0"></div>
    ${bCard(STOPS[0], ex + chat)}
    ${STOPS.slice(1).map(s => bRow(s.n, s.t, s.src)).join("")}
  </div>${tourFoot()}</div></div>
  ${note("r", 650, 168, "The proposal is written into the stop itself, struck and inserted, so you judge it where it will live.", 240)}
  ${note("r", 650, 330, "The chat folds under the part it is about. Every other part dims to one line.", 240)}</div>`;
};
B[2] = () => {
  const s = STOPS[1];
  const inner = `<div style="padding:4px 14px 12px 46px;color:var(--ink-2);line-height:1.55">${s.ex}</div>
    <div style="border-top:1px solid var(--rule);padding:8px 14px 12px">
      <div style="display:flex;align-items:center;gap:6px;color:var(--ink-2);font-weight:600;margin-bottom:10px">${I.down}<span style="color:var(--agent)">${I.chat}</span>Chat about this stop<span style="color:var(--ink-3);font-weight:400">no turns yet</span></div>
      <div style="color:var(--ink-3);margin:2px 0 10px">Ask about the stop or its lines. Select text in the code to quote it.</div>
      ${composer("Ask about this stop…", false)}</div>`;
  return `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ stop: false })}
  <div class="drawer" style="width:520px">${drawerTabs("Tour")}<div class="dwbody" style="padding-top:10px">
    ${bRow(0, "Problem", PROBLEM)}${bRow(0, "Solution", SOLUTION, 2)}
    <div style="height:1px;background:var(--rule);margin:6px 0"></div>
    <div style="display:flex;align-items:center;gap:10px;padding:7px 8px;opacity:.75">
      <span class="ring" style="width:20px;height:20px;font-size:10px">1</span><b style="font-weight:600">${STOPS[0].t}</b><span class="sp"></span>
      <span class="glyph">${I.chat}3</span><span style="color:var(--agent);font-size:11.5px;font-weight:600">Proposal waiting</span></div>
    ${bCard(s, inner)}
    ${STOPS.slice(2).map(x => bRow(x.n, x.t, x.src)).join("")}
  </div>${tourFoot()}</div></div>
  ${note("r", 650, 152, "Choosing another part folds the last one back to a line. Its chat and its waiting proposal stay with it.", 250)}</div>`;
};
B[3] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ stop: false })}
  <div class="drawer" style="width:520px">${drawerTabs("Comments", 2)}<div class="dwbody">${comments()}</div></div></div>
  ${note("r", 660, 50, "The chat belongs to the Tour tab, so on Comments it is simply not there. Back on Tour, Stop 1 is still focused with its chat open.", 260)}</div>`;

/* =====================================================================
   C. Drawer pairing: the dock stays, opened out of the part it is about.
   ===================================================================== */
const C = {};
const cTour = (active) => tourParts(active, { sol: 2, s1: 3 },
  "background:var(--agent-wash-2);margin:0 -17px 14px -8px;padding:8px 25px 10px 8px;border-radius:7px 0 0 7px;box-shadow:inset 0 1px 0 var(--agent-line), inset 0 -1px 0 var(--agent-line), inset 1px 0 0 var(--agent-line), inset 3px 0 0 var(--route)");
const cDock = (head, body, menu) => `
  <div style="width:400px;flex:none;display:flex;flex-direction:column;background:var(--drawer);border-left:2px solid var(--agent);position:relative">
    <div style="flex:0 0 38px;display:flex;align-items:center;gap:6px;padding:0 10px 0 14px;border-bottom:1px solid var(--rule)">
      <span style="color:var(--agent)">${I.chat}</span>
      <span style="display:inline-flex;align-items:center;gap:6px;padding:3px 8px;border-radius:6px;box-shadow:inset 0 0 0 1px var(--rule-2);font-weight:600;${menu ? "background:var(--panel-2)" : ""}">${head}${I.down}</span>
      <span class="btn quiet">${I.left}</span><span class="btn quiet">${I.right}</span><span class="sp"></span>
      <span class="btn quiet">${I.copy}</span><span class="btn quiet">${I.x}</span></div>
    ${body}${menu || ""}</div>`;
const cBody = (quote, inner, ph) => `
    <div style="padding:9px 16px;border-bottom:1px solid var(--rule);display:flex;align-items:center;gap:8px"><span class="quote">${quote}</span><span class="sp"></span><span class="btn quiet">${I.code}Show in code</span></div>
    <div style="flex:1;min-height:0;overflow:hidden;padding:14px 16px;display:flex;flex-direction:column;gap:12px">${inner}</div>
    <div style="padding:0 16px 14px">${composer(ph, false)}</div>`;
const stop1Chat = () => cBody("renderer.cpp, lines 3 to 8", `<div class="msgs">${conversationStop1()}</div>
    <div class="updrow"><span class="btn agent">${I.spark}Update this stop from this chat</span></div>
    ${proposalCard("Proposed explanation for Where the change starts")}`, "Ask about this stop…");
C[1] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ wash: true })}
  <div class="drawer" style="width:400px;border-right:none">${drawerTabs("Tour")}<div class="dwbody" style="overflow:visible">${cTour("s1")}</div>${tourFoot()}</div>
  ${cDock("Stop 1, Where the change starts", stop1Chat())}</div>
  ${note("r", 340, 590, "The part's row opens through the drawer's edge into the chat, like a folder tab. The dock exists only while Tour is open.", 250)}</div>`;
C[2] = () => {
  const item = (label, meta, on, hover) => `<div style="display:flex;align-items:center;gap:8px;padding:6px 10px;border-radius:5px;${hover ? "background:var(--signal);color:var(--on-signal)" : ""}">
    <span style="width:14px">${on ? I.check : ""}</span><span style="flex:1">${label}</span><span style="${hover ? "" : "color:var(--agent)"};font-size:11.5px">${meta}</span></div>`;
  const menu = `<div style="position:absolute;left:14px;top:34px;width:320px;padding:5px;border-radius:9px;background:#2B313A;box-shadow:0 0 0 1px var(--rule-2), 0 16px 40px rgba(0,0,0,.6);z-index:5">
    ${item("The whole tour", "")}${item("Problem", "")}${item("Solution", "2 turns", false, true)}
    <div style="height:1px;background:var(--rule-2);margin:4px 6px"></div>
    ${item("Stop 1, Where the change starts", "3 turns, proposal", true)}${item("Stop 2, The pool hands back the last frame", "")}
    ${item("Stop 3, Marking layers dirty", "")}${item("Stop 4, What the test pins down", "")}</div>`;
  return `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ wash: true })}
  <div class="drawer" style="width:400px;border-right:none">${drawerTabs("Tour")}<div class="dwbody" style="overflow:visible">${cTour("s1")}</div>${tourFoot()}</div>
  ${cDock("Stop 1, Where the change starts", stop1Chat(), menu)}</div>
  ${note("r", 380, 560, "Tabs are replaced by a jump list of the tour's parts (and back and forward). Choosing Solution moves the tab bridge up to the Solution.", 250)}</div>`;
};
C[3] = () => `<div class="win">${titlebar()}${status()}<div class="main">${tree()}${codeArea({ stop: false })}
  <div class="drawer" style="width:400px">${drawerTabs("Comments")}<div class="dwbody">${comments()}</div></div>
  <div style="width:34px;flex:none;background:var(--drawer);border-left:2px solid var(--agent-line);display:flex;flex-direction:column;align-items:center;padding-top:12px;gap:10px;color:var(--agent)">
    ${I.chat}<span style="writing-mode:vertical-rl;font-weight:600;letter-spacing:.01em">Stop 1 chat, proposal waiting</span></div></div>
  ${note("r", 760, 60, "On another tab the dock folds to a rail. It names the open chat; clicking it returns to the Tour tab with the chat as it was.", 250)}</div>`;

const DIRS = { A, B, C };
