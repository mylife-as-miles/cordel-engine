# Copyright (c) 2026 CORDEL contributors. MIT.
import json
from pathlib import Path
specs = {
    "hello": ({"client": "string"}, False),
    "hello_ack": ({"runtime": "string", "display_started": "boolean"}, True),
    "start_session": ({"scenario": "string"}, False),
    "session_started": ({"label": "string"}, True),
    "dialogue": ({"speaker": "string", "text": "string", "text_id": "string", "emit_time": "number"}, False),
    "dialogue_ack": ({}, True),
    "choice": ({"choices": "array", "emit_time": "number"}, False),
    "choice_result": ({"choice_id": "string"}, True),
    "narrative_command": ({"command": "string", "target": "string", "value": "boolean", "emit_time": "number"}, False),
    "command_result": ({"success": "boolean", "simulation_tick": "integer"}, True),
    "wait_for_event": ({"event_id": "string", "emit_time": "number"}, False),
    "gameplay_event": ({"event_id": "string", "simulation_tick": "integer", "watermark": "integer"}, True),
    "world_fact": ({"fact_id": "string", "revision": "integer", "value_type": "string", "value": "boolean", "simulation_tick": "integer"}, False),
    "resumed": ({"request_type": "string", "round_trip_ms": "number", "resume_time": "number"}, True),
    "session_completed": ({"outcome": "string", "counter": "integer", "beacon_fact": "boolean"}, True),
    "cancel_session": ({}, False), "session_cancelled": ({"pending_count": "integer"}, True),
    "checkpoint_request": ({}, False),
    "checkpoint_data": ({"checkpoint": "object"}, True),
    "checkpoint_restore": ({"checkpoint": "object"}, False),
    "checkpoint_restored": ({"checkpoint": "object"}, True),
    "rollback_request": ({}, False),
    "rollback_rejected": ({"reason": "string"}, True),
    "shutdown": ({}, False), "shutdown_ack": ({"pending_count": "integer"}, True),
    "error": ({"code": "string", "detail": "string", "fatal": "boolean"}, False),
    "diagnostic": ({"index": "integer"}, False),
}
schema = {"protocol_version": "cordel.narrative/0.1", "maximum_line_bytes": 16384,
          "envelope": {"protocol_version": "string", "session_id": "string", "message_id": "string", "type": "string", "payload": "object", "sequence": "integer"},
          "messages": {name: {"required": fields, "correlated": corr} for name, (fields, corr) in specs.items()}}
Path(__file__).resolve().parents[1].joinpath("protocol/schema.json").write_text(json.dumps(schema, indent=2) + "\n")
