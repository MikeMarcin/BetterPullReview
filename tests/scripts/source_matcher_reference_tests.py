#!/usr/bin/env python3
"""Compare public matcher spans with an independent small-input regex oracle.

Python's regex engine is used only as a bounded test reference, never by BPR.
The reference explicitly implements the RFC's zero-width scalar progress rule.
"""
import itertools
import re
import subprocess
import sys

ENGINE = re


def spans(pattern, text):
    compiled = ENGINE.compile("(?m)" + pattern)
    result = []
    cursor = 0
    while cursor <= len(text):
        match = compiled.search(text, cursor)
        if match is None:
            break
        result.append((match.start(), match.end()))
        cursor = match.end()
        if match.start() == match.end():
            cursor += 1
    return result


def main():
    global ENGINE
    if len(sys.argv)>2 and sys.argv[2]=='--re2':
        import re2
        ENGINE = re2
    atoms = ['a', 'b', '.', '[ab]', '[^a]', '(a|ab)', '(ab|a)', '(a|aa)']
    patterns = [atom + suffix for atom in atoms for suffix in ['', '?', '*', '+', '{1,2}', '*?', '+?', '{0,2}?']]
    patterns += ['(a*)*b', '^a*$', r'\ba\b', '(a|b)*a', 'a*ba*', '(ab)*|a', '(a|)b']
    if ENGINE.__name__=='re2':
        # Nullable-loop priority differs in Python; use the actual RE2 oracle.
        patterns += ['(a?|b)*', '(a?)*']
    texts = [''.join(chars) for size in range(6) for chars in itertools.product('ab', repeat=size)]
    texts += ['a\nb', 'a\na', 'b\na\nb', 'aa\n']
    cases = [(pattern, text) for pattern in patterns for text in texts]
    payload = ''.join(f'{pattern.encode().hex()}:{text.encode().hex()}\n' for pattern, text in cases)
    completed = subprocess.run([sys.argv[1], '--reference'], input=payload, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
    lines = completed.stdout.splitlines()
    if len(lines) != len(cases):
        raise AssertionError(f'expected {len(cases)} outputs, received {len(lines)}')
    for (pattern, text), line in zip(cases, lines):
        fields = line.split()
        actual = [tuple(map(int, item.split(','))) for item in fields[1:]]
        expected = spans(pattern, text)
        if fields[0] != '2' or actual != expected:
            raise AssertionError(f'{pattern!r} / {text!r}: got {line}, expected {expected}')
    print(f'source matcher {ENGINE.__name__} reference: {len(cases)} exact span comparisons passed')


if __name__ == '__main__':
    main()
