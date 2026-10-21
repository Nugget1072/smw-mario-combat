"""Summarise rea `analyze-javascript-application` Evidence JSON (as produced for the AS2->.js mirror).

Usage: python -I rea_summarize_js.py <evidence.json> [module-path ...]
Prints global statistics, files rea failed to parse, and for each module path given: the semantic nodes
rea recovered (functions, bindings, call sites, property slots), call/read/write relations and unknowns.
"""
import collections, json, sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
d = json.load(open(sys.argv[1], encoding="utf-8-sig"))
nr = d["normalized_result"]
sg = nr["semantic_graph"]
print("evidence_id:", d["evidence_id"])
print("statistics:", json.dumps(nr["statistics"]))
print("semantic nodes:", len(sg["nodes"]), "relations:", len(sg["relations"]),
      "fingerprints:", len(sg["fingerprints"]), "unknowns:", len(sg["unknowns"]))
print("semantic node kinds:", dict(collections.Counter(n["kind"] for n in sg["nodes"])))
print("relation kinds:", dict(collections.Counter(r["relation"] for r in sg["relations"])))
print("unknown families:", {f"{a}/{b}": c for (a, b), c in collections.Counter((u["family"], u["reason"]) for u in sg["unknowns"]).items()})
for n in nr["graph"]["nodes"]:
    for o in n.get("observations", []):
        p = o.get("properties") or {}
        if p.get("parse_status") == "failed":
            print("PARSE FAILED:", p.get("path"))

byid = {n["node_id"]: n for n in sg["nodes"]}


def mod(n):
    return (n.get("identity") or {}).get("module_path")


def line(n):
    r = (n.get("identity") or {}).get("source_range") or {}
    return (r.get("start") or {}).get("line")


for want in sys.argv[2:]:
    nodes = [n for n in sg["nodes"] if mod(n) == want]
    ids = {n["node_id"] for n in nodes}
    print(f"\n========== {want}: {len(nodes)} semantic nodes", dict(collections.Counter(n["kind"] for n in nodes)))
    for kind in ("module", "function", "parameter", "property-slot", "return-site"):
        for n in nodes:
            if n["kind"] == kind:
                print(f"  {kind:13} L{line(n)!s:5} {n.get('label')}")
    binds = collections.Counter(n.get("label") for n in nodes if n["kind"] == "binding")
    print("  bindings (name x count):", ", ".join(f"{k}x{v}" for k, v in sorted(binds.items())))
    calls = collections.Counter(n.get("label") for n in nodes if n["kind"] == "call-site")
    print("  call sites (label x count):", ", ".join(f"{k}x{v}" for k, v in calls.most_common()))
    lits = collections.Counter(n.get("label") for n in nodes if n["kind"] == "literal")
    print("  literals (top 60):", ", ".join(f"{k}x{v}" for k, v in lits.most_common(60)))
    rels = [r for r in sg["relations"] if r["source_node_id"] in ids or r["target_node_id"] in ids]
    print("  relations:", dict(collections.Counter(r["relation"] for r in rels)))
    for r in rels:
        if r["relation"] in ("calls", "writes-property", "reads-property", "captures", "argument-to-parameter", "writes"):
            s, t = byid.get(r["source_node_id"], {}), byid.get(r["target_node_id"], {})
            print(f"    {r['relation']:22} {s.get('kind')}:{s.get('label')} (L{line(s)}) -> {t.get('kind')}:{t.get('label')} (L{line(t)}) [{r['resolution']}]")
    us = [u for u in sg["unknowns"] if u["node_id"] in ids]
    print("  unknowns:", {f"{a}/{b}": c for (a, b), c in collections.Counter((u["family"], u["reason"]) for u in us).items()})
    for u in us[:3]:
        print("    e.g.", u["family"], u["reason"], "L%s" % line(byid.get(u["node_id"], {})), u["detail"][:150])
    # property access map recovered by rea (reads/writes on this/_parent/_root/Key receivers it could not resolve)
    import re as _re
    acc = collections.defaultdict(set)
    for u in us:
        m = _re.match(r"Static (read|write) property (\S+)", u["detail"])
        if m:
            acc[(m.group(1), m.group(2))].add(line(byid.get(u["node_id"], {})))
    for rw in ("write", "read"):
        items = sorted((k[1], sorted(x for x in v if x)) for k, v in acc.items() if k[0] == rw)
        print(f"  property {rw}s ({len(items)}):")
        for name, lines in items:
            print(f"    {name:28} lines {','.join(map(str, lines))}")
    for f in sg["fingerprints"]:
        if f["function_node_id"] in ids:
            fn = byid[f["function_node_id"]]
            print(f"  fingerprint {fn.get('label')} L{line(fn)} {f['algorithm']} {f['status']} {f['fingerprint_sha256'][:16]}")
