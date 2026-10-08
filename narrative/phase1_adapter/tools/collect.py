# Copyright (c) 2026 CORDEL contributors. MIT.
"""Validate observed story against ordinary Ren'Py and derive one-clock latency summaries."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')


def summarize(samples):
    if not samples:
        return None
    ordered = sorted(samples)
    return dict(count=len(samples), mean_ms=statistics.mean(samples), median_ms=statistics.median(samples),
                worst_ms=max(samples), p95_ms=ordered[min(len(samples)-1, int(.95*len(samples)))] if len(samples)>=20 else None)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('output',type=Path);args=parser.parse_args()
    output=args.output.resolve();root=Path(__file__).resolve().parents[3]
    result=read(output/'scenario-results.json')
    if not result['success'] or not all(c['passed'] for c in result['checks']):
        raise SystemExit('Native narrative gate failed')
    native=[json.loads(line) for line in (output/'native-trace.jsonl').read_text().splitlines()]
    matches=[]
    for branch, session in [('continue','story-continue'),('stay','choice-stay-0')]:
        messages=[r['message'] for r in native if r['event']=='native_receive' and r['message']['session_id']==session]
        lines=[m['payload']['text'] for m in messages if m['type']=='dialogue']
        final=[m['payload'] for m in messages if m['type']=='session_completed'][-1]
        reference=read(output/f'reference-{branch}.json')
        match=(lines==reference['dialogue'] and final['outcome']==reference['outcome'] and
               final['counter']==reference['counter'] and reference['completed'] and reference['display_started'])
        if not match:
            raise SystemExit(f'Ordinary RenPy story meaning differs: {branch}')
        matches.append(dict(branch=branch,dialogue=lines,outcome=final['outcome'],counter=final['counter'],matches_reference=True))
    write(output/'reference-comparison.json',matches)
    samples=read(output/'latency-samples.json')
    groups={}
    for kind in sorted({s['request_type'] for s in samples}):
        subset=[s for s in samples if s['request_type']==kind]
        groups[kind]=dict(worker_emit_to_valid_response_including_deliberate_wait=summarize([s['round_trip_ms'] for s in subset]),
                         native_ack_to_worker_resume_observed=summarize([s['native_ack_to_resume_observed_ms'] for s in subset if 'native_ack_to_resume_observed_ms' in s]))
    write(output/'latency-results.json',dict(samples=len(samples),groups=groups,
        clock_policy='Each round trip is measured on one process monotonic clock. No clock subtraction between processes.',
        interpretation='Worker RTT includes intentional presentation/event delay. Native RTT includes pipe and worker work plus next native queue-consumption interval; not bare wire latency. p95 omitted for groups below 20 samples.'))
    worker_trace=[]
    for path in sorted(output.glob('*/worker-trace.jsonl')):
        for line in path.read_text().splitlines():
            record=json.loads(line);record['worker_run']=path.parent.name;worker_trace.append(record)
    (output/'worker-trace.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False,separators=(',',':'))+'\n' for r in worker_trace))
    files=[p for base in ['narrative/phase1_adapter','native/phase1_host'] for p in (root/base).rglob('*')
           if p.is_file() and p.suffix in ('.py','.cpp','.hpp','.rpy','.sh','.json','.txt') and '__pycache__' not in p.parts
           and not any(part in ('cache','saves') for part in p.parts) and p.name not in ('cgltf.h','log.txt','traceback.txt','errors.txt')]
    write(output/'manifest.json',dict(identity='CORDEL ENGINE 0.1.0-dev',milestone='Phase 1.3 Narrative Ownership',
        evidence_class='Linux SDL offscreen / Mesa llvmpipe software; no visible desktop or physical devices',
        accepted_base='810df7e2d8587a5289cd84873b14467562bef92f',
        protocol='cordel.narrative/0.1',scene='examples/cordel_viewport/game/assets/cordel_scene.gltf',
        source_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)},
        binary_sha256=hashlib.sha256((root/'build/phase1-host/cordel-native-host').read_bytes()).hexdigest(),
        commands=['bash narrative/phase1_adapter/tools/run_offscreen.sh <new-output-dir>',
                  'bash native/phase1_host/tools/run_offscreen.sh <new-output-dir>',
                  'bash examples/cordel_viewport/tools/run_offscreen.sh <new-output-dir>',
                  'bash examples/cordel_viewport/tools/check_baseline.sh <new-output-dir>'],
        required_assertions=result['check_count'],ordinary_reference_branches=2,dependency_changes='none',
        pending=['visible desktop input','physical controllers','hardware timing','other OS transport','in-process CPython','general rollback/save compatibility']))
    print(f"Narrative ownership gate: {result['check_count']} native checks, two ordinary RenPy reference branches; {len(samples)} round-trip samples")

if __name__=='__main__':
    main()
