"""Opt-in XM5 acceptance: fresh-process readback and verified restoration.

Run with no other live backend. This changes reversible settings, never powers
off/unpairs the headset, and omits addresses, paired-device names and media.
Keep the report: originals are flushed before any write, including on failure.
"""
import argparse
import copy
import json
from pathlib import Path
import time

from verify_live_settings import Probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', type=Path, required=True)
    parser.add_argument('--allow-writes', action='store_true', required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--only', nargs='+')
    args = parser.parse_args()
    report = {'device': 'WH-1000XM5', 'ok': False, 'tests': []}
    probe = None

    def save():
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    def start():
        for attempt in range(3):
            try:
                return Probe(args.backend.resolve(), 'WH-1000XM5')
            except TimeoutError:
                if attempt == 2:
                    raise
                time.sleep(2)

    def restart():
        nonlocal probe
        if probe is not None:
            probe.close()
            probe = None
        probe = start()

    def settle(name):
        # Multipoint's confirmation can arrive after the setting ACK. Keep the
        # control channel alive for that exchange before testing a new session.
        if name == 'multipoint':
            probe.wait(lambda event: False, timeout=8, required=False)
        else:
            probe.settle()

    def value(group, field, index=None):
        v = probe.state[group][field]
        return copy.deepcopy(v if index is None else v[index])

    # Explicit reversible operations; no broad invocation of arbitrary catalog methods.
    cases = [
        ('noise', 'noiseControl', 'mode', 'setMode', 'supported', [0, 2, 1]),
        ('ambient', 'noiseControl', 'ambientLevel', 'setAmbientLevel', 'ambientSupported', [2]),
        ('focus', 'noiseControl', 'focusOnVoice', 'setFocusOnVoice', 'ambientSupported', None),
        ('dsee', 'settings', 'dseeEnabled', 'setDseeEnabled', 'dseeSupported', None),
        ('touch', 'settings', 'touchPanelEnabled', 'setTouchPanelEnabled', 'touchPanelSupported', None),
        ('speak', 'settings', 'speakToChatEnabled', 'setSpeakToChatEnabled', 'speakToChatSupported', None),
        ('sensitivity', 'settings', 'speakToChatSensitivity', 'setSpeakToChatSensitivity', 'speakToChatSupported', [0, 1, 2]),
        ('timeout', 'settings', 'speakToChatModeOut', 'setSpeakToChatModeOut', 'speakToChatSupported', [0, 1, 2, 3]),
        ('wear', 'settings', 'autoPauseEnabled', 'setAutoPauseEnabled', 'autoPauseSupported', None),
        ('auto-off', 'settings', 'autoPowerOffValue', 'setAutoPowerOffValue', 'autoPowerOffSupported', [17, 16]),
        ('nc-button', 'settings', 'ncButtonFunction', 'setNcButtonFunction', 'ncButtonSupported', [1, 2, 3, 4]),
        ('voice', 'settings', 'voiceGuidanceEnabled', 'setVoiceGuidanceEnabled', 'voiceGuidanceSupported', None),
        ('quality', 'settings', 'soundQualityPrior', 'setSoundQualityPrior', 'connectionQualitySupported', None),
        # Safe-listening GET reports availability, not the current mode. Its
        # on/off notifications require a separate same-session acceptance check.
        ('volume', 'playback', 'volume', 'setVolume', 'volumeSupported', 'lower'),
        ('equalizer', 'equalizer', 'bands', 'setBand', 'supported', [1]),
        ('bass', 'equalizer', 'clearBass', 'setClearBass', 'supported', [1]),
        ('preset', 'equalizer', 'presetId', 'selectPreset', 'supported', ['bright', 'flat']),
        ('multipoint', 'settings', 'multipointEnabled', 'setMultipointEnabled', 'multipointSupported', None),
    ]
    try:
        restart()
        report['firmware'] = probe.state['playback']['firmware']
        for name, group, field, method, capability, targets in cases:
            if args.only and name not in args.only:
                continue
            if not probe.state[group].get(capability):
                report['tests'].append({'setting': name, 'skipped': 'Not reported by device'})
                save()
                continue
            original = value(group, field)
            eq = copy.deepcopy(probe.state['equalizer']) if group == 'equalizer' else None
            noise = copy.deepcopy(probe.state['noiseControl']) if name in ('ambient', 'focus') else None
            row = {'setting': name, 'original': original, 'readbacks': [], 'restored': False}
            if eq:
                row['equalizerOriginal'] = {k: eq[k] for k in ('presetId', 'bands', 'clearBass')}
            if noise:
                row['noiseOriginal'] = {k: noise[k] for k in ('mode', 'ambientLevel', 'focusOnVoice')}
            report['tests'].append(row)
            save()
            try:
                if noise:
                    probe.request('noiseControl', 'setMode', [2])
                    probe.settle()
                if targets is None:
                    targets = [not original]
                elif targets == 'lower':
                    targets = [max(0, original - 1)]
                indices = range(len(original)) if name == 'equalizer' else [None]
                for index in indices:
                    for target in targets:
                        request = [index, target] if index is not None else [target]
                        probe.request(group, method, request)
                        settle(name)
                        restart()
                        actual = value(group, field, index)
                        row['readbacks'].append({'index': index, 'requested': target, 'actual': actual})
                        save()
                        print(f'{name}[{index}]: requested={target}, fresh={actual}', flush=True)
                        if actual != target:
                            raise AssertionError('Accepted but not applied by the headset')
            except Exception as error:
                row['error'] = str(error)
                print(f'{name}: FAILED: {error}', flush=True)
            finally:
                # Obtain a fresh channel before restoration; never replay the failed test command.
                restart()
                if eq:
                    for i, band in enumerate(eq['bands']):
                        probe.request(group, 'setBand', [i, band])
                        probe.settle()
                    probe.request(group, 'setClearBass', [eq['clearBass']])
                    probe.settle()
                    probe.request(group, 'selectPreset', [eq['presetId']])
                else:
                    probe.request(group, method, [original])
                settle(name)
                if noise:
                    probe.request('noiseControl', 'setMode', [noise['mode']])
                    probe.settle()
                restart()
                row['restored'] = (all(value(group, k) == eq[k] for k in ('presetId', 'bands', 'clearBass'))
                                   if eq else value(group, field) == original)
                if noise:
                    row['restored'] &= all(value('noiseControl', k) == noise[k] for k in ('mode', 'ambientLevel', 'focusOnVoice'))
                save()
                print(f'{name}: restored={row["restored"]}', flush=True)
                if not row['restored']:
                    raise AssertionError('Restoration failed; stop and recover using saved originals')
        report['ok'] = bool(report['tests']) and all(r.get('restored') and not r.get('error') for r in report['tests'])
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        if probe is not None:
            probe.close()
        save()
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
