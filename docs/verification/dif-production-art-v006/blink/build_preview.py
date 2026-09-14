"""Build the self-contained, presentation-only blink operator surface."""
import base64
import hashlib
import json
from pathlib import Path

HERE=Path(__file__).resolve().parent
frames=[HERE/'frames'/f'frame-{i:03d}.png' for i in range(6)]
payload={'images':['data:image/png;base64,'+base64.b64encode(p.read_bytes()).decode() for p in frames],
         'sha256':[hashlib.sha256(p.read_bytes()).hexdigest() for p in frames]}
template='''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dif v006 blink — pending visual review</title>
<style>body{font:15px system-ui;margin:24px;background:#eef0f4;color:#181b22}h1{font-size:23px;margin:0 0 8px}p{max-width:1000px}button{font:inherit;padding:8px 12px;margin:3px;border:1px solid #737782;border-radius:6px;cursor:pointer;background:white}button:focus-visible{outline:3px solid #694bc2}button:disabled{opacity:.5}.row{display:flex;gap:8px;align-items:start;margin:10px 0;flex-wrap:wrap}.tile{display:flex;flex-direction:column;align-items:center;padding:12px;border:1px solid #747984;border-radius:6px;gap:4px}.light{background:#f6f7f9;color:#181b22}.dark{background:#1c2026;color:white}.black{background:black;color:white}canvas{display:block}#observations{font:13px ui-monospace;white-space:pre-wrap}details{max-width:1100px;overflow-wrap:anywhere}#status{font-weight:600}</style>
<h1>Dif · blink v006</h1>
<p>Unaccepted local candidate for BPR-0756. One 600 ms blink: open → half → closed → half → open → open, each frame 100 ms. The accepted neutral supplies all pixels outside the eye interiors and all alpha. Keep browser zoom at 100% to review the labelled CSS sizes.</p>
<p><strong>One visual decision:</strong> Is this six-frame blink ready for the final catalog, with clean purple eye material and a natural close/reopen motion at all four displayed sizes?</p>
<div><button id="play" disabled>Play once · 1×</button><button id="repeat" disabled>Repeat with neutral rests</button><button id="stop">Stop at neutral</button></div>
<div id="poses"></div><p id="status">Loading six pinned frames…</p><div id="surfaces"></div><pre id="observations">No playback observations yet.</pre>
<p>These are browser canvas previews of exact RGBA candidates. They are not Qt runtime evidence. The agent records technical observations; only the owner supplies the visual verdict to the integrator.</p>
<details><summary>Exact frame identities and renderer</summary><pre id="identities"></pre><p>Canvas 2D drawImage from the 256-pixel source; high-quality browser smoothing, no CSS interpolation or transform, no frame blending. requestAnimationFrame chooses floor(elapsed/100) and returns to exact neutral after 600 ms. A repeated preview adds 1000 ms of neutral before and after each blink.</p></details>
<script>
const subject=PAYLOAD;
const sizes=[64,72,96,256], themes=['light','dark','black'];
const canvases=[],images=[];let token=0;
for(const theme of themes){const row=document.createElement('div');row.className='row';for(const size of sizes){const tile=document.createElement('div');tile.className='tile '+theme;const label=document.createElement('span');label.textContent=theme+' · '+size+' × '+size+' CSS px';const c=document.createElement('canvas');c.width=c.height=size;c.style.width=c.style.height=size+'px';c.setAttribute('aria-label',theme+' background, '+size+' pixel blink');tile.append(label,c);row.append(tile);canvases.push(c)}document.querySelector('#surfaces').append(row)}
function draw(i){for(const c of canvases){const ctx=c.getContext('2d');ctx.clearRect(0,0,c.width,c.height);ctx.imageSmoothingEnabled=true;ctx.imageSmoothingQuality='high';ctx.drawImage(images[i],0,0,c.width,c.height)}document.querySelector('#status').textContent='Frame '+i+' · '+(['open','half lowered','closed','half reopened','open','open tail'][i])}
function stop(){token++;if(images.length===6)draw(0)}
async function play(repeat=false){const own=++token;let start=null,last=-1,observed=[];function tick(now){if(own!==token)return;if(start===null)start=now;const elapsed=now-start;if(elapsed>=600){draw(0);document.querySelector('#observations').textContent=JSON.stringify({nominal_frame_ms:100,observed_frames:observed,returned_to_neutral_ms:Math.round(elapsed),missed_frames:[0,1,2,3,4,5].filter(i=>!observed.some(o=>o.frame===i))},null,2);if(repeat)setTimeout(()=>{if(own===token)play(true)},2000);return}const i=Math.floor(elapsed/100);if(i!==last){draw(i);observed.push({frame:i,paint_ms:Math.round(elapsed)});last=i}requestAnimationFrame(tick)}requestAnimationFrame(tick)}
for(let i=0;i<6;i++){const b=document.createElement('button');b.textContent='Show '+i;b.disabled=true;b.onclick=()=>{stop();draw(i)};document.querySelector('#poses').append(b)}
document.querySelector('#stop').onclick=stop;document.querySelector('#play').onclick=()=>play(false);document.querySelector('#repeat').onclick=()=>play(true);
document.addEventListener('visibilitychange',()=>{if(document.hidden)stop()});
document.querySelector('#identities').textContent=subject.sha256.map((h,i)=>i+': '+h).join('\n');
Promise.all(subject.images.map(src=>new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>resolve(im);im.onerror=reject;im.src=src}))).then(all=>{images.push(...all);draw(0);document.querySelectorAll('button').forEach(b=>b.disabled=false)}).catch(()=>document.querySelector('#status').textContent='Frame loading failed; do not review.');
</script></html>'''
(HERE/'preview.html').write_text(template.replace('PAYLOAD',json.dumps(payload)))
print('Built preview.html from six pinned candidate frames')
