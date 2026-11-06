"""Installed-widget acceptance, using isolated device profiles from the running host."""
import sys
import time
import traceback
from types import SimpleNamespace

from gi.repository import Gdk, GLib, Gtk

from client import CATALOG, value


def wait_for(predicate, message):
    context = GLib.MainContext.default()
    deadline = time.monotonic() + 12
    while not predicate():
        if time.monotonic() > deadline:
            raise AssertionError(message)
        if not context.iteration(False):
            time.sleep(.001)


class WidgetParity:
    def __init__(self, app):
        self.app = app
        self.client = app.client
        self.commands = set()

    def find(self, page, method):
        return next(b for b in self.app.bindings if b['page'] == page and b['field']['method'] == method)

    def navigate(self, page):
        count = self.client.sent
        index = next(i for i, spec in enumerate(CATALOG['pages']) if spec['id'] == page)
        self.app.notebook.set_current_page(index)
        self.app.refresh()
        assert self.client.sent == count, f'Navigation wrote a command on {page}'

    def action(self, target, method, invoke):
        before = self.client.sent
        invoke()
        assert self.client.sent == before + 1, f'{target}.{method} did not write exactly once'
        wait_for(lambda: not self.app.pending, f'{target}.{method} did not complete')
        assert self.client.ready and not self.client.error, (target, method, self.client.error)
        self.commands.add((target, method))

    def edit(self, binding, new, numeric=False):
        f, w = binding['field'], binding['widget']
        assert w.get_sensitive() and binding['controls'].get_visible(), f
        if value(self.client.state, f) == new:
            return
        def invoke():
            if f['kind'] == 'choice':
                w.set_active(next(i for i, item in enumerate(binding['choices']) if item['value'] == new))
            elif f['kind'] == 'toggle':
                w.set_active(new)
            elif numeric:
                binding['number'].set_value(new)
            else:
                w.set_value(new)
                binding['commit']()
                binding['commit']()
        self.action(f['group'], f['method'], invoke)
        assert value(self.client.state, f) == new, (f, new, value(self.client.state, f))

    def confirm(self, target, method, widget):
        count = self.client.sent
        widget.clicked()
        assert self.app.confirmation is not None
        self.app.confirmation.response(Gtk.ResponseType.CANCEL)
        assert self.app.confirmation is None and self.client.sent == count
        widget.clicked()
        self.action(target, method, lambda: self.app.confirmation.response(Gtk.ResponseType.OK))

    def prerequisite(self, page, method, new):
        binding = self.find(page, method)
        if binding['widget'].get_sensitive() and (not binding['choices'] or
                any(item['value'] == new for item in binding['choices'])):
            self.edit(binding, new)

    def profile(self, profile, locale):
        self.client.argv = [str(self.app.args.backend.resolve()), '--profile', profile, '--locale', locale]
        self.client.restart()
        wait_for(lambda: self.client.ready, f'Could not start {profile}')
        assert self.client.state['app']['previewMode'] and self.app.locale == locale
        count = self.client.sent
        for spec in CATALOG['pages']:
            page = spec['id']
            self.navigate(page)
            for b in self.app.bindings:
                f = b['field']
                if b['page'] != page or f['kind'] == 'label':
                    continue
                if f['key'] in ('ambientLevel', 'focusOnVoice'):
                    self.prerequisite('noise', 'setMode', 2)
                if f['key'] == 'adaptiveSensitivity':
                    self.prerequisite('noise', 'setAdaptiveEnabled', True)
                if f['key'] == 'listeningRoomSize':
                    self.prerequisite('sound', 'setListeningModeEnabled', True)
                if f['key'] in ('speakToChatSensitivity', 'speakToChatModeOut'):
                    self.prerequisite('behavior', 'setSpeakToChatEnabled', True)
                if not b['widget'].get_sensitive():
                    assert b['reason'].get_text(), (profile, page, f)
                    continue
                if not b['controls'].get_visible():
                    assert f['key'] in ('ambientLevel', 'focusOnVoice', 'adaptiveEnabled', 'adaptiveSensitivity',
                                        'listeningRoomSize', 'speakToChatSensitivity', 'speakToChatModeOut',
                                        'touchLeft', 'touchRight'), f
                    continue
                if f['kind'] == 'button':
                    if f['group'] == 'playback' and f['method'] != 'powerOff':
                        self.action(f['group'], f['method'], b['widget'].clicked)
                    continue
                original = value(self.client.state, f)
                if f['kind'] == 'toggle':
                    self.edit(b, not original)
                    self.edit(b, original)
                elif f['kind'] == 'slider':
                    self.edit(b, f['max'] if original == f['min'] else f['min'])
                    self.edit(b, original, numeric=True)
                elif f['kind'] == 'choice':
                    for item in list(b['choices']):
                        self.edit(b, item['value'])
                    if original is not None:
                        self.edit(b, original)
        self.devices()
        self.navigate('home')
        power = self.find('home', 'powerOff')['widget']
        if power.get_sensitive():
            self.confirm('playback', 'powerOff', power)
            assert not power.get_sensitive()
        print(f'Xfce {locale} {profile}: {self.client.sent - count} verified widget writes', flush=True)

    def devices(self):
        self.navigate('devices')
        state = self.client.state['deviceManager']
        if not state.get('supported') or not state.get('devices'):
            return
        def buttons():
            return self.app.device_buttons['devices']
        devices = state['devices']
        for connected in (True, False):
            # The preview protocol accepts a multipoint write asynchronously.
            # Provide each reported row state before using the corresponding button.
            item = self.client.state['deviceManager']['devices'][0]
            item['connected'] = connected
            self.app.refresh()
            method = 'disconnectDevice' if connected else 'connectDevice'
            self.action('deviceManager', method, buttons()[(item['mac'], method)].clicked)
        if len(devices) > 1 and state.get('sourceSwitchSupported'):
            item = self.client.state['deviceManager']['devices'][1]
            item['connected'], item['playback'] = True, False
            self.app.refresh()
            self.action('deviceManager', 'switchDevice', buttons()[(item['mac'], 'switchDevice')].clicked)
        unpair = next(w for (_, method), w in buttons().items() if method == 'unpairDevice')
        self.confirm('deviceManager', 'unpairDevice', unpair)

    def desktop(self):
        from __main__ import APP_ICON, PAGE_ICONS, panel_geometry
        app = self.app
        theme = Gtk.IconTheme.get_default()
        assert app.window.get_icon_name() == APP_ICON and app.quick.get_icon_name() == APP_ICON
        assert theme.lookup_icon(APP_ICON, 32, Gtk.IconLookupFlags.USE_BUILTIN)
        assert len(set(PAGE_ICONS.values())) == len(CATALOG['pages'])
        for spec, label, tab in app.page_titles:
            assert theme.has_icon(PAGE_ICONS[spec['id']]) and label.get_text()
            icon = tab.get_children()[0]
            assert isinstance(icon, Gtk.Image) and icon.get_visible() and tab.get_visible()
        work = SimpleNamespace(x=-1280, y=34, width=1280, height=990)
        for x, expected_x in [(-1270, -1280), (-48, -440)]:
            for y, expected_y in [(2, 34), (1028, 544)]:
                anchor = SimpleNamespace(x=x, y=y, width=20, height=20)
                assert panel_geometry(work, anchor, 440, 480) == (expected_x, expected_y, 440, 480)
                assert panel_geometry(work, anchor, 2000, 2000) == (-1280, 34, 1280, 990)
        count = self.client.sent
        app.quick_button.clicked()
        wait_for(lambda: app.quick.get_mapped(), 'Quick panel did not map')
        surface = app.quick.get_window()
        event = Gdk.Event.new(Gdk.EventType.KEY_PRESS)
        event.keyval, event.window = Gdk.KEY_Escape, surface
        event.set_device(app.quick.get_display().get_default_seat().get_keyboard())
        app.quick.emit('key-press-event', event)
        app.settings_button.clicked()
        app.shortcut('m')
        if app.tray:
            app.tray.emit('activate')
        assert app.quick.get_visible() and app.quick.get_window() == surface
        assert app.window.get_visible() and self.client.sent == count
        mode = self.find('quick', 'setMode')
        picker = mode['widget']
        picker.popup()
        wait_for(lambda: picker.get_property('popup-shown'), 'Quick picker did not open')
        selected = picker.get_active()
        app.refresh()
        assert picker.get_property('popup-shown') and picker.get_active() == selected
        picker.popdown()
        assert app.quick.get_visible() and self.client.sent == count
        self.edit(mode, 2)
        for method in ('setAmbientLevel', 'setVolume', 'setFocusOnVoice'):
            b = self.find('quick', method)
            val = value(self.client.state, b['field'])
            new = (not val if isinstance(val, bool) else
                   b['field']['max'] if val == b['field']['min'] else b['field']['min'])
            self.edit(b, new)
        for method in ('previousTrack', 'nextTrack', 'togglePlayback'):
            self.action('playback', method, self.find('quick', method)['widget'].clicked)
        app.quick_close.clicked()
        assert not app.quick.get_visible() and app.window.get_visible()

    def connection(self):
        self.navigate('connection')
        scan = self.find('connection', 'refreshDevices')['widget']
        stop = self.find('connection', 'disconnectDevice')['widget']
        self.action('app', 'disconnectDevice', stop.clicked)
        before = self.client.sent
        self.app.shortcut('p')
        self.app.shortcut('n')
        assert before == self.client.sent and scan.get_sensitive()
        self.action('app', 'refreshDevices', scan.clicked)
        wait_for(lambda: any(w.get_sensitive() for w in self.app.device_buttons['connection'].values()), 'Discovery pending')
        button = next(w for w in self.app.device_buttons['connection'].values() if w.get_sensitive())
        self.action('app', 'connectDevice', button.clicked)
        wait_for(lambda: self.client.state['connection']['state'] == 'connected', 'Connection pending')
        self.client.state['connection']['state'] = 'initializing'
        self.app.refresh()
        assert not scan.get_sensitive() and stop.get_sensitive()
        assert not any(w.get_sensitive() for w in self.app.device_buttons['connection'].values())
        self.client.state['connection']['state'] = 'connected'
        self.app.refresh()

    def run(self):
        wait_for(lambda: self.client.ready, 'Backend did not start')
        assert self.client.state['app']['previewMode'] and not self.app.args.live
        assert self.client.sent == 0
        result = []
        assert self.client.send(dict(target='app', method='describe', args=[]), result.append)
        wait_for(lambda: result, 'Missing command contract')
        contract = result[0]['contract']
        profiles = contract['profiles']
        assert len(profiles) == 12
        locale = self.app.locale
        for profile in profiles:
            self.profile(profile, locale)
        self.client.argv = [str(self.app.args.backend.resolve()), '--profile', 'WH-1000XM6', '--locale', locale]
        self.client.restart()
        wait_for(lambda: self.client.ready, 'Quick control profile failed to start')
        self.desktop()
        safe = self.find('sound', 'setSafeListeningEnabled')
        self.client.state['settings']['safeListeningStateKnown'] = False
        self.app.refresh()
        assert safe['widget'].get_sensitive() and safe['widget'].get_active() == -1 and safe['reason'].get_text()
        self.edit(safe, False)
        self.connection()
        before = self.client.sent
        self.client.argv[0] += '.does-not-exist'
        self.client.restart()
        wait_for(lambda: self.client.error == 'backend-start-failed', 'Missing backend error not displayed')
        assert self.app.retry.get_visible() and not self.find('home', 'setVolume')['widget'].get_sensitive()
        self.client.argv[0] = str(self.app.args.backend.resolve())
        self.app.retry.clicked()
        wait_for(lambda: self.client.ready, 'Retry failed')
        assert self.client.sent == before and not self.app.retry.get_visible()
        expected = {(op['target'], op['method']) for op in contract['commands']}
        assert self.commands == expected, ('command coverage', expected - self.commands, self.commands - expected)
        print(f'PASS: Xfce GTK {locale}, {len(profiles)} profiles, {len(self.commands)} native commands, '
              'quick controls, unknown state, confirmations, backend failure and retry', flush=True)


def run_parity(app):
    try:
        WidgetParity(app).run()
    except Exception:
        traceback.print_exc(file=sys.stderr)
        app.exit_code = 1
    app.quit()
    return GLib.SOURCE_REMOVE
