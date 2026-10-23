"""Opt-in hardware acceptance: write, restart host, read from headset, restore.

Never run this in CI or preview. Reports omit addresses and media metadata.
"""
import argparse
import json
from pathlib import Path
import queue
import subprocess
import threading
import time


class Probe:
    def __init__(self, executable, model):
        self.events = queue.Queue()
        self.state = {}
        self.sequence = 0
        self.process = subprocess.Popen([str(executable), '--live'], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        text=True, encoding='utf-8')
        def read():
            for line in self.process.stdout:
                try:
                    self.events.put(json.loads(line))
                except ValueError as error:
                    self.events.put(RuntimeError('Invalid backend JSON: ' + str(error)))
        threading.Thread(target=read, daemon=True).start()
        def drain_errors():
            for _ in self.process.stderr:
                pass
        threading.Thread(target=drain_errors, daemon=True).start()
        try:
            self.wait(lambda e: e.get('type') == 'state')
            self.request('app', 'refreshDevices')
            self.wait(lambda e: bool(e.get('connection', {}).get('devices')))
            matches = [d for d in self.state['connection']['devices'] if d['name'] == model]
            if len(matches) != 1:
                raise RuntimeError('Expected one connected headset matching the requested model')
            self.request('app', 'connectDevice', [model, matches[0]['mac']])
            self.wait(lambda e: e.get('connection', {}).get('state') == 'connected')
            if self.state['app']['previewMode']:
                raise RuntimeError('Hardware acceptance cannot use preview state')
        except BaseException:
            self.close()
            raise

    def wait(self, predicate, timeout=20, required=True):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            try:
                event = self.events.get(timeout=min(1, max(.001, end - time.monotonic())))
            except queue.Empty:
                if self.process.poll() is not None:
                    raise RuntimeError('Backend stopped before completing hardware verification')
                continue
            if isinstance(event, Exception):
                raise event
            if event.get('type') == 'state':
                self.state = event
            if predicate(event):
                return event
        if required:
            raise TimeoutError('No expected headset/backend response')

    def request(self, target, method, args=None):
        self.sequence += 1
        rid = str(self.sequence)
        request = dict(version=1, id=rid, target=target, method=method, args=args or [])
        self.process.stdin.write(json.dumps(request) + '\n')
        self.process.stdin.flush()
        result = self.wait(lambda e: e.get('type') == 'result' and e.get('id') == rid)
        if not result.get('ok'):
            raise RuntimeError(method + ': ' + result.get('errorCode', 'rejected'))
        return result

    def settle(self):
        self.wait(lambda e: False, timeout=3, required=False)

    def close(self):
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()
        try:
            self.process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', type=Path, required=True)
    parser.add_argument('--device', required=True)
    parser.add_argument('--allow-writes', action='store_true', required=True)
    parser.add_argument('--settings', nargs='+', choices=['noise', 'touch', 'multipoint', 'dsee'], required=True)
    parser.add_argument('--report', type=Path, required=True)
    options = parser.parse_args()
    definitions = {
        'noise': ('noiseControl', 'mode', 'setMode', 'supported'),
        'touch': ('settings', 'touchPanelEnabled', 'setTouchPanelEnabled', 'touchPanelSupported'),
        'multipoint': ('settings', 'multipointEnabled', 'setMultipointEnabled', 'multipointSupported'),
        'dsee': ('settings', 'dseeEnabled', 'setDseeEnabled', 'dseeSupported'),
    }
    report = dict(device=options.device, ok=False, tests=[])
    def save():
        options.report.parent.mkdir(parents=True, exist_ok=True)
        options.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    def start():
        # Multipoint changes can deliberately disconnect the control channel.
        # Retry only fresh initialization, never replay a setting command.
        for attempt in range(3):
            try:
                return Probe(options.backend.resolve(), options.device)
            except TimeoutError:
                if attempt == 2:
                    raise
                time.sleep(2)
    probe = None
    try:
        probe = start()
        report['firmware'] = probe.state['playback']['firmware']
        report['capabilities'] = probe.request('app', 'describe')['deviceDiagnostics']
        save()
        for name in options.settings:
            group, field, method, capability = definitions[name]
            if not probe.state[group].get(capability):
                raise RuntimeError(name + ' is not available in the real capability snapshot')
            original = probe.state[group][field]
            row = dict(setting=name, original=original, readbacks=[], restored=False)
            report['tests'].append(row)
            save()  # Preserve recovery value before the first write.
            try:
                targets = [0, 2, 1] if name == 'noise' else [not original]
                for target in targets:
                    probe.request(group, method, [target])
                    probe.settle()
                    row['afterWriteDiagnostics'] = probe.request('app', 'describe')['deviceDiagnostics']
                    save()
                    probe.close()
                    probe = start()  # Fresh process and a fresh Bluetooth initialization.
                    actual = probe.state[group][field]
                    row['readbacks'].append(dict(requested=target, actual=actual))
                    save()
                    print(f'{name}: fresh device readback {actual}, expected {target}', flush=True)
                    if actual != target:
                        raise AssertionError(name + ' was accepted but not applied by the headset')
            finally:
                if probe is None or probe.process.poll() is not None:
                    probe = start()
                probe.request(group, method, [original])
                probe.settle()
                probe.close()
                probe = start()
                row['restored'] = probe.state[group][field] == original
                save()
                print(f'{name}: original value restored={row["restored"]}', flush=True)
                if not row['restored']:
                    raise AssertionError(name + ' original value could not be restored')
        report['ok'] = True
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if probe is not None:
            probe.close()
        save()


if __name__ == '__main__':
    main()
