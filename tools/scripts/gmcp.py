"""Tiny client for the GhidraMCP HTTP API (headless or GUI plugin).

Usage:
  python gmcp.py schema <substring>                 # list endpoints whose path/description contains substring
  python gmcp.py get  <endpoint> [k=v ...]          # GET  http://127.0.0.1:$GMCP_PORT/<endpoint>?k=v
  python gmcp.py post <endpoint> [k=v ...]          # POST JSON body {"k": "v", ...}
  python gmcp.py postjson <endpoint> '<json>'       # POST JSON body
Env: GMCP_PORT (default 8090), GMCP_OUT (optional file to also save the raw response to).
"""
import json, os, sys, urllib.parse, urllib.request

PORT = os.environ.get("GMCP_PORT", "8090")
BASE = f"http://127.0.0.1:{PORT}"


def call(method, ep, params=None, body=None, timeout=3600):
    url = f"{BASE}/{ep.lstrip('/')}"
    data = None
    headers = {}
    if method == "GET" and params:
        url += "?" + urllib.parse.urlencode(params)
    elif method == "POST":
        if body is not None:
            data = body.encode()
            headers["Content-Type"] = "application/json"
        else:
            data = urllib.parse.urlencode(params or {}).encode()
            headers["Content-Type"] = "application/x-www-form-urlencoded"
    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.read().decode("utf-8", "replace")
    except urllib.error.HTTPError as e:
        return f"HTTP {e.code}: " + e.read().decode("utf-8", "replace")


def kv(args):
    out = {}
    for a in args:
        k, _, v = a.partition("=")
        out[k] = v
    return out


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    cmd = sys.argv[1]
    if cmd == "schema":
        s = json.loads(call("GET", "mcp/schema"))
        q = sys.argv[2].lower() if len(sys.argv) > 2 else ""
        for t in s["tools"]:
            if q in t["path"].lower() or q in t.get("description", "").lower():
                ps = ", ".join(f"{p['name']}{'*' if p.get('required') else ''}({p.get('source')})" for p in t.get("params", []))
                print(f"{t['method']:4} {t['path']}  [{ps}]\n      {t.get('description','')[:200]}")
        return
    if cmd == "get":
        res = call("GET", sys.argv[2], kv(sys.argv[3:]))
    elif cmd == "post":
        # the server's body params are read from a JSON body (form bodies were ignored in testing)
        res = call("POST", sys.argv[2], body=json.dumps(kv(sys.argv[3:])))
    elif cmd == "postjson":
        res = call("POST", sys.argv[2], body=sys.argv[3])
    else:
        raise SystemExit(__doc__)
    if os.environ.get("GMCP_OUT"):
        with open(os.environ["GMCP_OUT"], "w", encoding="utf-8") as f:
            f.write(res)
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print(res)


if __name__ == "__main__":
    main()
