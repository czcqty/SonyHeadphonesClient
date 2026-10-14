"""Shared pipe transport and binding helpers; native widgets live in adapters."""
import argparse
import json
from pathlib import Path
import subprocess
import threading


CATALOG = json.loads(Path(__file__).with_name('catalog.json').read_text(encoding='utf-8'))


def options():
    parser = argparse.ArgumentParser()
    parser.add_argument('--backend', required=True, type=Path)
    parser.add_argument('--profile', default='WH-1000XM6')
    parser.add_argument('--live', action='store_true')
    parser.add_argument('--locale', choices=['en', 'zh_CN'])
    parser.add_argument('--theme', choices=['system', 'light', 'dark'])
    parser.add_argument('--route', choices=[p['id'] for p in CATALOG['pages']], default='home')
    parser.add_argument('--panel', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    args = parser.parse_args()
    if args.smoke_test and args.live:
        parser.error('Smoke testing requires isolated preview mode')
    return args


def title(label, locale):
    return label.get(locale, label.get('en', '')) if isinstance(label, dict) else str(label)


def dynamic(text, locale):
    text = str(text)
    messages = CATALOG['messages']
    if text in messages:
        return title(messages[text], locale)
    for source, labels in messages.items():
        if '%1' in source:
            prefix, suffix = source.split('%1', 1)
            if text.startswith(prefix) and text.endswith(suffix):
                detail = text[len(prefix):len(text)-len(suffix) if suffix else None]
                return title(labels, locale).replace('%1', detail)
    return text  # Device names, metadata and OS diagnostics stay verbatim.


def connection_error(state, locale):
    connection = state.get('connection', {})
    code, detail = connection.get('errorCode', ''), connection.get('errorDetail', '')
    return (dynamic(code, locale) + (': ' + str(detail) if detail else '')) if code else str(detail)


def display(value, locale):
    if value is None: return '—'
    if isinstance(value, bool): return dynamic('Yes' if value else 'No', locale)
    if isinstance(value, list):
        return '\n'.join(str(item.get('name', '')) + (' · ' + dynamic('connected' if item.get('connected') else 'disconnected', locale)
            if 'connected' in item else '') for item in value if isinstance(item, dict)) or '—'
    return dynamic(value, locale)


def value(state, field):
    result = state.get(field['group'], {}).get(field['key'])
    if 'index' in field:
        return result[field['index']] if isinstance(result, list) and field['index'] < len(result) else None
    return result


def enabled(state, field):
    if field['kind'] == 'label' or field['group'] in ('app', 'preferences'):
        return True
    cap = field.get('capability')
    return (state.get('connection', {}).get('state') == 'connected'
            and (not cap or state.get(field['group'], {}).get(cap, False))
            and ('index' not in field or value(state, field) is not None))


def choices(state, field):
    group = state.get(field['group'], {})
    if field.get('source'):
        return [dict(value=item[field['valueKey']], label=item['name'])
                for item in group.get(field['source'], [])]
    return [item for item in field.get('options', [])
            if not item.get('capability') or group.get(item['capability'], False)]


def command(field, new_value=None):
    args = [] if field['kind'] == 'button' else [new_value]
    if 'index' in field:
        args.insert(0, field['index'])
    return dict(target=field['group'], method=field['method'], args=args)


class Client:
    def __init__(self, args, dispatch, changed):
        self.state, self.error, self.sent = {}, '', 0
        self.closed = False
        self.dispatch, self.changed = dispatch, changed
        self.process = None
        try:
            argv = [str(args.backend.resolve())]
            argv += ['--live'] if args.live else ['--profile', args.profile]
            if args.locale: argv += ['--locale', args.locale]
            if args.theme: argv += ['--theme', args.theme]
            self.process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                            stderr=subprocess.PIPE, text=True, encoding='utf-8', bufsize=1)
            threading.Thread(target=self._read, daemon=True).start()
            threading.Thread(target=self._diagnostics, daemon=True).start()
        except OSError as error:
            self.error = str(error)
            dispatch(changed)

    def _diagnostics(self):
        for line in self.process.stderr:
            # Bound memory even for a noisy backend. Device diagnostics stay local.
            self.last_diagnostic = line[-4096:]

    def _read(self):
        for line in self.process.stdout:
            try:
                event = json.loads(line)
                self.dispatch(lambda event=event: self._receive(event))
            except (ValueError, TypeError):
                pass
        self.dispatch(self._stopped)

    def _receive(self, event):
        if self.closed:
            return
        if event.get('type') == 'state':
            if event.get('version') != 1:
                self.error, self.state = 'unsupported-version', {}
            else:
                self.state = event
        elif event.get('type') == 'result':
            self.error = '' if event.get('ok') else event.get('errorCode', 'Command rejected')
        self.changed()

    def _stopped(self):
        if not self.closed:
            self.error, self.state = 'Backend stopped', {}
            self.changed()

    def send(self, request):
        if self.closed or not self.process:
            return
        try:
            request = dict(request, version=1, id=str(self.sent + 1))
            self.process.stdin.write(json.dumps(request) + '\n')
            self.process.stdin.flush()
            self.sent += 1
        except (OSError, ValueError) as error:
            self.error = str(error)
            self.changed()

    def close(self):
        self.closed = True
        if not self.process:
            return
        try:
            self.process.stdin.close()
            self.process.wait(timeout=2)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait()
