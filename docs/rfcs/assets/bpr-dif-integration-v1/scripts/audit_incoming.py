#!/usr/bin/env python3
"""Measure the pack's unmodified incoming PNGs; not a shipping-art validator.

Optional maintainer dependency: Pillow. No repository or provider operations.
Default output is human-readable; --json emits the report as JSON. --output
writes a report only when explicitly requested.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path


def audit(root: Path) -> dict:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError('This optional image audit requires Pillow; pack verification does not.') from exc
    manifest = json.loads((root/'assets/INPUT-MANIFEST.json').read_text(encoding='utf-8'))
    result = []
    for entry in manifest['assets']:
        path = root/entry['path']
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != entry['sha256']:
            raise ValueError('Input hash mismatch: '+entry['path'])
        chunks = []
        offset = 8
        while offset+12 <= len(raw):
            n = struct.unpack('>I', raw[offset:offset+4])[0]
            if offset+n+12 > len(raw):
                raise ValueError('Truncated PNG: '+entry['path'])
            chunks.append(raw[offset+4:offset+8].decode('ascii',errors='replace'))
            offset += n+12
            if chunks[-1] == 'IEND':
                break
        with Image.open(path) as original:
            original.load()
            mode = original.mode
            im = original.convert('RGBA')
        alpha = im.getchannel('A')
        hist = alpha.histogram()
        pixels = im.tobytes()
        hidden_rgb = sum(1 for i in range(0, len(pixels), 4)
                         if pixels[i+3] == 0 and (pixels[i] or pixels[i+1] or pixels[i+2]))
        cells = []
        for i, rect in enumerate(entry['crop_proposals_half_open']):
            cell = im.crop(tuple(rect))
            a = cell.getchannel('A')
            bounds = a.point(lambda v: 255 if v >= 128 else 0).getbbox()
            border = (sum(a.crop((0,0,cell.width,1)).histogram()[1:])+
                      sum(a.crop((0,cell.height-1,cell.width,cell.height)).histogram()[1:])+
                      sum(a.crop((0,1,1,cell.height-1)).histogram()[1:])+
                      sum(a.crop((cell.width-1,1,cell.width,cell.height-1)).histogram()[1:]))
            cells.append({'frame':i,'proposed_rect':rect,'crop_size':list(cell.size),
                          'alpha_ge_128_bounds':list(bounds) if bounds else None,
                          'nonzero_alpha_on_crop_perimeter':border})
        notes=[]
        if not entry['integer_equal_cell_grid']:
            notes.append('Nominal equal cells are fractional. Review explicit rectangles, then pad/register; do not stretch.')
        if hidden_rgb:
            notes.append('RGB exists under zero alpha. Canonicalize transparent RGB; alpha-ignoring previews are misleading.')
        if any(hist[1:9]):
            notes.append('Low-alpha pixels exist. Inspect detached remnants/edges; counts alone do not identify artifacts.')
        notes.append('Body/face identity, foot landmarks, loop seams and transitions require separate validation/review.')
        result.append({'clip_id':entry['clip_id'],'path':entry['path'],'sha256':entry['sha256'],
                       'size':list(im.size),'original_mode':mode,'png_chunk_types':list(dict.fromkeys(chunks)),
                       'alpha_min':alpha.getextrema()[0],'alpha_max':alpha.getextrema()[1],
                       'zero_alpha_pixels':hist[0],'fully_opaque_pixels':hist[255],
                       'alpha_1_through_8_pixels':sum(hist[1:9]),
                       'near_opaque_250_through_255_pixels':sum(hist[250:256]),
                       'nonzero_rgb_under_zero_alpha_pixels':hidden_rgb,
                       'has_actual_transparency':hist[0]>0 and sum(hist[1:])>0,
                       'equal_integer_square_grid':entry['integer_equal_cell_grid'],
                       'cells':cells,'notes':notes})
    return {'schema':'bpr.dif.raw-asset-audit.v1',
            'scope':'Exact incoming bytes and provisional crop measurements only',
            'not_proof_of':'production readiness, art acceptance, native runtime behavior or rights',
            'source_images_modified':False,'assets':result,
            'summary':{'asset_count':len(result),'all_have_actual_alpha':all(a['has_actual_transparency'] for a in result),
                       'fractional_grid_count':sum(not a['equal_integer_square_grid'] for a in result)}}


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--json',action='store_true')
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:
        result=audit(args.root.resolve())
        text=json.dumps(result,indent=2)+'\n'
        if args.output:
            args.output.parent.mkdir(parents=True,exist_ok=True)
            args.output.write_text(text,encoding='utf-8')
        if args.json:
            print(text,end='')
        else:
            print('Incoming Dif image audit — not a production acceptance')
            for a in result['assets']:
                grid='integer square cells' if a['equal_integer_square_grid'] else 'FRACTIONAL nominal grid'
                print(f"  {a['clip_id']}: {a['size'][0]}×{a['size'][1]}, alpha {a['alpha_min']}–{a['alpha_max']}, {grid}")
            print('All ten inputs retained unchanged. Body identity, registration and motion review remain required.')
            if args.output:
                print('Report: '+str(args.output))
        return 0
    except (OSError,ValueError,KeyError,RuntimeError) as exc:
        print('Audit failed: '+str(exc),file=sys.stderr)
        return 1

if __name__=='__main__':
    raise SystemExit(main())
