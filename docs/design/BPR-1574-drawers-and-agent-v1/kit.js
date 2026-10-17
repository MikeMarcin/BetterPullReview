/* BPR-1574 design round v1: shared builders. Static mockups, no product code.
   Marks and tints are BPR-1080's glyphs and BPR-1079's tints, as the accepted
   Comments drawer draws them; people and content are fixture-style, made up. */
"use strict";
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&":"&amp;", "<":"&lt;", ">":"&gt;", '"':"&quot;" })[c]);

const L = {
  praise:{lt:"#BF3989",dk:"#F778BA",g:'<path d="M12 19.5 C6.2 15.6 3.5 12.6 3.5 9.2 C3.5 6.6 5.4 4.8 7.7 4.8 C9.6 4.8 11 5.9 12 7.6 C13 5.9 14.4 4.8 16.3 4.8 C18.6 4.8 20.5 6.6 20.5 9.2 C20.5 12.6 17.8 15.6 12 19.5 Z"/>'},
  nitpick:{lt:"#6E7781",dk:"#9BA4AE",g:'<path d="M15.5 9.5 C15.5 12.81 12.81 15.5 9.5 15.5 C6.19 15.5 3.5 12.81 3.5 9.5 C3.5 6.19 6.19 3.5 9.5 3.5 C12.81 3.5 15.5 6.19 15.5 9.5 Z"/><path d="M12.43 15.62 L15.62 12.43 L21.63 18.44 C23.75 20.57 20.57 23.75 18.44 21.63 Z" fill="currentColor" stroke="none"/>'},
  suggestion:{lt:"#1F6FD0",dk:"#6CB0FF",g:'<path d="M9.2 15.6 C7 14.3 5.5 12.2 5.5 9.6 C5.5 6 8.4 3.5 12 3.5 C15.6 3.5 18.5 6 18.5 9.6 C18.5 12.2 17 14.3 14.8 15.6 L14.8 17 L9.2 17 Z"/><path d="M9.8 20.3 L14.2 20.3"/>'},
  issue:{lt:"#D1242F",dk:"#FF7B72",g:'<path d="M12 4 L12 14"/><circle cx="12" cy="19" r="1.75" fill="currentColor" stroke="none"/>'},
  question:{lt:"#8250DF",dk:"#C29BFF",g:'<path d="M8.4 8.6 C8.4 5.9 10 4.2 12.2 4.2 C14.4 4.2 15.9 5.7 15.9 7.7 C15.9 10.6 12.2 11.1 12.2 14.3"/><circle cx="12.2" cy="19" r="1.75" fill="currentColor" stroke="none"/>'},
  thought:{lt:"#4F5BD5",dk:"#9AA5FF",g:'<path d="M7.2 14 C5 14 3.6 12.6 3.6 11 C3.6 9.4 5 8.2 6.8 8.4 C7.2 5.8 9.3 4 12 4 C14.6 4 16.6 5.7 17 8 C19 7.9 20.5 9.3 20.5 11 C20.5 12.7 19.2 14 17.3 14 Z"/><circle cx="7.6" cy="18" r="1.8" fill="currentColor" stroke="none"/>'},
  note:{lt:"#0E7C86",dk:"#56C7D0",g:'<circle cx="12" cy="6" r="1.75" fill="currentColor" stroke="none"/><path d="M12 10.5 L12 19"/>'},
  todo:{lt:"#C24E00",dk:"#FFA05A",g:'<path d="M15.5 12.5 L15.5 18.5 C15.5 19.05 15.05 19.5 14.5 19.5 L5.5 19.5 C4.95 19.5 4.5 19.05 4.5 18.5 L4.5 9.5 C4.5 8.95 4.95 8.5 5.5 8.5 L11 8.5"/><path d="M7.8 13 L10.6 15.8 L19.8 5"/>'},
};
const lum = (h) => { const c = [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16) / 255).map((v) => (v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4)); return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]; };
const onTint = (h) => { const l = lum(h), cw = 1.05 / (l + 0.05), cd = (l + 0.05) / (lum("#111418") + 0.05); return cw >= cd ? "#FFFFFF" : "#111418"; };
const PEOPLE = {
  tbutler:["#C8DAF5","#1D3B66","#2A4468","#BBD3F5","TB"], jiayin:["#F5D2C0","#6B2F12","#55321F","#F2C3AA","JY"],
  you:["#CDE8D2","#1D5A2C","#24472D","#B9E3C1","FA"], "observer-three":["#C6EAE4","#14544C","#204843","#AEE3DB","OT"],
  "reviewer-one":["#DCD3F5","#3F2A7A","#3A3157","#D3C8F5","RO"], "author-two":["#F1E1B4","#6A4E00","#4D411C","#EED9A0","AT"],
};
const slug = (a) => a.replace(/[^a-z0-9]/g, "");
(function themeVars() {
  let lt = "", dk = "";
  for (const l of Object.keys(L)) { lt += `--t-${l}:${L[l].lt};--o-${l}:${onTint(L[l].lt)};`; dk += `--t-${l}:${L[l].dk};--o-${l}:${onTint(L[l].dk)};`; }
  for (const [a, c] of Object.entries(PEOPLE)) { lt += `--avb-${slug(a)}:${c[0]};--avk-${slug(a)}:${c[1]};`; dk += `--avb-${slug(a)}:${c[2]};--avk-${slug(a)}:${c[3]};`; }
  const s = document.createElement("style"); s.textContent = `:root{${lt}} body.dark{${dk}}`; document.head.appendChild(s);
})();

/* The mark: a 16 px rounded square, 1.5 px border in the tint, glyph in a 12 px box (BPR-1083 v4). */
function mark(label, weight = "normal") {
  const t = `var(--t-${label})`, blk = weight === "blocking", open = weight === "open";
  const ink = blk ? `var(--o-${label})` : "var(--ink)";
  const fill = blk ? t : open ? "var(--panel)" : `color-mix(in srgb, ${t} var(--soft), var(--panel))`;
  const dash = open ? ' stroke-dasharray="2.5 1.5"' : "";
  return `<svg class="lm" viewBox="0 0 16 16" aria-label="${label}"><rect x=".75" y=".75" width="14.5" height="14.5" rx="3.25" style="fill:${fill}"/><rect x=".75" y=".75" width="14.5" height="14.5" rx="3.25" fill="none" style="stroke:${t}" stroke-width="1.5"${dash}/><svg x="2" y="2" width="12" height="12" viewBox="0 0 24 24" fill="none" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round" style="stroke:${ink};color:${ink}">${L[label].g}</svg></svg>`;
}
const av = (who, cls = "") => who === "agent"
  ? `<span class="av agent ${cls}" title="Agent">&gt;_</span>`
  : `<span class="av ${cls}" style="--avb:var(--avb-${slug(who)});--avk:var(--avk-${slug(who)})">${PEOPLE[who][4]}</span>`;
const tint = (label) => `--t:var(--t-${label})`;

const I = {
  home:'<path d="M4 11 12 4l8 7"/><path d="M6 9.5V20h12V9.5"/>',
  repos:'<rect x="4" y="4" width="16" height="16" rx="2"/><path d="M9 4v16"/>',
  files:'<path d="M4 6.5h6l2 2h8v10H4z"/>',
  ext:'<path d="M14 4h6v6"/><path d="M20 4l-8.5 8.5"/><path d="M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5"/>',
  refresh:'<path d="M19.5 12a7.5 7.5 0 1 1-2.2-5.3"/><path d="M19.5 4.5v4h-4"/>',
  comments:'<path d="M4 5.5h16v10H9l-5 4z"/>',
  panel:'<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="M14.5 5v14"/>',
  hide:'<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="M8 5v14M16 5v14"/>',
  user:'<circle cx="12" cy="8.5" r="3.5"/><path d="M5 20c1.2-3.6 3.8-5.5 7-5.5s5.8 1.9 7 5.5"/>',
  gear:'<circle cx="12" cy="12" r="3"/><path d="M12 2.8v2.4M12 18.8v2.4M2.8 12h2.4M18.8 12h2.4M5.5 5.5l1.7 1.7M16.8 16.8l1.7 1.7M5.5 18.5l1.7-1.7M16.8 7.2l1.7-1.7"/>',
  left:'<path d="M14.5 6l-6 6 6 6"/>', right:'<path d="M9.5 6l6 6-6 6"/>', up:'<path d="M6 14.5l6-6 6 6"/>', down:'<path d="M6 9.5l6 6 6-6"/>',
  dleft:'<path d="M12 6l-6 6 6 6"/><path d="M18 6l-6 6 6 6"/>', dright:'<path d="M6 6l6 6-6 6"/><path d="M12 6l6 6-6 6"/>',
  newonly:'<circle cx="12" cy="12" r="4" fill="currentColor" stroke="none"/><circle cx="12" cy="12" r="8.5"/>',
  unres:'<circle cx="12" cy="12" r="8.5"/><path d="M8 16 16 8"/>',
  next:'<circle cx="12" cy="7" r="3" fill="currentColor" stroke="none"/><path d="M12 12.5v8M8.5 17l3.5 3.5 3.5-3.5"/>',
  notes:'<path d="M6 3.5h9l3.5 3.5v13.5H6z"/><path d="M9 11h6M9 14.5h6M9 18h3.5"/>',
  tour:'<circle cx="6" cy="18" r="2.2"/><circle cx="18" cy="6" r="2.2"/><path d="M8 17c6 0 2-9 8-10"/>',
  agent:'<rect x="3.5" y="4.5" width="17" height="15" rx="3"/><path d="M7.5 10l3 2.5-3 2.5"/><path d="M12.5 15.5h4"/>',
  resolve:'<circle cx="12" cy="12" r="8.5"/><path d="M8.3 12.3l2.5 2.5 5-5.4"/>',
  read:'<path d="M2.5 12.5l3.6 3.6L13 9"/><path d="M10.6 16.1l.8.8L20.5 8"/>',
  more:'<circle cx="6" cy="12" r="1.3" fill="currentColor"/><circle cx="12" cy="12" r="1.3" fill="currentColor"/><circle cx="18" cy="12" r="1.3" fill="currentColor"/>',
  close:'<path d="M6 6l12 12M18 6 6 18"/>', plus:'<path d="M12 5v14M5 12h14"/>',
  lock:'<rect x="5.5" y="10.5" width="13" height="9.5" rx="2"/><path d="M8.5 10.5V8a3.5 3.5 0 0 1 7 0v2.5"/>',
  promote:'<path d="M7 17 17 7"/><path d="M9 7h8v8"/>',
  check:'<path d="M5 12.5l4.5 4.5L19 7.5"/>',
  pencil:'<path d="M4 20h4L19 9l-4-4L4 16z"/><path d="M13.5 6.5l4 4"/>',
  ring:'<circle cx="12" cy="12" r="7.5"/>',
  target:'<circle cx="12" cy="12" r="6.5"/><circle cx="12" cy="12" r="1.6" fill="currentColor" stroke="none"/><path d="M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3"/>',
  warn:'<path d="M12 4 21 19.5H3z"/><path d="M12 10v4.5"/><circle cx="12" cy="17.2" r=".6" fill="currentColor"/>',
  grip:'<circle cx="9" cy="7" r="1" fill="currentColor"/><circle cx="15" cy="7" r="1" fill="currentColor"/><circle cx="9" cy="12" r="1" fill="currentColor"/><circle cx="15" cy="12" r="1" fill="currentColor"/><circle cx="9" cy="17" r="1" fill="currentColor"/><circle cx="15" cy="17" r="1" fill="currentColor"/>',
  eye:'<path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="2.8"/>',
  send:'<path d="M4 12 20 4l-4 16-4-7z"/><path d="M12 13l8-9"/>',
  stop:'<rect x="7" y="7" width="10" height="10" rx="2"/>',
  max:'<path d="M14 4h6v6M10 20H4v-6M20 4l-7 7M4 20l7-7"/>',
  back:'<path d="M10 6l-6 6 6 6"/><path d="M4 12h16"/>',
  funnel:'<path d="M4 5h16l-6 7.5V19l-4-2v-4.5z"/>',
  search:'<circle cx="10.5" cy="10.5" r="6"/><path d="M15 15l5 5"/>',
  file:'<path d="M7 3.5h7l4 4V20.5H7z"/><path d="M14 3.5v4h4"/>',
  pr:'<circle cx="6" cy="6" r="2.2"/><circle cx="6" cy="18" r="2.2"/><circle cx="18" cy="18" r="2.2"/><path d="M6 8.2v7.6"/><path d="M18 15.8V10a3 3 0 0 0-3-3h-4"/>',
  play:'<path d="M8 5.5v13l10-6.5z"/>', history:'<path d="M4 12a8 8 0 1 0 2.4-5.7"/><path d="M4 5v4h4"/><path d="M12 8v4.5l3 2"/>',
  link:'<path d="M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1"/><path d="M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1"/>',
  question:'<circle cx="12" cy="12" r="8.5"/><path d="M9.6 9.6a2.5 2.5 0 1 1 3.4 2.3c-.6.3-1 .8-1 1.4v.7"/><circle cx="12" cy="16.8" r=".6" fill="currentColor"/>',
};
const ic = (n, c = "") => `<svg class="i ${c}" viewBox="0 0 24 24" aria-hidden="true">${I[n]}</svg>`;

/* ---------- code: a fixture-style file, highlighted by a small C++ tokenizer ---------- */
const CODE = [
  [412, "    // Scale the logical width into device pixels, once per frame."],
  [413, "    float Renderer::device_scale(const View& view) const"],
  [414, "    {"],
  [415, "        const auto ratio = view.device_pixel_ratio();"],
  [416, "        if (ratio <= 0.0f) {"],
  [417, "            return 1.0f;"],
  [418, "        }"],
  [419, "        return std::clamp(ratio, 1.0f, 4.0f);"],
  [420, "    }"],
  [421, ""],
  [422, "    int Renderer::target_width(const View& view) const"],
  [423, "    {"],
  [424, "        auto width = view.width();", "add"],
  [425, "        if (width <= 0) {", "add"],
  [426, "            return 1;", "add"],
  [427, "        }", "add"],
  [428, "        return scale(width);"],
  [429, "    }"],
  [430, ""],
  [431, "    int Renderer::scale(int width) const"],
  [432, "    {"],
  [433, "        const auto factor = device_scale(*View_);", "add"],
  [434, "        return static_cast<int>(std::lround(width * factor));", "add"],
  [435, "    }"],
  [436, ""],
  [437, "    void Renderer::begin_frame(FrameTiming& timing)"],
  [438, "    {"],
  [439, "        timing.Started = Clock_.now();"],
  [440, "        Retry_.reset(RetryBudget_);", "add"],
  [441, "        for (auto& pass : Passes_) {"],
  [442, "            pass.prepare(Targets_, target_width(*View_));"],
  [443, "        }"],
  [444, "        if (!Retry_.consume()) {", "add"],
  [445, "            Log_.warn(\"retry budget exhausted\");", "add"],
  [446, "            return;", "add"],
  [447, "        }", "add"],
  [448, "        submit(timing);"],
  [449, "    }"],
  [450, ""],
  [451, "    void Renderer::submit(FrameTiming& timing)"],
  [452, "    {"],
  [453, "        Queue_.push(std::move(Commands_));"],
  [454, "        timing.Submitted = Clock_.now();"],
  [455, "        Stats_.record(timing);"],
  [456, "    }"],
];
const KW = /\b(const|auto|if|return|for|void|int|float|static_cast|std|namespace|struct|class|true|false|nullptr)\b/;
function hl(src) {
  if (!src) return "";
  if (/^\s*\/\//.test(src)) return `<span class="cm">${esc(src)}</span>`;
  return src.split(/(\"[^\"]*\"|\b[A-Za-z_][A-Za-z0-9_]*\b|\d+(?:\.\d+)?f?)/).map((p) => {
    if (!p) return "";
    if (p.startsWith('"')) return `<span class="str">${esc(p)}</span>`;
    if (/^\d/.test(p)) return `<span class="nu">${esc(p)}</span>`;
    if (KW.test(p) && p.match(KW)[0] === p) return `<span class="k">${p}</span>`;
    if (/^(Renderer|View|FrameTiming)$/.test(p)) return `<span class="ty">${p}</span>`;
    if (/^(device_scale|target_width|scale|begin_frame|submit|prepare|consume|reset|warn|push|record|now|width|device_pixel_ratio|clamp|lround|move)$/.test(p)) return `<span class="fn">${p}</span>`;
    return esc(p);
  }).join("");
}
/* opts.marks: {line: "thread"|"newthr"|"note"|"stop:N"|"agent"}, opts.cls: {line: "pick"|"write"|"sel"} */
function codeCol(opts = {}) {
  const marks = opts.marks || {}, cls = opts.cls || {}, from = opts.from || 412;
  const rows = CODE.filter((r) => r[0] >= from);
  const pane = (side) => rows.map(([n, src, kind]) => {
    const isAdd = kind === "add";
    if (side === "base" && isAdd) return `<div class="ln"><span class="g"></span><span class="m"></span><span class="t"></span></div>`;
    const bn = side === "base" ? n - rows.filter((r) => r[2] === "add" && r[0] < n).length : n;
    const m = side === "head" ? marks[n] : null;
    let mk = "";
    if (m && m.startsWith("stop:")) mk = `<span class="gm stop">${m.slice(5)}</span>`;
    else if (m) mk = `<span class="gm ${m}"></span>`;
    const c = [side === "head" && isAdd ? "add" : "", side === "head" ? (cls[n] || "") : ""].join(" ");
    return `<div class="ln ${c}"><span class="g">${side === "base" ? bn : n}</span><span class="m">${mk}</span><span class="t">${hl(src)}</span></div>`;
  }).join("");
  const ticks = (opts.ticks || []).map(([top, k]) => `<i class="tk ${k}" style="top:${top}%"></i>`).join("");
  return `<div class="codecol">
    <div class="fhead"><span class="path">src/engine/</span>renderer.cpp</div>
    <div class="panes"><div class="pane">${pane("base")}</div><div class="pane">${pane("head")}</div>
    <div class="ruler"><i class="thumb" style="top:31%;height:14%"></i>${ticks}</div></div></div>`;
}

/* ---------- window chrome ---------- */
function titleBar(o = {}) {
  const modes = o.modes || [["Overview", "⌘1"], ["Review", "⌘2"], ["Discuss", "⌘3"], ["Threads", "⌘4"]];
  const on = o.modality || "Review";
  return `<div class="tbar"><div class="lights"><i></i><i></i><i></i></div>
    <span class="ib">${ic("home")}</span><span class="ib">${ic("repos")}</span><span class="ib ${o.files ? "on" : ""}">${ic("files")}</span>
    <div class="prid"><b>bpr-fixtures/engine #22</b><span>Clamp device scale and bound frame retries</span><span class="ib" style="width:20px">${ic("ext", "s")}</span></div>
    <span class="sp"></span>
    <div class="modes">${modes.map(([n, k, nw]) => `<span class="${n === on ? "on" : ""} ${nw ? "new" : ""}">${n}<kbd>${k}</kbd></span>`).join("")}</div>
    <span class="ib">${ic("refresh")}</span>
    <span class="ib ${o.drawerOn === false ? "" : "on"}">${ic("comments")}<i class="bd">${o.badge ?? 2}</i></span>
    ${o.agentButton || ""}
    <span class="ib">${ic("hide")}</span><span class="ib">${ic("user")}</span><span class="ib">${ic("gear")}</span></div>`;
}
function reviewBar(o = {}) {
  return `<div class="rbar"><span class="seg">${ic("right", "xs")} Updates</span><span class="vsep"></span>
    <span class="seg">Comparing <b>whole PR</b>, Oct 14 → Oct 17 ${ic("down", "xs")}</span><span class="vsep"></span>
    <span class="seg"><b>File 3 of 18</b> ${ic("dleft", "s")} ${ic("dright", "s")}</span><span class="seg"><b>Changes 2 of 5</b> ${ic("left", "s")} ${ic("right", "s")}</span>
    <span class="sp"></span>${o.right ?? `<span class="revpill"><i></i>Review · 2 pending</span><span class="pill outline">Finish review</span><span class="pill quiet">Compose review</span><span class="pill quiet">${ic("comments", "s")}Comment</span>`}</div>`;
}
function win(o) {
  return `<div class="win">${titleBar(o)}${o.noReviewBar ? "" : reviewBar(o.bar || {})}
    <div class="main">${o.left || ""}${o.centre}${o.drawer || ""}</div>
    <div class="sbar">${o.status || "Actionable file 3 of 18. Private progress: unread, not accepted. Review history is ready."}</div></div>`;
}
function drawer(o) {
  const tabs = o.tabs.map((t) => {
    const [name, extra, isOn, icon] = t;
    return `<span class="tab ${isOn ? "on" : ""}">${icon ? ic(icon, "s") : ""}${name}${extra || ""}</span>`;
  }).join("");
  return `<aside class="drawer" style="${o.width ? `width:${o.width}px` : ""}"><div class="dhead">${tabs}<div class="tools">${o.tools || ""}</div></div>
    <div class="dbody">${o.body}</div>${o.foot ? `<div class="dfoot">${o.foot}</div>` : ""}</aside>`;
}
const tool = (n, on = false, title = "") => `<span class="ib ${on ? "on" : ""}" title="${esc(title)}">${ic(n)}</span>`;
const annot = (n, x, y, text) => `<div class="annot" data-n="${n}" style="left:${x}px;top:${y}px">${text}</div>`;

/* A post, in the accepted anatomy: avatar, mark under it, rail in the label's tint. */
function post(who, time, body, o = {}) {
  const label = o.label, weight = o.weight || "normal";
  const name = who === "you" ? "you" : who === "agent" ? (o.agentName || "Agent") : who;
  const pfx = label ? `<span class="pfx">${label}${o.decos ? ` (${o.decos})` : ""}:</span> ` : "";
  return `<div class="post ${weight === "blocking" ? "blk" : ""} ${o.dash ? "dash" : ""}" style="${label ? tint(label) : ""}"><i class="rail"></i>
    <div class="gut">${av(who)}${label ? mark(label, weight) : ""}</div>
    <div class="pb"><div class="nm"><b>${name}</b>${o.unread ? '<span class="dot"></span>' : ""}<time>${time}</time>${o.meta ? `<span class="meta">${o.meta}</span>` : ""}</div>
    <div class="tx">${pfx}${body}</div>${o.after || ""}</div></div>`;
}

/* ---------- page scaffolding: frames scale to the page; ?still=ID&theme=dark shows one ---------- */
function page(o) {
  const q = new URLSearchParams(location.search);
  if (q.get("theme") === "dark") document.body.classList.add("dark");
  if (q.get("annot") === "0") document.body.classList.add("noannot");
  const still = q.get("still");
  const nav = [["index.html", "Summary"], ["principles.html", "Principles"], ["notes.html", "Notes"], ["tours.html", "Tours"], ["agent.html", "Agent"], ["compose.html", "Compose review"], ["overview.html", "Overview panel"]]
    .map(([h, n]) => `<a href="${h}" class="${n === o.nav ? "on" : ""}">${n}</a>`).join("");
  const dirs = o.directions.map((d) => `<section class="dir ${still === d.id ? "only" : ""}" id="${d.id}">
      <div class="dh"><span class="letter">${d.letter}</span><h2>${d.name}</h2></div>
      <div class="framebox"><div class="scaler">${d.frame}</div></div>
      ${d.states ? `<div class="states">${d.states}</div>` : ""}
      <div class="why"><div><h3>The idea</h3>${d.idea}</div><div><h3>Gains</h3><ul>${d.pros.map((p) => `<li>${p}</li>`).join("")}</ul></div><div><h3>Costs</h3><ul>${d.cons.map((p) => `<li>${p}</li>`).join("")}</ul></div></div></section>`).join("");
  document.body.innerHTML = `<div class="page"><header class="top"><h1>${o.title}</h1><nav class="surf">${nav}</nav>
      <span class="themeswitch chip" role="button" tabindex="0" id="themeBtn">Light / Dark</span></header>
      <p class="lede">${o.lede}</p>${o.before || ""}${dirs}${o.after || ""}</div>`;
  if (still) { document.body.classList.add("still"); document.getElementById(still)?.classList.add("only"); }
  document.getElementById("themeBtn").onclick = () => document.body.classList.toggle("dark");
  const fit = () => document.querySelectorAll(".framebox").forEach((b) => {
    if (still) return;
    const s = b.clientWidth / 1440; b.querySelector(".scaler").style.transform = `scale(${s})`; b.style.height = `${900 * s}px`;
  });
  addEventListener("resize", fit); fit();
}
