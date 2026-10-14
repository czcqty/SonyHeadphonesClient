"""Exercise every enabled declarative binding against the real C++ host."""
import json
import queue
import subprocess
import sys
import threading
from client import CATALOG, choices, command, enabled, value


count = 0
for profile in ['WH-1000XM6', 'WF-1000XM5', 'WF-L910', 'WF-C510']:
    process = subprocess.Popen([sys.argv[1], '--profile', profile], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
    events = queue.Queue()
    threading.Thread(target=lambda: [events.put(json.loads(line)) for line in process.stdout], daemon=True).start()
    threading.Thread(target=lambda: [None for _ in process.stderr], daemon=True).start()

    def read(kind):
        for _ in range(100):
            event = events.get(timeout=10)
            if event['type'] == kind:
                return event
        raise AssertionError('Missing event: ' + kind)

    try:
        state = read('state')
        assert state['app']['previewMode']
        process.stdin.write(json.dumps(dict(target='app', method='describe', args=[])) + '\n')
        process.stdin.flush()
        contract = read('result')['contract']
        state = read('state')
        bound = {(f['group'], f['method']) for p in CATALOG['pages'] for f in p['fields'] if f['method']}
        bound.add(('app', 'connectDevice'))  # Discovery list selection.
        expected = {(c['target'], c['method']) for c in contract['commands']}
        assert bound == expected, (expected - bound, bound - expected)
        for page in CATALOG['pages']:
            for field in page['fields']:
                if not field['method'] or field['kind'] == 'button' or not enabled(state, field):
                    continue
                current = value(state, field)
                if field['kind'] == 'toggle':
                    target = not current
                elif field['kind'] == 'slider':
                    target = field['min'] if current != field['min'] else field['max']
                else:
                    available = choices(state, field)
                    if not available:
                        continue
                    target = next((item['value'] for item in available if item['value'] != current), available[0]['value'])
                process.stdin.write(json.dumps(command(field, target)) + '\n')
                process.stdin.flush()
                response = read('result')
                assert response['ok'], (profile, page['id'], field, response)
                state = read('state')
                if field['key']:  # Action choices accept a MAC; device acknowledgement is asynchronous.
                    assert value(state, field) == target, (profile, field, value(state, field), target)
                count += 1
        process.stdin.close()
        assert process.wait(timeout=10) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
print(f'Native catalog: PASS ({count} writes across four profiles)')
