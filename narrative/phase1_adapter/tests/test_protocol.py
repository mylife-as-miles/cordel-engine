# Copyright (c) 2026 CORDEL contributors. MIT.
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "protocol"))
from protocol import VERSION, decode, encode

class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.message = dict(protocol_version=VERSION, session_id="control", message_id="n-1", sequence=1,
                            type="hello", payload={"client": "CORDEL é😀"})
    def test_round_trip(self):
        self.assertEqual(decode(encode(self.message)), self.message)
    def test_json_and_framing(self):
        for bad in ('{', '[1,]', 'NaN', '{"x":1,"x":2}', ' ' * 16385):
            with self.subTest(bad=bad[:30]), self.assertRaises(ValueError):
                decode(bad)
    def test_envelope(self):
        for key, value in (("type", None), ("type", "unknown"), ("payload", []), ("sequence", True),
                           ("sequence", 0), ("protocol_version", "wrong"), ("session_id", "")):
            message = copy.deepcopy(self.message);message[key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                encode(message)
    def test_correlation(self):
        self.message.update(type="dialogue_ack", payload={})
        with self.assertRaises(ValueError):
            encode(self.message)
        self.message["correlation_id"] = "w-2"
        self.assertEqual(decode(encode(self.message)), self.message)
    def test_payload(self):
        self.message.update(type="choice_result", payload={"choice_id": 1}, correlation_id="w-2")
        with self.assertRaises(ValueError):
            encode(self.message)

if __name__ == "__main__":
    unittest.main()
