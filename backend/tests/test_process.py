"""Exercise the actual private-pipe host, without Bluetooth or user settings."""
import json
import queue
import subprocess
import sys
import threading


def start(*options):
    process = subprocess.Popen([sys.argv[1], *options], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    lines = queue.Queue()
    def read():
        for line in process.stdout:
            lines.put(json.loads(line))
        lines.put(None)
    threading.Thread(target=read, daemon=True).start()
    return process, lines


process, lines = start('--profile', 'WH-1000XM6')
try:
    state = lines.get(timeout=10)
    assert state['type'] == 'state' and state['app']['previewMode']
    # A burst tests ordered results, request correlation and one state per command.
    for i in range(150):
        process.stdin.write((json.dumps(dict(version=1, id=str(i), target='playback',
                                           method='setVolume', args=[i % 31])) + '\n').encode())
    process.stdin.flush()
    for i in range(150):
        result, state = lines.get(timeout=10), lines.get(timeout=10)
        assert result['id'] == str(i) and result['ok'] and result['status'] == 'accepted', result
        assert state['type'] == 'state' and state['playback']['volume'] == i % 31, state
    for data, code in [(b'not-json\n', 'invalid-json'), (b'[]\n', 'invalid-json'),
                       (b'x' * 70000 + b'\n', 'frame-too-large')]:
        process.stdin.write(data)
        process.stdin.flush()
        assert lines.get(timeout=10)['errorCode'] == code
        assert lines.get(timeout=10)['type'] == 'state'
    # Existing native clients do not send version/id: the v1 envelope stays compatible.
    process.stdin.write(b'{"target":"settings","method":"setDseeEnabled","args":[false]}\n')
    process.stdin.flush()
    assert lines.get(timeout=10)['ok']
    assert not lines.get(timeout=10)['settings']['dseeEnabled']
    process.stdin.write(b'{"target":"app","method":"disconnectDevice","args":[]}')
    process.stdin.close()
    assert lines.get(timeout=10)['ok']
    assert lines.get(timeout=10)['connection']['state'] == 'disconnected'
    assert process.wait(timeout=10) == 0
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
for options in [('--profile', 'missing'), ('--live', '--profile', 'WH-1000XM6'), ('--locale', 'invalid'), ('--theme', 'invalid')]:
    result = subprocess.run([sys.argv[1], *options], input=b'', capture_output=True, timeout=10)
    assert result.returncode == 2, result
print('PASS: ordered commands, legacy envelope, malformed/oversized frames, EOF shutdown, mode isolation')

process, lines = start('--locale', 'zh_CN', '--theme', 'dark')
try:
    state = lines.get(timeout=10)
    assert state['preferences'] == dict(locale='zh_CN', themeMode='dark', persistent=False)
    for key, method, target in [('locale', 'setLocale', 'en'), ('themeMode', 'setThemeMode', 'system')]:
        process.stdin.write((json.dumps(dict(target='preferences', method=method, args=[target])) + '\n').encode())
        process.stdin.flush()
        assert lines.get(timeout=10)['ok']
        state = lines.get(timeout=10)
        assert state['preferences'][key] == target
    process.stdin.close()
    assert process.wait(timeout=10) == 0
finally:
    if process.poll() is None: process.kill(); process.wait()
print('PASS: session overrides and preview preference roundtrip')
