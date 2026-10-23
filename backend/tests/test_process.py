"""Exercise the actual private-pipe host, without Bluetooth or user settings."""
import json
import queue
import subprocess
import sys
import threading
import os
import tempfile
import time


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

# Exercise simultaneous fragmented input and backpressured output. Every byte
# must belong to one complete JSON frame, including large describe responses.
process = subprocess.Popen([sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, bufsize=0)
writer_errors = []
def fragmented_writer():
    try:
        for i in range(80):
            data = (json.dumps(dict(id=str(i), target='app', method='describe', args=[])) + '\n').encode()
            for offset in range(0, len(data), 7):
                process.stdin.write(data[offset:offset + 7])
                time.sleep(.0004)
        process.stdin.close()
    except Exception as error:
        writer_errors.append(error)
writer = threading.Thread(target=fragmented_writer, daemon=True)
writer.start()
try:
    pending = b''
    frames = []
    while chunk := process.stdout.read(250):
        time.sleep(.001)
        pending += chunk
        while b'\n' in pending:
            line, pending = pending.split(b'\n', 1)
            frames.append(json.loads(line))
    assert not pending
    assert process.wait(timeout=10) == 0
    writer.join(timeout=5)
    assert not writer.is_alive() and not writer_errors
    assert len(frames) == 161
    for i in range(80):
        assert frames[1 + 2*i]['id'] == str(i) and frames[1 + 2*i]['ok']
        assert frames[2 + 2*i]['type'] == 'state'
finally:
    if process.poll() is None: process.kill(); process.wait()
print('PASS: fragmented bidirectional frames under output backpressure')

if sys.platform.startswith('linux'):
    # No actual system bus or user preferences are accessible in this case.
    with tempfile.TemporaryDirectory() as directory:
        environment = dict(os.environ, XDG_DATA_HOME=directory, XDG_CONFIG_HOME=directory,
                           DBUS_SYSTEM_BUS_ADDRESS='unix:path=' + directory + '/missing-bus')
        result = subprocess.run([sys.argv[1], '--live'], input=b'', capture_output=True,
                                env=environment, timeout=10)
        assert result.returncode == 0, result.stderr
        frames = [json.loads(line) for line in result.stdout.splitlines()]
        assert frames and frames[0]['type'] == 'state'
        assert b'Could not get system bus' in result.stderr
    print('PASS: Linux DBus failure diagnostics do not contaminate protocol stdout')
