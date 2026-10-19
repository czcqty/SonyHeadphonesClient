"""Deterministic child failures and recovery; no Bluetooth or GUI dependencies."""
import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import time
import unittest

from process_client import ProcessClient


def fixture(mode):
    snapshot = {key: {} for key in ('app', 'connection', 'playback', 'noiseControl',
                                   'equalizer', 'settings', 'deviceManager', 'preferences')}
    snapshot.update(type='state', version=1)
    if mode == 'timeout':
        time.sleep(30)
    if mode == 'invalid':
        print('[]', flush=True)
        time.sleep(30)
    if mode == 'version':
        snapshot['version'] = 99
    if mode == 'busy':
        print('Another live backend owns the device session', file=sys.stderr, flush=True)
        return 3
    if mode == 'unicode':
        for byte in '设备诊断'.encode('utf-8'):
            sys.stderr.buffer.write(bytes([byte]))
            sys.stderr.buffer.flush()
            time.sleep(.01)
    if mode == 'noisy':
        sys.stderr.write('x' * 200000 + 'TAIL')
        sys.stderr.flush()
    print(json.dumps(snapshot), flush=True)
    if mode == 'stop':
        time.sleep(.2)
        return 7
    if mode == 'hang':
        time.sleep(30)
    for line in sys.stdin:
        request = json.loads(line)
        print(json.dumps(dict(type='result', ok=True, id=request['id'])), flush=True)
        snapshot['app']['lastId'] = request['id']
        snapshot['app']['writes'] = snapshot['app'].get('writes', 0) + 1
        print(json.dumps(snapshot), flush=True)
    if mode == 'slow-exit':
        time.sleep(.3)
    return 0


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.events = queue.Queue()
        self.clients = []

    def make(self, mode='ready', path=None):
        argv = [str(path)] if path else [sys.executable, str(Path(__file__).resolve()), '--fixture', mode]
        client = ProcessClient(argv, self.events.put, lambda: None)
        self.clients.append(client)
        return client

    def pump(self, predicate, timeout=5):
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            try:
                self.events.get(timeout=.02)()
            except queue.Empty:
                pass
            if predicate():
                return
        self.fail('Timed out waiting for transport state')

    def tearDown(self):
        for client in self.clients:
            process = client._session['process']
            client.close()
            if process:
                process.wait(timeout=5)

    def test_missing_and_start_failure(self):
        client = self.make(path=Path(tempfile.gettempdir()) / 'soundconnect-nonexistent' / 'backend')
        self.pump(lambda: client.error == 'backend-start-failed')
        self.assertFalse(client.ready)
        self.assertFalse(client.send({}))

    def test_timeout(self):
        original = ProcessClient.START_TIMEOUT
        ProcessClient.START_TIMEOUT = .15
        try:
            client = self.make('timeout')
            self.pump(lambda: client.error == 'backend-start-timeout')
            self.assertEqual(client.state, {})
        finally:
            ProcessClient.START_TIMEOUT = original

    def test_protocol_failures(self):
        for mode, error in [('invalid', 'backend-protocol-error'), ('version', 'unsupported-version')]:
            client = self.make(mode)
            self.pump(lambda: client.error == error)
            self.assertFalse(client.ready)
            self.assertEqual(client.state, {})

    def test_busy_diagnostic(self):
        client = self.make('busy')
        self.pump(lambda: client.error == 'backend-stopped')
        self.assertIn('owns the device session', client.diagnostic)

    def test_unicode_diagnostics_across_pipe_reads(self):
        client = self.make('unicode')
        self.pump(lambda: client.ready and client.diagnostic == '设备诊断')

    def test_bounded_diagnostics(self):
        client = self.make('noisy')
        self.pump(lambda: client.ready and client.diagnostic.endswith('TAIL'))
        self.assertLessEqual(len(client.diagnostic), ProcessClient.DIAGNOSTIC_LIMIT)

    def test_ordered_commands_and_restart_without_replay(self):
        client = self.make()
        self.assertFalse(client.send({}))
        self.pump(lambda: client.ready)
        for i in range(20):
            self.assertTrue(client.send(dict(target='app', method='test', args=[i])))
        self.pump(lambda: client.state.get('app', {}).get('lastId') == '20')
        self.assertEqual(client.state['app']['writes'], 20)
        old_session = client._session
        old_process = old_session['process']
        client.restart()
        client._post(old_session, lambda: self.fail('Stale generation callback ran'))
        self.pump(lambda: client.ready)
        self.assertEqual(client.state['app'], {})
        self.assertEqual(client.sent, 20)
        old_process.wait(timeout=5)

    def test_restart_reaps_previous_owner_before_launch(self):
        client = self.make('slow-exit')
        self.pump(lambda: client.ready)
        old_process = client.process
        client.restart()
        self.pump(lambda: client.ready)
        self.assertIsNotNone(old_process.poll(), 'New child raced the old session owner')

    def test_crash_clears_state(self):
        client = self.make('stop')
        self.pump(lambda: client.ready)
        self.pump(lambda: client.error == 'backend-stopped')
        self.assertEqual(client.state, {})
        self.assertFalse(client.send({}))

    def test_close_does_not_block_ui_and_kills_unresponsive_child(self):
        client = self.make('hang')
        self.pump(lambda: client.ready)
        process = client.process
        before = time.monotonic()
        client.close()
        self.assertLess(time.monotonic() - before, .2)
        process.wait(timeout=5)
        self.assertFalse(client.ready)


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--fixture':
        sys.exit(fixture(sys.argv[2]))
    unittest.main()
