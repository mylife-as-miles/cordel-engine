# Copyright (c) 2026 CORDEL contributors. MIT.
"""Strict, bounded JSONL envelope shared with the native schema validator."""
import json
import math
from pathlib import Path

VERSION = "cordel.narrative/0.1"
MAX_LINE = 16384
SCHEMA = json.loads(Path(__file__).with_name("schema.json").read_text())


def validate(message):
    bounded(message)
    if not isinstance(message, dict):
        raise ValueError("envelope must be object")
    for key, kind in SCHEMA["envelope"].items():
        check_type(message.get(key), kind, key)
    if message["protocol_version"] != VERSION:
        raise ValueError("incompatible protocol version")
    if message["type"] not in SCHEMA["messages"]:
        raise ValueError("unknown message type")
    for key in ("message_id", "session_id"):
        value = message[key]
        if not value or len(value) > 128 or not value.isascii():
            raise ValueError("invalid stable identifier")
    if message["sequence"] < 1 or message["sequence"] > 2**53 - 1:
        raise ValueError("invalid sequence")
    if "correlation_id" in message:
        check_type(message["correlation_id"], "string", "correlation_id")
        if not message["correlation_id"] or len(message["correlation_id"]) > 128:
            raise ValueError("invalid correlation")
    spec = SCHEMA["messages"][message["type"]]
    if spec["correlated"] and "correlation_id" not in message:
        raise ValueError("missing correlation")
    for key, kind in spec["required"].items():
        check_type(message["payload"].get(key), kind, key)
        if kind == "integer" and message["payload"][key] < 0:
            raise ValueError("negative ordinal: " + key)
    return message


def check_type(value, kind, key):
    valid = {"string": type(value) is str, "boolean": type(value) is bool,
             "integer": type(value) in (int, float) and math.isfinite(value) and value == int(value) and abs(value) <= 2**53 - 1,
             "number": type(value) in (int, float) and math.isfinite(value),
             "object": type(value) is dict, "array": type(value) is list}
    if not valid.get(kind, False):
        raise ValueError("wrong or missing field: " + key)


def bounded(value, depth=0):
    if depth > 32:
        raise ValueError("JSON depth limit exceeded")
    if type(value) is str:
        value.encode("utf-8", errors="strict")
    elif type(value) is int and abs(value) > 2**53 - 1:
        raise ValueError("integer outside exact protocol range")
    elif type(value) is float and not math.isfinite(value):
        raise ValueError("nonfinite number")
    elif type(value) is dict:
        for key, child in value.items():
            bounded(key, depth+1)
            bounded(child, depth+1)
    elif type(value) is list:
        for child in value:
            bounded(child, depth+1)


def decode(line):
    if len(line.encode("utf-8")) > MAX_LINE:
        raise ValueError("oversized message")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate JSON key")
            result[key] = value
        return result
    return validate(json.loads(line, object_pairs_hook=unique,
                               parse_constant=lambda _: (_ for _ in ()).throw(ValueError("nonfinite number"))))


def encode(message):
    line = json.dumps(validate(message), ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    if len(line.encode("utf-8")) > MAX_LINE:
        raise ValueError("oversized message")
    return line + "\n"
