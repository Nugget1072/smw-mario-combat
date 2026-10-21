"""Build a JS-parseable mirror of the FFDec AS2 script tree so rea's JavaScript analyzer can parse it.

Usage: python -I as2_to_js_mirror.py <ffdec scripts dir> <out dir> [--normalize]
  - copies every *.as to *.js (same relative path); the source tree is never modified
  - --normalize additionally rewrites a few Flash-4/AS2-only tokens that are not JavaScript, outside string
    literals and comments:  `or`->`||`, `and`->`&&`, `not`->`!`, `<>`->`!=`, `add`->`+`,
    typed declarations `var x:Type` / `):Type` / `(a:Type` -> untyped,
    clip/button handlers `onClipEvent(load){` / `on(press){` -> `function onClipEvent_load(){`.
    AS2 `class a.b.C extends D {...}` packages are left alone (they still fail; rea records the failure).
Writes a package.json whose "main" is the hero script so rea treats the tree as one application.
"""
import os, re, sys

src, dst = sys.argv[1], sys.argv[2]
normalize = "--normalize" in sys.argv

TOK = re.compile(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/)', re.S)


def norm_code(code):
    # FFDec clip/button handlers `onClipEvent(load){` / `on(press){` -> named function declarations
    code = re.sub(r'^(onClipEvent|on)\s*\(([^)]*)\)\s*\{',
                  lambda m: f"function {m.group(1)}_{re.sub(r'[^A-Za-z0-9]+', '_', m.group(2)).strip('_')}(){{",
                  code, flags=re.M)
    code = re.sub(r'\bor\b', '||', code)
    code = re.sub(r'\band\b', '&&', code)
    code = re.sub(r'\bnot\b', '!', code)
    code = re.sub(r'\badd\b', '+', code)
    code = code.replace('<>', '!=')
    code = re.sub(r'(\bvar\s+[A-Za-z_$][\w$]*)\s*:\s*[A-Za-z_$][\w$.]*', r'\1', code)
    code = re.sub(r'\)\s*:\s*[A-Za-z_$][\w$.]*(\s*\{)', r')\1', code)
    code = re.sub(r'([(,]\s*[A-Za-z_$][\w$]*)\s*:\s*[A-Za-z_$][\w$.]*(?=\s*[,)])', r'\1', code)
    return code


def normalize_text(t):
    parts = TOK.split(t)
    return "".join(p if i % 2 else norm_code(p) for i, p in enumerate(parts))


n = 0
for root, _, files in os.walk(src):
    for f in files:
        if not f.endswith(".as"):
            continue
        rel = os.path.relpath(os.path.join(root, f), src)
        out = os.path.join(dst, rel[:-3] + ".js")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(os.path.join(root, f), encoding="utf-8") as fh:
            t = fh.read()
        if normalize:
            t = normalize_text(t)
        with open(out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(t)
        n += 1
with open(os.path.join(dst, "package.json"), "w") as fh:
    fh.write('{"name":"mariocombat-as2-mirror","version":"0.0.0","private":true,'
             '"main":"DefineSprite_440/frame_1/DoAction.js"}\n')
print(f"wrote {n} files to {dst} (normalize={normalize})")
