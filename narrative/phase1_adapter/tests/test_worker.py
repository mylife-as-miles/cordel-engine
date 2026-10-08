# Copyright (c) 2026 CORDEL contributors. MIT.
"""Real checkout Ren'Py AST execution through the pinned SDK, no display interaction."""
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'narrative/phase1_adapter/protocol'))
from protocol import VERSION, decode, encode

class Peer:
    def __init__(self):
        self.temp = tempfile.TemporaryDirectory(prefix='cordel-worker-')
        self.stderr = open(Path(self.temp.name) / 'stderr.txt', 'w+')
        self.process = subprocess.Popen([str(ROOT / 'lib/py3-linux-x86_64/python'),
            str(ROOT / 'narrative/phase1_adapter/worker/bootstrap.py')], cwd=ROOT,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr)
        self.sequence = 0
        self.buffer = b''
        self.session = 'control'
    def send(self, kind, payload=None, correlation=None, session=None):
        self.sequence += 1
        message = dict(protocol_version=VERSION, session_id=session or self.session,
            message_id=f't-{self.sequence}', sequence=self.sequence, type=kind, payload=payload or {})
        if correlation:
            message['correlation_id'] = correlation
        self.raw(encode(message).encode())
        return message
    def raw(self, line):
        self.process.stdin.write(line);self.process.stdin.flush()
    def receive(self, kind=None):
        deadline = time.monotonic()+5
        while time.monotonic() < deadline:
            if b'\n' not in self.buffer:
                ready, _, _ = select.select([self.process.stdout], [], [], max(0, deadline-time.monotonic()))
                if not ready:
                    break
                data = os.read(self.process.stdout.fileno(), 4096)
                if not data:
                    break
                self.buffer += data
            if b'\n' in self.buffer:
                line, self.buffer = self.buffer.split(b'\n', 1)
                message = decode(line.decode())
                if kind is None or message['type'] == kind:
                    return message
        self.stderr.flush();self.stderr.seek(0)
        raise AssertionError('Worker timed out/EOF: '+str(kind)+'\n'+self.stderr.read()[-3000:])
    def hello(self):
        request=self.send('hello', {'client':'tests'})
        result=self.receive('hello_ack')
        assert result['correlation_id']==request['message_id']
        assert result['payload']['display_started'] is False
    def start(self, scenario='story'):
        self.session = 'test-'+str(self.sequence+1)
        self.send('start_session', {'scenario':scenario})
        return self.receive('session_started')
    def finish(self):
        if self.process.poll() is None:
            try:
                self.send('shutdown', session='control')
                self.receive('shutdown_ack')
                self.process.wait(timeout=3)
            except (BrokenPipeError, AssertionError, subprocess.TimeoutExpired):
                self.process.kill();self.process.wait(timeout=3)
        self.process.stdin.close();self.process.stdout.close();self.stderr.close();self.temp.cleanup()

class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.peer = Peer()
        self.addCleanup(self.peer.finish)
        self.peer.hello()
    def to_event(self):
        self.peer.start();line=self.peer.receive('dialogue');self.peer.send('dialogue_ack', correlation=line['message_id'])
        command=self.peer.receive('narrative_command')
        self.peer.send('command_result', {'success':True,'simulation_tick':1}, command['message_id'])
        self.peer.send('world_fact', {'fact_id':'beacon_enabled','revision':1,'value_type':'boolean','value':True,'simulation_tick':1})
        return self.peer.receive('wait_for_event')
    def to_choice(self):
        wait=self.to_event()
        self.peer.send('gameplay_event', {'event_id':'beacon_reached','simulation_tick':10,'watermark':1}, wait['message_id'])
        line=self.peer.receive('dialogue');self.peer.send('dialogue_ack', correlation=line['message_id'])
        return self.peer.receive('choice')
    def test_both_branches_real_ast(self):
        for branch, counter, text in [('continue',1,'We continue.'),('stay',2,'We stay.')]:
            choice=self.to_choice()
            self.assertEqual([c['id'] for c in choice['payload']['choices']], ['continue','stay'])
            self.peer.send('choice_result', {'choice_id':branch}, choice['message_id'])
            line=self.peer.receive('dialogue');self.assertEqual(line['payload']['text'],text)
            self.peer.send('dialogue_ack', correlation=line['message_id'])
            done=self.peer.receive('session_completed')['payload']
            self.assertEqual(done,dict(outcome=branch,counter=counter,beacon_fact=True))
    def test_malformed_suite_worker_survives(self):
        for line in [b'{broken\n',b'{}\n',b'{"protocol_version":"wrong"}\n']:
            self.peer.raw(line)
            self.assertEqual(self.peer.receive('error')['payload']['code'],'malformed')
        base=dict(protocol_version=VERSION,session_id='control',message_id='bad',sequence=200,type='hello',payload={'client':'tests'})
        for key, value in [('protocol_version','wrong'),('type','unknown'),('payload',[]),('type',None)]:
            m=base.copy();m[key]=value;self.peer.raw((json.dumps(m)+'\n').encode())
            self.assertEqual(self.peer.receive('error')['payload']['code'],'malformed')
        self.assertIsNone(self.peer.process.poll())
    def test_oversized_worker_closes_safely(self):
        self.peer.raw(b'x'*16385+b'\n')
        self.assertEqual(self.peer.receive('error')['payload']['code'],'oversized')
        self.assertEqual(self.peer.process.wait(timeout=3),0)
    def test_invalid_choice_and_correlation(self):
        choice=self.to_choice()
        self.peer.send('choice_result',{'choice_id':'missing'},choice['message_id'])
        self.assertEqual(self.peer.receive('error')['payload']['code'],'choice')
        self.peer.send('choice_result',{'choice_id':'stay'},'unknown')
        self.assertEqual(self.peer.receive('error')['payload']['code'],'correlation')
        self.peer.send('choice_result',{'choice_id':'stay'},choice['message_id'])
        self.assertEqual(self.peer.receive('dialogue')['payload']['text'],'We stay.')
    def test_event_stale_duplicate_sequence(self):
        event=self.to_event()
        self.peer.send('gameplay_event',{'event_id':'other','simulation_tick':1,'watermark':1},event['message_id'])
        self.assertEqual(self.peer.receive('error')['payload']['code'],'event')
        self.peer.send('dialogue_ack',correlation=event['message_id'],session='stale')
        self.assertEqual(self.peer.receive('error')['payload']['code'],'stale_session')
        stale=dict(protocol_version=VERSION,session_id=self.peer.session,message_id='old',sequence=1,type='dialogue_ack',payload={},correlation_id='old')
        self.peer.raw(encode(stale).encode())
        self.assertEqual(self.peer.receive('error')['payload']['code'],'malformed')
    def test_cancel_each_wait(self):
        for wait in ('dialogue','event','choice'):
            if wait=='dialogue':self.peer.start();self.peer.receive('dialogue')
            elif wait=='event':self.to_event()
            else:self.to_choice()
            request=self.peer.send('cancel_session')
            ack=self.peer.receive('session_cancelled')
            self.assertEqual(ack['correlation_id'],request['message_id'])
            self.assertEqual(ack['payload']['pending_count'],0)
    def test_exception_is_protocol_error(self):
        self.peer.start('exception');line=self.peer.receive('dialogue')
        self.peer.send('dialogue_ack',correlation=line['message_id'])
        error=self.peer.receive('error')
        self.assertTrue(error['payload']['fatal'])
        self.assertIn('deterministic narrative exception',error['payload']['detail'])
        self.assertIsNone(self.peer.process.poll())
    def test_shutdown_waiting(self):
        self.peer.start();self.peer.receive('dialogue')
        self.peer.send('shutdown',session='control')
        self.assertEqual(self.peer.receive('shutdown_ack')['payload']['pending_count'],0)
        self.assertEqual(self.peer.process.wait(timeout=3),0)

if __name__=='__main__':
    unittest.main()
