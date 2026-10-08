# Copyright (c) 2026 CORDEL contributors. MIT.
"""Fixture-scoped adapter: real Ren'Py Context/Say/Menu AST, synchronous worker waits."""
import json
import os
from pathlib import Path
import sys
import time
import traceback
from protocol import VERSION, MAX_LINE, decode, encode


class Cancelled(BaseException):
    pass


class Restore(BaseException):
    def __init__(self, state):
        self.state = state


class ScriptFailure(BaseException):
    pass


class Worker:
    def __init__(self):
        self.output = os.fdopen(os.dup(1), "w", encoding="utf-8", buffering=1)
        self.session = "control"
        self.sequence = 0
        self.peer_sequence = 0
        self.handshake = False
        self.retired_sessions = set()
        self.pending = None
        self.boundary = "cordel_story"
        self.start_id = None
        self.world_effect = False
        self.facts = {}
        self.stop = False
        self.cancel_id = None
        self.trace = None
        destination = os.environ.get("CORDEL_WORKER_TRACE")
        if destination:
            self.trace = open(destination, "w", encoding="utf-8")

    def log(self, event, **fields):
        record = dict(event=event, timestamp=time.monotonic(), session_id=self.session)
        record.update(fields)
        if self.trace:
            self.trace.write(json.dumps(record, ensure_ascii=False) + "\n")
            self.trace.flush()

    def emit(self, kind, payload=None, correlation=None, session=None):
        self.sequence += 1
        message = dict(protocol_version=VERSION, session_id=session or self.session,
                       message_id=f"w-{self.sequence}", sequence=self.sequence,
                       type=kind, payload=payload or {})
        if correlation:
            message["correlation_id"] = correlation
        self.output.write(encode(message))
        self.output.flush()
        # Finite flood deliberately does not create an unbounded diagnostic log.
        if kind != "diagnostic":
            self.log("worker_emit", **message)
        return message["message_id"]

    def error(self, code, detail, fatal=False, correlation=None):
        return self.emit("error", {"code": code, "detail": str(detail)[:1024], "fatal": fatal}, correlation)

    def receive(self):
        # readline size caps memory even before JSON parsing. Oversize kills transport.
        line = sys.stdin.buffer.readline(MAX_LINE + 2)
        if not line:
            self.stop = True
            raise Cancelled()
        if len(line) - 1 > MAX_LINE or not line.endswith(b"\n"):
            self.error("oversized", "framing limit exceeded", True)
            self.stop = True
            raise Cancelled()
        try:
            message = decode(line.decode("utf-8"))
            if message["sequence"] <= self.peer_sequence:
                raise ValueError("stale or out-of-order sequence")
            self.peer_sequence = message["sequence"]
        except (ValueError, UnicodeError, RecursionError) as error:
            self.error("malformed", error)
            return None
        self.log("worker_receive", **message)
        if message["type"] == "shutdown" and message["session_id"] == "control":
            self.stop = True
            self.pending = None
            self.emit("shutdown_ack", {"pending_count": 0}, message["message_id"], "control")
            raise Cancelled()
        if message["type"] == "hello" and message["session_id"] == "control":
            import renpy
            if self.handshake:
                self.error("state", "duplicate handshake")
            else:
                self.handshake = True
                self.emit("hello_ack", {"runtime": renpy.version, "display_started": renpy.game.interface.started},
                          message["message_id"], "control")
            return None
        if not self.handshake:
            self.error("state", "handshake required")
            return None
        if self.start_id and message["session_id"] != self.session:
            self.error("stale_session", "session does not own this wait", correlation=message["message_id"])
            return None
        return message

    def snapshot(self):
        import renpy
        return {"schema": "cordel.fixture-checkpoint/0.1", "session_id": self.session, "label": self.boundary,
                "outcome": renpy.store.outcome, "counter": renpy.store.story_counter,
                "facts": self.facts.copy(), "world_effect": self.world_effect}

    def wait_response(self, kind, payload, response):
        payload["emit_time"] = time.monotonic()
        request = self.emit(kind, payload)
        if kind == "narrative_command" and self.replay:
            self.sequence += 1
            duplicate = dict(protocol_version=VERSION, session_id=self.session, message_id=request,
                             sequence=self.sequence, type=kind, payload=payload)
            self.output.write(encode(duplicate)); self.output.flush()
            self.log("duplicate_command_probe", **duplicate)
            self.emit("dialogue", {"speaker": "probe", "text": "stale", "text_id": "stale",
                                   "emit_time": time.monotonic()}, session="cancelled-stale-session")
        self.pending = request
        started = time.monotonic()
        try:
            while True:
                message = self.receive()
                if not message:
                    continue
                msgtype = message["type"]
                correlation = message.get("correlation_id")
                if msgtype == "cancel_session":
                    self.pending = None
                    self.cancel_id = message["message_id"]
                    raise Cancelled()
                if msgtype == "world_fact":
                    fact = message["payload"]
                    if fact["value_type"] != "boolean" or fact["fact_id"] != "beacon_enabled":
                        self.error("fact", "unknown fact or value type", correlation=message["message_id"])
                        continue
                    old = self.facts.get(fact["fact_id"])
                    if old is None or fact["revision"] > old["revision"]:
                        self.facts[fact["fact_id"]] = fact.copy()
                    continue
                if msgtype == "checkpoint_request":
                    if self.boundary not in ("cordel_choice_boundary", "cordel_final_boundary"):
                        self.error("unsafe_checkpoint", "only labelled safe fixture boundaries", correlation=message["message_id"])
                    else:
                        self.emit("checkpoint_data", {"checkpoint": self.snapshot()}, message["message_id"])
                    continue
                if msgtype == "checkpoint_restore":
                    state = message["payload"]["checkpoint"]
                    if (state.get("schema") != "cordel.fixture-checkpoint/0.1" or state.get("session_id") != self.session or
                            state.get("label") not in ("cordel_choice_boundary", "cordel_final_boundary") or
                            state.get("outcome") not in ("unset", "continue", "stay") or
                            type(state.get("counter")) is not int or not 0 <= state["counter"] <= 2 or
                            type(state.get("facts")) is not dict or state.get("world_effect") is not True):
                        self.error("checkpoint", "invalid fixture state", correlation=message["message_id"])
                        continue
                    self.pending = None
                    self.emit("checkpoint_restored", {"checkpoint": state}, message["message_id"])
                    raise Restore(state)
                if msgtype == "rollback_request":
                    self.emit("rollback_rejected", {"reason": "declared world-effect boundary; general rollback disabled"}, message["message_id"])
                    continue
                if msgtype != response or correlation != request:
                    self.error("correlation", "unrelated, duplicate or unknown response", correlation=message["message_id"])
                    continue
                if kind == "choice" and message["payload"]["choice_id"] not in ("continue", "stay"):
                    self.error("choice", "unknown stable choice ID", correlation=message["message_id"])
                    continue
                if kind == "wait_for_event" and message["payload"]["event_id"] != payload["event_id"]:
                    self.error("event", "unrelated event", correlation=message["message_id"])
                    continue
                self.pending = None
                self.emit("resumed", {"request_type": kind, "round_trip_ms": (time.monotonic()-started)*1000,
                                      "resume_time": time.monotonic()}, request)
                return message["payload"]
        finally:
            self.pending = None

    def say(self, text, **kwargs):
        import renpy
        node = renpy.game.script.lookup(renpy.game.context().current)
        text_id = getattr(node, "identifier", None) or renpy.game.context().translate_identifier
        self.wait_response("dialogue", {"speaker": "Narrator", "text": text, "text_id": text_id or "fixture_line"}, "dialogue_ack")

    def menu(self, items, **kwargs):
        if [text for text, _ in items] != ["Continue", "Stay"]:
            raise RuntimeError("fixture menu shape changed")
        choice = self.wait_response("choice", {"choices": [{"id": "continue", "text": "Continue"},
                                                           {"id": "stay", "text": "Stay"}]}, "choice_result")
        return items[0 if choice["choice_id"] == "continue" else 1][1]

    def command(self, command, target, value):
        result = self.wait_response("narrative_command", {"command": command, "target": target, "value": value}, "command_result")
        if not result["success"]:
            raise RuntimeError("native command rejected: " + target)
        if command == "set_beacon_enabled":
            self.world_effect = True

    def wait(self, event):
        self.wait_response("wait_for_event", {"event_id": event}, "gameplay_event")

    def execute(self, label):
        import renpy
        # Public context call executes Label/Say/Menu/Jump/If/Python/Return nodes.
        # Rollback is disabled; cancellation uses BaseException to avoid error UI.
        active = []
        depth = len(renpy.game.contexts)
        def track_context():
            active.append(renpy.game.context())
        renpy.config.context_callbacks.append(track_context)
        try:
            renpy.game.call_in_new_context(label, _clear_layers=False)
        finally:
            renpy.config.context_callbacks.remove(track_context)
            # Ren'Py run_context cleans dynamics for Exception, but our explicit
            # cancellation/restore uses BaseException to bypass interactive error UI.
            for context in reversed(active):
                context.pop_all_dynamic()
            assert len(renpy.game.contexts) == depth, "narrative context stack leaked"
            self.log("context_unwound", context_depth=depth, dynamic_roots_remaining=sum(len(c.dynamic_stack) for c in active))

    def execute_session(self, message):
        import renpy
        self.session = message["session_id"]
        self.start_id = message["message_id"]
        self.facts = {}
        self.world_effect = False
        self.boundary = "cordel_story"
        renpy.store.outcome = "unset"
        renpy.store.story_counter = 0
        scenario = message["payload"]["scenario"]
        self.replay = scenario == "replay"
        labels = {"story": "cordel_story", "replay": "cordel_story", "exception": "cordel_exception", "invalid_target": "cordel_invalid_target", "pause": "cordel_pause"}
        if scenario not in (*labels, "flood"):
            self.error("scenario", "unknown scenario", True, self.start_id)
            self.start_id = None
            self.session = "control"
            return
        label = labels.get(scenario, "cordel_story")
        self.emit("session_started", {"label": label}, self.start_id)
        def exception_handler(_):
            raise ScriptFailure(traceback.format_exc())
        saved_handler = renpy.config.exception_handler
        renpy.config.exception_handler = exception_handler
        try:
            if scenario == "flood":
                for index in range(4096):
                    self.emit("diagnostic", {"index": index})
            while True:
                try:
                    self.execute(label)
                    break
                except Restore as restored:
                    state = restored.state
                    renpy.store.outcome = state["outcome"]
                    renpy.store.story_counter = state["counter"]
                    self.facts = state["facts"].copy()
                    self.world_effect = state["world_effect"]
                    label = state["label"]
                    self.boundary = label
            self.emit("session_completed", {"outcome": renpy.store.outcome, "counter": renpy.store.story_counter,
                                           "beacon_fact": self.facts.get("beacon_enabled", {}).get("value", False)}, self.start_id)
        except Cancelled:
            if self.cancel_id:
                self.emit("session_cancelled", {"pending_count": 0, "context_depth": len(renpy.game.contexts)}, self.cancel_id)
                self.cancel_id = None
        except (ScriptFailure, Exception) as error:
            self.error("script_exception", error, True, self.start_id)
            self.log("worker_exception", detail=str(error)[:2048])
        finally:
            renpy.config.exception_handler = saved_handler
            self.pending = None
            self.start_id = None
            self.retired_sessions.add(self.session)
            self.session = "control"
            assert not renpy.game.interface.started, "headless adapter started renderer"

    def run(self):
        import renpy
        self.log("worker_started", runtime=renpy.version, display_started=renpy.game.interface.started)
        while not self.stop:
            try:
                message = self.receive()
                if message:
                    if message["type"] == "start_session" and message["session_id"] != "control":
                        if message["session_id"] in self.retired_sessions or len(self.retired_sessions) >= 128:
                            self.error("stale_session", "reused session ID or worker session ledger exhausted", correlation=message["message_id"])
                        else:
                            self.execute_session(message)
                    else:
                        self.error("state", "no matching active session", correlation=message["message_id"])
            except Cancelled:
                break
        self.log("worker_shutdown", pending_count=0, display_started=renpy.game.interface.started)
        if self.trace:
            self.trace.close()
        self.output.close()


class Reference:
    """Ordinary displayed Ren'Py execution of exactly the same fixture script."""
    boundary = "cordel_story"
    def __init__(self):
        self.reference_index = 0 if os.environ.get("CORDEL_REFERENCE_CHOICE", "continue") == "continue" else 1
        self.lines = []
        self.events = []
    def dialogue(self, text):
        self.lines.append(text)
    def command(self, command, target, value):
        self.events.append({"command": command, "target": target, "value": value})
    def wait(self, event):
        self.events.append({"wait": event})
    def finish_reference(self):
        import renpy
        Path(os.environ["CORDEL_REFERENCE_OUTPUT"]).write_text(json.dumps({
            "dialogue": self.lines, "events": self.events, "outcome": renpy.store.outcome,
            "counter": renpy.store.story_counter, "completed": True,
            "display_started": renpy.game.interface.started}, indent=2) + "\n")
