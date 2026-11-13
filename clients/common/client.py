"""Shared pipe transport and binding helpers; native widgets live in adapters."""
import argparse
import json
from pathlib import Path
from process_client import ProcessClient


CATALOG = json.loads(Path(__file__).with_name('catalog.json').read_text(encoding='utf-8'))


def options():
    parser = argparse.ArgumentParser()
    parser.add_argument('--backend', type=Path, default=Path('/usr/libexec/sonyheadphonesclient/SoundConnectBackend'))
    parser.add_argument('--profile', default='WH-1000XM6')
    parser.add_argument('--live', action='store_true')
    parser.add_argument('--locale', choices=['en', 'zh_CN'])
    parser.add_argument('--theme', choices=['system', 'light', 'dark'])
    parser.add_argument('--route', choices=[p['id'] for p in CATALOG['pages']], default='home')
    parser.add_argument('--panel', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    parser.add_argument('--parity-test', action='store_true')
    args = parser.parse_args()
    if (args.smoke_test or args.parity_test) and args.live:
        parser.error('Native UI tests require isolated preview mode')
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
    group = state.get(field['group'], {})
    if field.get('knownKey') and not group.get(field['knownKey']):
        return None
    result = group.get(field['key'])
    if 'index' in field:
        return result[field['index']] if isinstance(result, list) and field['index'] < len(result) else None
    return result


def quick_page():
    spec = CATALOG['quick']
    extra = ('previousTrack', 'nextTrack', 'setAmbientLevel', 'setFocusOnVoice')
    by_method = {f['method']: f for page in CATALOG['pages'] for f in page['fields']}
    included = {f['method'] for f in spec['fields']}
    return dict(spec, fields=spec['fields'] + [by_method[m] for m in extra if m not in included])


def visible(state, field):
    key = field['key']
    settings = state.get('settings', {})
    noise = state.get('noiseControl', {})
    playback = state.get('playback', {})
    if key in ('touchLeft', 'touchRight'):
        return not settings.get('touchPanelSupported') or bool(settings.get('touchSupported'))
    if key == 'adaptiveEnabled':
        return bool(noise.get('adaptiveSupported'))
    if key in ('ambientLevel', 'focusOnVoice'):
        return noise.get('mode') == 2
    if key == 'adaptiveSensitivity':
        return bool(noise.get('adaptiveEnabled'))
    if key == 'listeningRoomSize':
        return bool(settings.get('listeningModeEnabled'))
    if key in ('speakToChatSensitivity', 'speakToChatModeOut'):
        return bool(settings.get('speakToChatEnabled'))
    if key in ('batteryRight', 'batteryRightCharging'):
        return bool(playback.get('hasRightBattery'))
    if key in ('batteryCase', 'batteryCaseCharging'):
        return bool(playback.get('hasCaseBattery'))
    if key == 'safeListeningPressure':
        return bool(playback.get('safeListeningSupported'))
    return True


def unavailable_reason(state, field, locale):
    if field['kind'] == 'label':
        return ''
    if not state:
        return dynamic('Waiting for backend state.', locale)
    method = field.get('method')
    group = field['group']
    status = state.get('connection', {}).get('state')
    active = status in ('connected', 'scanning', 'connecting', 'initializing')
    if group in ('app', 'preferences') or field['kind'] == 'devices':
        if (method in ('refreshDevices', 'connectDevice') or field['kind'] == 'devices') and active:
            return dynamic('Cancel or disconnect the current connection first.', locale)
        if method == 'disconnectDevice' and not active:
            return dynamic('No connection is active.', locale)
        return ''
    if status != 'connected':
        return dynamic('Connect headphones on the Connection page first.', locale)
    cap = field.get('capability')
    if cap and not state.get(group, {}).get(cap):
        return dynamic('These headphones do not report support for this feature.', locale)
    if 'index' in field and value(state, field) is None:
        return dynamic('This equalizer band is not provided by these headphones.', locale)
    if method == 'powerOff' and state.get('playback', {}).get('powerOffPending'):
        return dynamic('Powering off…', locale)
    return ''


def enabled(state, field):
    return not unavailable_reason(state, field, 'en')


def choices(state, field):
    group = state.get(field['group'], {})
    if field.get('source'):
        items = group.get(field['source'], [])
        if field['group'] == 'deviceManager':
            method = field['method']
            if method == 'connectDevice':
                items = [item for item in items if not item.get('connected')]
            elif method == 'disconnectDevice':
                items = [item for item in items if item.get('connected')]
            elif method == 'switchDevice':
                items = [item for item in items if item.get('connected') and not item.get('playback')]
        return [dict(value=item[field['valueKey']], label=item['name'] or str(item[field['valueKey']]))
                for item in items]
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
