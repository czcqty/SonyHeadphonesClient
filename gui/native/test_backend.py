"""Contract smoke test for the real shared ViewModel host (no Bluetooth)."""
import json
import queue
import subprocess
import sys
import threading


def check(profile, commands):
    process = subprocess.Popen([sys.argv[1], '--profile', profile], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
    lines = queue.Queue()
    threading.Thread(target=lambda: [lines.put(line) for line in process.stdout], daemon=True).start()

    def read(kind):
        for _ in range(30):
            event = json.loads(lines.get(timeout=10))
            if event['type'] == kind:
                return event
        raise AssertionError('Missing ' + kind)

    try:
        initial = read('state')
        assert initial['app']['previewMode']
        for target, method, args, accepted, expectation in commands:
            process.stdin.write(json.dumps(dict(target=target, method=method, args=args)) + '\n')
            process.stdin.flush()
            result = read('result')
            assert result['ok'] == accepted, (target, method, result)
            state = read('state')
            if expectation:
                group, key, value = expectation
                assert state[group][key] == value, (group, key, state[group][key])
        process.stdin.close()
        assert process.wait(timeout=10) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()


check('WH-1000XM6', [
    ('noiseControl', 'setMode', [2], True, ('noiseControl', 'mode', 2)),
    ('playback', 'setVolume', [12], True, ('playback', 'volume', 12)),
    ('settings', 'setDseeEnabled', [False], True, ('settings', 'dseeEnabled', False)),
    ('equalizer', 'setBand', [0, 4], True, ('equalizer', 'bands', [4, 2, 3, 2, 1])),
    ('noiseControl', 'setMode', ['invalid'], False, ('noiseControl', 'mode', 2)),
    ('noiseControl', 'setMode', [1.5], False, ('noiseControl', 'mode', 2)),
    ('settings', 'deleteLater', [], False, None),
    ('app', 'setProperty', ['locale', 'invalid'], False, None),
])
check('WF-L910', [('noiseControl', 'setMode', [1], False, None)])
check('WF-C510', [('settings', 'setDseeEnabled', [True], False, None)])
print('Native backend contract: PASS (state, commands, rejection, capabilities, orderly exit)')

