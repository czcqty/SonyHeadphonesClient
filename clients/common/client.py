"""Shared pipe transport and binding helpers; native widgets live in adapters."""
import argparse
import json
from pathlib import Path
from process_client import ProcessClient


CATALOG = json.loads(Path(__file__).with_name('catalog.json').read_text(encoding='utf-8'))


def options():
    parser = argparse.ArgumentParser()
    parser.add_argument('--backend', type=Path, default=Path('/usr/libexec/soundconnect/SoundConnectBackend'))
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
    if field['kind'] == 'label':
        return True
    if not state:
        return False
    if field['group'] in ('app', 'preferences'):
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


class Client(ProcessClient):
    def __init__(self, args, dispatch, changed):
        argv = [str(args.backend.resolve())]
        argv += ['--live'] if args.live else ['--profile', args.profile]
        if args.locale: argv += ['--locale', args.locale]
        if args.theme: argv += ['--theme', args.theme]
        super().__init__(argv, dispatch, changed)
