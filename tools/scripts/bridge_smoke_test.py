"""MCP stdio round-trip through bridge-mcp-ghidra -> GhidraMCP HTTP server.

Run with the bridge's own venv python (it has the `mcp` package):
  $env:GHIDRA_MCP_URL = "http://127.0.0.1:8090"
  tools\\uv-tools\\tools\\ghidra-mcp-bridge\\Scripts\\python.exe -I tools\\scripts\\bridge_smoke_test.py [tool] [json-args]
Default: lists tools, then calls get_metadata and search_strings for 'WIN 32'.
"""
import asyncio, json, os, sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

BRIDGE = os.path.join(os.path.dirname(sys.executable), "bridge-mcp-ghidra.exe")


async def main():
    params = StdioServerParameters(command=BRIDGE, args=["--transport", "stdio"], env=dict(os.environ))
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            tools = (await s.list_tools()).tools
            print(f"bridge exposes {len(tools)} MCP tools; first: {[t.name for t in tools[:12]]}")
            calls = [(sys.argv[1], json.loads(sys.argv[2]) if len(sys.argv) > 2 else {})] if len(sys.argv) > 1 else [
                ("get_metadata", {}),
                ("search_strings", {"search_term": "WIN 32,", "limit": 5}),
            ]
            names = {t.name for t in tools}
            for name, args in calls:
                if name not in names:
                    print(f"[{name}] not exposed by bridge; candidates:", [n for n in names if name.split('_')[-1] in n][:10])
                    continue
                res = await s.call_tool(name, args)
                text = "\n".join(getattr(c, "text", str(c)) for c in res.content)
                print(f"--- {name}({args}):\n{text[:1500]}")


asyncio.run(main())
