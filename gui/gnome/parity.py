"""Preview-only integration checks against real GTK controls and the private backend."""
import os
import sys
import time
import traceback

from gi.repository import GLib, Gtk
from client import enabled, value


def wait(predicate, message):
    deadline = time.monotonic() + 12
    context = GLib.MainContext.default()
    while not predicate():
        if time.monotonic() >= deadline:
            raise AssertionError(message)
        if not context.iteration(False):
            time.sleep(.002)


def children(widget):
    child = widget.get_first_child()
    while child:
        yield child
        yield from children(child)
        child = child.get_next_sibling()


class Parity:
    def __init__(self, app):
        self.app = app
        self.client = app.client
        self.covered = set()
        self.requests = []
        self.results = []

    def action(self, target, method, perform):
        before = self.client.sent
        perform()
        assert self.client.sent == before + 1, (target, method, 'expected one write', before, self.client.sent)
        wait(lambda: len(self.results) == len(self.requests), 'Missing reply for ' + method)
        request, result = self.requests[-1], self.results[-1]
        assert (request['target'], request['method']) == (target, method), request
        assert result and result['ok'], (request, result)
        assert self.client.sent == before + 1, (method, 'reply generated another write')
        self.covered.add((target, method))

    def field(self, key, page=None):
        return next(b for b in self.app.bindings if b['field']['key'] == key and (page is None or b['page'] == page))

    def edit(self, binding, new, numeric=False):
        field, widget = binding['field'], binding['widget']
        assert binding['row'].get_visible() and widget.is_sensitive(), (field['key'], 'not available')
        kind = field['kind']
        def change():
            if kind == 'toggle':
                widget.set_active(new)
            elif kind == 'choice':
                index = next(i for i, item in enumerate(binding['choices']) if item['value'] == new)
                widget.set_selected(index)
                self.app.refresh()
                assert widget.get_selected() == index, (field['method'], 'old snapshot replaced selection')
            elif numeric:
                binding['number'].set_value(new)
            else:
                widget.set_value(new)
                binding['commit']()
                binding['commit']()
        self.action(field['group'], field['method'], change)
        assert value(self.client.state, field) == new, (field, new, value(self.client.state, field))

    def prerequisite(self, key, setting):
        b = self.field(key)
        if enabled(self.client.state, b['field']) and value(self.client.state, b['field']) != setting:
            self.edit(b, setting)

    def navigate(self, route):
        before = self.client.sent
        self.app.nav.select_row(self.app.nav_rows[route])
        assert self.app.stack.get_visible_child_name() == route
        self.app.refresh()
        assert self.client.sent == before, 'Page navigation wrote settings'

    def check_page(self, route):
        self.navigate(route)
        if route == 'noise':
            if self.client.state['noiseControl']['ambientSupported']:
                self.prerequisite('mode', 2)
            self.prerequisite('adaptiveEnabled', True)
        if route == 'sound':
            self.prerequisite('listeningModeEnabled', True)
        if route == 'behavior':
            self.prerequisite('speakToChatEnabled', True)
        for b in (b for b in self.app.bindings if b['page'] == route):
            f, w = b['field'], b['widget']
            if f['kind'] == 'label' or f['method'] == 'powerOff':
                continue
            if f['key'] == 'listeningRoomSize':
                self.prerequisite('listeningModeEnabled', True)
            cap = f.get('capability')
            current = value(self.client.state, f)
            if cap and not self.client.state[f['group']].get(cap) or ('index' in f and current is None):
                assert not w.is_sensitive() and b['row'].get_subtitle(), (route, f, 'unsupported control')
                continue
            # Ambient controls require ambient mode; some ANC-only profiles lack it.
            assert b['row'].get_visible() and w.is_sensitive(), (route, f['method'], 'unexpectedly unavailable')
            if f['kind'] == 'button':
                self.action(f['group'], f['method'], lambda: w.emit('clicked'))
            elif f['kind'] == 'toggle':
                self.edit(b, not current)
                self.edit(b, current)
            elif f['kind'] == 'slider':
                self.edit(b, f['max'] if current == f['min'] else f['min'])
                self.edit(b, current, numeric=True)
            elif f['kind'] == 'choice':
                for item in list(b['choices']):
                    if item['value'] != value(self.client.state, f):
                        self.edit(b, item['value'])
                if current is not None and current != value(self.client.state, f):
                    self.edit(b, current)

    def click_device(self, route, method, confirm=False):
        actions = self.app.device_lists[route]['actions']
        button = next(button for (_, name), button in actions.items() if name == method and button.is_sensitive())
        target = 'app' if route == 'connection' else 'deviceManager'
        if confirm:
            def respond(response):
                button.emit('clicked')
                assert self.app.confirmation
                self.app.confirmation.response(response)
            before = self.client.sent
            respond('cancel')
            assert self.client.sent == before
            self.action(target, method, lambda: respond('confirm'))
        else:
            self.action(target, method, lambda: button.emit('clicked'))

    def devices(self):
        group = self.client.state['deviceManager']
        if not group['supported']:
            return
        self.click_device('devices', 'disconnectDevice')
        if group['sourceSwitchSupported']:
            self.click_device('devices', 'switchDevice')
        # The preview backend accepts the command without simulating a second host's
        # connection changes. Feed its disconnected report to the presentation only.
        self.client.state['deviceManager']['devices'] = [dict(d, connected=False, playback=False) for d in group['devices']]
        self.app.refresh()
        self.click_device('devices', 'connectDevice')
        self.click_device('devices', 'unpairDevice', confirm=True)

    def power(self):
        self.navigate('home')
        b = next(b for b in self.app.bindings if b['field']['method'] == 'powerOff')
        if not b['widget'].is_sensitive():
            return
        def confirm(response):
            b['widget'].emit('clicked')
            assert self.app.confirmation
            self.app.confirmation.response(response)
        before = self.client.sent
        confirm('cancel')
        assert before == self.client.sent
        self.action('playback', 'powerOff', lambda: confirm('confirm'))
        assert not b['widget'].is_sensitive()

    def check_pointer(self):
        """Opt in only on the isolated Xvfb display created by the package test."""
        import ctypes as c
        import gi
        gi.require_version('GdkX11', '4.0')
        from gi.repository import GdkX11

        assert isinstance(self.app.window.get_display(), GdkX11.X11Display)
        x11 = c.CDLL('libX11.so.6')
        xtest = c.CDLL('libXtst.so.6')
        x11.XOpenDisplay.argtypes = [c.c_char_p]
        x11.XOpenDisplay.restype = c.c_void_p
        x11.XDefaultRootWindow.argtypes = [c.c_void_p]
        x11.XDefaultRootWindow.restype = c.c_ulong
        x11.XTranslateCoordinates.argtypes = [c.c_void_p, c.c_ulong, c.c_ulong, c.c_int, c.c_int,
                                              c.POINTER(c.c_int), c.POINTER(c.c_int), c.POINTER(c.c_ulong)]
        x11.XSync.argtypes = [c.c_void_p, c.c_int]
        x11.XCloseDisplay.argtypes = [c.c_void_p]
        xtest.XTestFakeMotionEvent.argtypes = [c.c_void_p, c.c_int, c.c_int, c.c_int, c.c_ulong]
        xtest.XTestFakeButtonEvent.argtypes = [c.c_void_p, c.c_uint, c.c_int, c.c_ulong]
        display = x11.XOpenDisplay(None)
        assert display

        def settle(seconds=.12):
            x11.XSync(display, False)
            until = time.monotonic() + seconds
            wait(lambda: time.monotonic() >= until, 'Waiting for pointer events')

        def move(widget, fraction):
            ok, box = widget.compute_bounds(self.app.window)
            assert ok and box.get_width() and box.get_height()
            x, y, child = c.c_int(), c.c_int(), c.c_ulong()
            assert x11.XTranslateCoordinates(display, self.app.window.get_surface().get_xid(),
                                             x11.XDefaultRootWindow(display),
                                             round(box.get_x() + fraction * box.get_width()),
                                             round(box.get_y() + box.get_height() / 2),
                                             c.byref(x), c.byref(y), c.byref(child))
            xtest.XTestFakeMotionEvent(display, -1, x.value, y.value, 0)
            settle()

        def click(binding, fraction):
            move(binding['widget'], fraction)
            xtest.XTestFakeButtonEvent(display, 1, True, 0)
            xtest.XTestFakeButtonEvent(display, 1, False, 0)
            settle()

        try:
            self.navigate('home')
            self.app.window.present()
            settle(.3)
            volume = self.field('volume', 'home')
            level = value(self.client.state, volume['field'])
            self.action('playback', 'setVolume', lambda: click(volume, .24))
            clicked = value(self.client.state, volume['field'])
            assert clicked != level
            settle(.4)
            def drag():
                count = self.client.sent
                move(volume['widget'], .24)
                xtest.XTestFakeButtonEvent(display, 1, True, 0)
                for fraction in (.35, .5, .72, .80):
                    move(volume['widget'], fraction)
                    assert self.client.sent == count, 'Slider wrote while the pointer was held'
                xtest.XTestFakeButtonEvent(display, 1, False, 0)
                settle()
            self.action('playback', 'setVolume', drag)
            assert value(self.client.state, volume['field']) > clicked
            self.navigate('sound')
            settle()
            dsee = self.field('dseeEnabled')
            original = value(self.client.state, dsee['field'])
            self.action('settings', 'setDseeEnabled', lambda: click(dsee, .5))
            assert value(self.client.state, dsee['field']) != original
            print('PASS: native pointer click, slider drag/release and switch each issued one command', flush=True)
        finally:
            x11.XCloseDisplay(display)

    def extras(self):
        self.navigate('application')
        for key, target in [('locale', 'zh_CN' if self.app.locale == 'en' else 'en'), ('themeMode', 'dark')]:
            b = self.field(key)
            old = value(self.client.state, b['field'])
            self.edit(b, target)
            self.edit(b, old)
        self.navigate('sound')
        safe = self.field('safeListeningEnabled')
        self.client.state['settings']['safeListeningStateKnown'] = False
        self.app.refresh()
        selected = safe['widget'].get_selected()
        assert safe['choices'][selected]['value'] is None and safe['row'].get_subtitle()
        assert safe['widget'].get_selected_item().get_string() == self.app.tr('State not reported', '状态尚未确认')
        self.edit(safe, False)
        self.navigate('noise')
        picker = self.field('mode')['widget']
        picker.activate()
        popup = next(w for w in children(picker) if isinstance(w, Gtk.Popover))
        wait(popup.get_visible, 'Noise picker did not open')
        model, selected = picker.get_model(), picker.get_selected()
        self.app.refresh()
        assert popup.get_visible() and picker.get_model() == model and picker.get_selected() == selected
        popup.popdown()
        self.app.quick_button.emit('clicked')
        before = self.client.sent
        self.app.lookup_action('panel').activate(None)
        self.app.settings_button.emit('clicked')
        assert self.app.quick.get_visible() and self.client.sent == before
        for method in ('previousTrack', 'togglePlayback', 'nextTrack'):
            b = next(b for b in self.app.bindings if b['page'] == 'quick' and b['field']['method'] == method)
            self.action('playback', method, lambda: b['widget'].emit('clicked'))
        mode = self.field('mode', 'quick')
        if value(self.client.state, mode['field']) != 2:
            self.edit(mode, 2)
        ambient = self.field('ambientLevel', 'quick')
        self.edit(ambient, 1 if ambient['widget'].get_value() != 1 else 2)
        focus = self.field('focusOnVoice', 'quick')
        self.edit(focus, not focus['widget'].get_active())
        self.app.close_button.emit('clicked')
        assert not self.app.quick.get_visible()
        self.navigate('connection')
        for method in ('disconnectDevice', 'refreshDevices'):
            b = next(b for b in self.app.bindings if b['page'] == 'connection' and b['field']['method'] == method)
            assert b['widget'].is_sensitive()
            self.action('app', method, lambda: b['widget'].emit('clicked'))
        self.click_device('connection', 'connectDevice')
        wait(lambda: self.client.state['connection']['state'] == 'connected', 'Discovery did not reconnect')
        self.client.state['connection']['state'] = 'initializing'
        self.app.refresh()
        actions = self.app.device_lists['connection']['actions'].values()
        assert actions and all(not b.is_sensitive() for b in actions)
        disconnect = next(b for b in self.app.bindings if b['page'] == 'connection' and b['field']['method'] == 'disconnectDevice')
        assert disconnect['widget'].is_sensitive()
        self.action('app', 'disconnectDevice', lambda: disconnect['widget'].emit('clicked'))
        exe = self.client.argv[0]
        self.client.argv[0] = exe + '.missing'
        self.client.restart()
        wait(lambda: self.client.error == 'backend-start-failed', 'Missing backend failure was not reported')
        assert self.app.retry.get_visible() and not self.client.state and not self.field('volume')['widget'].is_sensitive()
        self.client.argv[0] = exe
        self.app.retry.emit('clicked')
        wait(lambda: self.client.ready, 'Retry failed')
        assert not self.app.retry.get_visible() and self.field('volume')['widget'].is_sensitive()

    def run(self):
        wait(lambda: self.client.ready, 'Initial backend timeout')
        assert not self.app.args.live and self.client.state['app']['previewMode'] and not self.client.sent
        replies = []
        self.client.send(dict(target='app', method='describe', args=[]), on_result=replies.append)
        wait(lambda: replies, 'Missing API description')
        contract = replies[0]['contract']
        send = self.client.send
        def record(request, on_result=None):
            self.requests.append(request)
            def done(result):
                self.results.append(result)
                if on_result:
                    on_result(result)
            return send(request, on_result=done)
        self.client.send = record
        for profile in contract['profiles']:
            self.client.argv[self.client.argv.index('--profile') + 1] = profile
            self.client.restart()
            wait(lambda: self.client.ready, 'Backend failed to start ' + profile)
            assert self.client.state['app']['previewMode']
            before = len(self.requests)
            if profile == 'WH-1000XM6' and os.environ.get('SOUNDCONNECT_TEST_X11_POINTER') == '1':
                self.check_pointer()
            for page in ('home', 'sound', 'noise', 'equalizer', 'behavior', 'touch', 'voice', 'devices'):
                self.check_page(page)
            self.devices()
            if profile == 'WH-1000XM6':
                self.extras()
            self.power()
            print(f'GNOME {profile}: {len(self.requests)-before} verified native writes', flush=True)
        expected = {(c['target'], c['method']) for c in contract['commands']}
        assert self.covered == expected, expected - self.covered
        assert len(contract['profiles']) == 12 and len(expected) == 45
        print(f'GNOME native parity PASS: 12 profiles, 45 commands, {len(self.requests)} verified writes', flush=True)


def run(app):
    try:
        Parity(app).run()
    except Exception:
        traceback.print_exc(file=sys.stderr)
        app.exit_code = 1
    app.quit()
    return GLib.SOURCE_REMOVE
