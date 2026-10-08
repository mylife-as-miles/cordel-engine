# Copyright (c) 2026 CORDEL contributors. MIT.
# Intentionally NOT Ren'Py: a finite malicious transport peer used only in native tests.
import json
import os
import sys
sys.stdin.buffer.readline(16386)
kind = os.environ.get("CORDEL_ADVERSARY", "invalid_json")
message = dict(protocol_version="cordel.narrative/0.1", session_id="control", message_id="w-1", sequence=1,
               type="hello_ack", payload={"runtime": "malicious test peer", "display_started": False}, correlation_id="n-1")
if kind == "invalid_json":
    print('{broken JSON', flush=True)
elif kind == "oversized":
    print('x' * 18000, flush=True)
elif kind == "wrong_version":
    message["protocol_version"] = "wrong"
    print(json.dumps(message), flush=True)
elif kind == "unknown_type":
    message["type"] = "unknown"
    print(json.dumps(message), flush=True)
elif kind == "sequence":
    print(json.dumps(message), flush=True)
    print(json.dumps(message), flush=True)
elif kind == "payload":
    message["payload"] = []
    print(json.dumps(message), flush=True)
else:
    raise SystemExit(2)
# Host must close this wait with bounded process cleanup after detecting corruption.
sys.stdin.buffer.read()
