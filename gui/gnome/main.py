#!/usr/bin/env python3
"""GNOME presentation using stock GTK 4/libadwaita widgets."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'clients' / 'common'))
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Adw, Gdk, Gio, GLib, Gtk
from client import (CATALOG, Client, choices, command, connection_error, display, dynamic,
                    enabled, options, quick_page, title, unavailable_reason, value, visible)


APP_ID = 'io.github.czcqty.SoundConnect.gnome'
ICON = 'io.github.czcqty.SoundConnect'
PAGE_ICONS = {
    'home': 'go-home-symbolic', 'connection': 'bluetooth-symbolic',
    'sound': 'audio-volume-high-symbolic', 'noise': 'audio-headphones-symbolic',
    'equalizer': 'audio-card-symbolic', 'behavior': 'power-profile-balanced-symbolic',
    'touch': 'input-touchpad-symbolic', 'voice': 'audio-input-microphone-symbolic',
    'devices': 'preferences-system-devices-symbolic', 'application': 'preferences-system-symbolic',
}


ACTION_ICONS = {
    'previousTrack': 'media-skip-backward-symbolic', 'nextTrack': 'media-skip-forward-symbolic',
    'togglePlayback': 'media-playback-start-symbolic', 'powerOff': 'system-shutdown-symbolic',
    'refreshDevices': 'view-refresh-symbolic', 'disconnectDevice': 'network-offline-symbolic',
    'connectDevice': 'bluetooth-symbolic', 'switchDevice': 'audio-speakers-symbolic',
    'unpairDevice': 'list-remove-symbolic',
}


def app_icon():
    source = Path(__file__).resolve().parents[1] / 'assets/icons/app-icon.svg'
    image = Gtk.Image.new_from_file(str(source)) if source.is_file() else Gtk.Image.new_from_icon_name(ICON)
    image.set_pixel_size(28)
    return image


class App(Adw.Application):
    def __init__(self, args):
        flags = Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        if not args.live:
            flags |= Gio.ApplicationFlags.NON_UNIQUE
        super().__init__(application_id=APP_ID, flags=flags)
        self.add_main_option('panel', 0, GLib.OptionFlags.NONE, GLib.OptionArg.NONE, 'Open quick controls', None)
        self.connect('command-line', self.command_line)
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.locale = args.locale or 'en'
        self.theme = None
        self.page_titles, self.nav_rows, self.device_lists = [], {}, {}
        self.pending = {}
        self.confirmation = None
        self.connect('activate', self.activate_ui)
        self.connect('shutdown', lambda *_: self.client and self.client.close())

    def tr(self, en, zh):
        return zh if self.locale == 'zh_CN' else en

    def command_line(self, _, invocation):
        panel = invocation.get_options_dict().contains('panel')
        if self.client:
            (self.quick if panel else self.window).present()
        else:
            self.args.panel = panel
            self.activate()
        return 0

    def activate_ui(self, *_):
        if self.client:
            self.window.present()
            return
        Gtk.Window.set_default_icon_name(ICON)
        self.window = Adw.ApplicationWindow(application=self, title='Sound Connect', default_width=960, default_height=740)
        self.window.set_size_request(370, 340)
        self.window.connect('close-request', self.close_main)
        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        header.pack_start(app_icon())
        self.nav_button = Gtk.Button(icon_name='open-menu-symbolic', visible=False)
        self.nav_button.connect('clicked', lambda *_: self.split.set_show_sidebar(True))
        header.pack_start(self.nav_button)
        self.quick_label = Gtk.Label()
        quick_face = Gtk.Stack(hhomogeneous=False, vhomogeneous=False)
        quick_face.add_named(self.quick_label, 'label')
        quick_face.add_named(Gtk.Image.new_from_icon_name('multimedia-volume-control-symbolic'), 'icon')
        self.quick_button = Gtk.Button(child=quick_face)
        self.quick_button.connect('clicked', lambda *_: self.quick.present())
        header.pack_end(self.quick_button)
        toolbar.add_top_bar(header)
        self.status = Gtk.Label(wrap=True, margin_start=12, margin_end=12, margin_top=6, margin_bottom=6)
        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        content.append(self.status)
        self.retry = Gtk.Button(label=dynamic('backend-retry', self.locale))
        self.retry.connect('clicked', lambda *_: self.client.restart())
        content.append(self.retry)
        self.stack = Gtk.Stack(hexpand=True, vexpand=True)
        self.nav = Gtk.ListBox(selection_mode=Gtk.SelectionMode.SINGLE)
        self.nav.add_css_class('navigation-sidebar')
        scroll = Gtk.ScrolledWindow(hscrollbar_policy=Gtk.PolicyType.NEVER, child=self.nav)
        self.split = Adw.OverlaySplitView(sidebar=scroll, content=self.stack, vexpand=True,
                                          min_sidebar_width=180, max_sidebar_width=230)
        breakpoint = Adw.Breakpoint.new(Adw.BreakpointCondition.parse('max-width: 650sp'))
        breakpoint.add_setter(self.split, 'collapsed', True)
        breakpoint.add_setter(self.nav_button, 'visible', True)
        breakpoint.add_setter(quick_face, 'visible-child-name', 'icon')
        self.window.add_breakpoint(breakpoint)
        content.append(self.split)
        toolbar.set_content(content)
        self.window.set_content(toolbar)
        for page in CATALOG['pages']:
            route = page['id']
            label = title(page['label'], self.locale)
            self.stack.add_titled(self.make_page(page), route, label)
            row = Adw.ActionRow(title=label, activatable=True)
            row.add_prefix(Gtk.Image.new_from_icon_name(PAGE_ICONS[route]))
            self.nav.append(row)
            self.nav_rows[route] = row
        self.nav.connect('row-selected', self.navigate)
        self.nav.connect('row-activated', self.navigate)
        self.nav.select_row(self.nav_rows[self.args.route])
        self.quick = Adw.ApplicationWindow(application=self, title=self.tr('Quick controls', '快捷控制'), default_width=460, default_height=550)
        quick_toolbar = Adw.ToolbarView()
        quick_header = Adw.HeaderBar(show_start_title_buttons=False, show_end_title_buttons=False)
        quick_header.pack_start(app_icon())
        self.close_button = Gtk.Button(icon_name='window-close-symbolic')
        self.close_button.connect('clicked', self.close_quick)
        quick_header.pack_end(self.close_button)
        self.settings_button = settings = Gtk.Button(label=self.tr('All settings', '全部设置'))
        settings.connect('clicked', self.show_settings)
        quick_header.pack_start(settings); quick_toolbar.add_top_bar(quick_header)
        quick_content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.quick_status = Gtk.Label(wrap=True, margin_start=12, margin_end=12, margin_top=6, margin_bottom=6)
        quick_content.append(self.quick_status)
        panel_page = self.make_page(quick_page())
        panel_page.set_vexpand(True)
        quick_content.append(panel_page)
        quick_toolbar.set_content(quick_content); self.quick.set_content(quick_toolbar)
        self.quick.connect('close-request', lambda *_: True)
        for name, key, callback in [('play', 'p', lambda: self.playback_action('togglePlayback')),
                                    ('next', 'n', lambda: self.playback_action('nextTrack')),
                                    ('panel', 'm', self.quick.present)]:
            action = Gio.SimpleAction.new(name, None)
            action.connect('activate', lambda *_, cb=callback: cb())
            self.add_action(action)
            self.set_accels_for_action('app.' + name, ['<Control><Alt>' + key])
        quit_action = Gio.SimpleAction.new('quit', None)
        quit_action.connect('activate', lambda *_: self.quit())
        self.add_action(quit_action)
        self.set_accels_for_action('app.quit', ['<Control>q'])
        self.retranslate()
        self.client = Client(self.args, self.dispatch, self.refresh)
        self.refresh()
        (self.quick if self.args.panel else self.window).present()
        if self.args.smoke_test:
            GLib.timeout_add_seconds(3, self.smoke)
        elif self.args.parity_test:
            from parity import run
            GLib.idle_add(run, self)

    @staticmethod
    def dispatch(callback):
        def run():
            callback()
            return GLib.SOURCE_REMOVE
        GLib.idle_add(run)

    def navigate(self, _, row):
        if row is None:
            return
        route = next(key for key, item in self.nav_rows.items() if item == row)
        self.stack.set_visible_child_name(route)
        if self.split.get_collapsed():
            self.split.set_show_sidebar(False)

    def show_settings(self, *_):
        self.window.present()

    def close_main(self, *_):
        if self.quick.get_visible():
            self.window.set_visible(False)
        else:
            self.quit()
        return True

    def close_quick(self, *_):
        self.quick.set_visible(False)
        if not self.window.get_visible():
            self.show_settings()
        return True

    def playback_action(self, method):
        field = next(b['field'] for b in self.bindings if b['field']['method'] == method)
        self.send(field)

    @staticmethod
    def edit_key(request):
        args = request['args']
        index = args[0] if request['method'] == 'setBand' else None
        return request['target'], request['method'], index

    def submit(self, request):
        key = self.edit_key(request)
        if key in self.pending or not self.client.ready:
            return
        self.pending[key] = request
        def finished(_):
            self.pending.pop(key, None)
            self.refresh()
        if self.client.send(request, on_result=finished):
            self.refresh()
        else:
            finished(None)

    def send(self, field, new_value=None):
        if self.syncing or not self.client or not enabled(self.client.state, field):
            return
        request = command(field, new_value)
        if field['method'] in ('powerOff', 'unpairDevice'):
            self.confirm_command(request, field['method'] == 'powerOff')
        else:
            self.submit(request)

    def confirm_command(self, request, power):
        if self.confirmation:
            self.confirmation.present()
            return self.confirmation
        dialog = Adw.MessageDialog.new(self.window,
            self.tr('Power off headphones?' if power else 'Unpair device?', '是否关闭耳机电源？' if power else '是否取消设备配对？'),
            self.tr('The Bluetooth connection will be closed.', '蓝牙连接将会断开。'))
        dialog.add_response('cancel', self.tr('Cancel', '取消'))
        dialog.add_response('confirm', self.tr('Power off' if power else 'Unpair', '关机' if power else '取消配对'))
        dialog.set_response_appearance('confirm', Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response('cancel'); dialog.set_close_response('cancel')
        def respond(_, response):
            self.confirmation = None
            if response == 'confirm' and self.client.ready:
                self.submit(request)
        dialog.connect('response', respond)
        self.confirmation = dialog
        dialog.present()
        return dialog

    def make_page(self, spec):
        page = Adw.PreferencesPage(title=title(spec['label'], self.locale))
        group = Adw.PreferencesGroup(title=title(spec['label'], self.locale))
        page.add(group)
        self.page_titles.append((spec, page, group))
        for field in spec['fields']:
            if field['kind'] == 'devices' or (spec['id'] == 'devices' and
                                               (field['key'] == 'devices' or field.get('source') == 'devices')):
                continue
            label = title(field['label'], self.locale)
            kind = field['kind']
            row = Adw.ActionRow(title=label, use_markup=False)
            binding = dict(page=spec['id'], field=field, row=row, widget=None, choices=[], dirty=False)
            if kind == 'toggle':
                widget = Gtk.Switch(valign=Gtk.Align.CENTER)
                row.add_suffix(widget); row.set_activatable_widget(widget)
                widget.connect('notify::active', lambda w, _, f=field: self.send(f, w.get_active()))
            elif kind == 'slider':
                widget = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, field['min'], field['max'], 1)
                widget.set_digits(0)
                widget.set_hexpand(True)
                widget.set_size_request(70, -1)
                number = Gtk.SpinButton.new_with_range(field['min'], field['max'], 1)
                number.set_valign(Gtk.Align.CENTER)
                number.set_width_chars(3)
                binding['number'] = number
                number.connect('value-changed', lambda n, f=field: self.send(f, n.get_value_as_int()))
                widget.set_tooltip_text(label); row.add_suffix(widget)
                row.add_suffix(number)
                def changed(w, b=binding):
                    if not self.syncing:
                        b['dirty'] = True
                def commit(*_, b=binding):
                    if b['dirty'] and not self.syncing:
                        b['dirty'] = False
                        self.send(b['field'], round(b['widget'].get_value()))
                    return False
                widget.connect('value-changed', changed)
                pointer = Gtk.EventControllerLegacy()
                pointer.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
                def pointer_event(_, event, cb=commit):
                    if event.get_event_type() in (Gdk.EventType.BUTTON_RELEASE, Gdk.EventType.TOUCH_END):
                        GLib.idle_add(cb)
                    return False
                pointer.connect('event', pointer_event); widget.add_controller(pointer)
                keys = Gtk.EventControllerKey(); keys.connect('key-released', commit); widget.add_controller(keys)
                focus = Gtk.EventControllerFocus(); focus.connect('leave', commit); widget.add_controller(focus)
                scroll = Gtk.EventControllerScroll(flags=Gtk.EventControllerScrollFlags.BOTH_AXES)
                scroll.set_propagation_phase(Gtk.PropagationPhase.CAPTURE)
                def scrolled(*_, cb=commit):
                    GLib.idle_add(cb)
                    return False
                scroll.connect('scroll', scrolled); widget.add_controller(scroll)
                binding['commit'] = commit
            elif kind == 'choice':
                row = Adw.ComboRow(title=label, use_markup=False)
                widget = row; binding['row'] = row
                def selected(w, _, b=binding):
                    index = w.get_selected()
                    if self.syncing or index >= len(b['choices']):
                        return
                    item = b['choices'][index]
                    if item['value'] is not None and item['value'] != value(self.client.state, b['field']):
                        self.send(b['field'], item['value'])
                row.connect('notify::selected', selected)
            elif kind == 'button':
                widget = Gtk.Button(icon_name=ACTION_ICONS[field['method']], valign=Gtk.Align.CENTER,
                                    tooltip_text=label)
                widget.connect('clicked', lambda _, f=field: self.send(f))
                row.add_suffix(widget); row.set_activatable_widget(widget)
            else:
                widget = row
                if spec['id'] == 'connection' and field['key'] == 'state':
                    self.progress = Gtk.Spinner(valign=Gtk.Align.CENTER)
                    row.add_suffix(self.progress)
            binding['widget'] = widget; self.bindings.append(binding); group.add(row)
        if spec['id'] in ('devices', 'connection'):
            devices = Adw.PreferencesGroup()
            page.add(devices)
            self.device_lists[spec['id']] = dict(group=devices, rows=[], actions={}, signature=None)
        if spec['id'] == 'application':
            info = Adw.ActionRow()
            group.add(info)
            self.preference_info = info
            self.update_preference_info()
        return page

    def refresh_devices(self, route, state):
        listed = self.device_lists[route]
        discovering = route == 'connection'
        connection = state.get('connection', {})
        status = connection.get('state')
        connected = status == 'connected'
        busy = status in ('connecting', 'initializing', 'scanning')
        group = connection if discovering else state.get('deviceManager', {})
        devices = group.get('devices', []) if discovering or connected else []
        fields = next(p['fields'] for p in CATALOG['pages'] if p['id'] == route)
        signature = (devices, self.client.ready, status, bool(connection_error(state, self.locale)),
                     group.get('supported'), group.get('sourceSwitchSupported'), self.locale,
                     tuple(k for k in self.pending if k[0] in ('deviceManager', 'app')))
        if signature == listed['signature']:
            return
        listed['signature'] = signature
        group_widget = listed['group']
        expanded = {row.device_mac for row in listed['rows'] if isinstance(row, Adw.ExpanderRow) and row.get_expanded()}
        for row in listed['rows']:
            group_widget.remove(row)
        listed['rows'].clear()
        listed['actions'].clear()
        group_widget.set_title(self.tr('Available headphones', '可用耳机') if discovering
                               else self.tr('Paired devices', '已配对设备'))
        for device in devices:
            mac = device['mac']
            name = device['name'] or mac
            row = Adw.ExpanderRow(title=name, subtitle=mac, use_markup=False)
            row.device_mac = mac
            row.set_expanded(discovering or mac in expanded)
            listed['rows'].append(row)
            group_widget.add(row)
            def add_action(request, label, sensitive):
                button = Gtk.Button(icon_name=ACTION_ICONS[request['method']], tooltip_text=label,
                                    valign=Gtk.Align.CENTER)
                action_row = Adw.ActionRow(title=label, use_markup=False)
                action_row.add_suffix(button)
                action_row.set_activatable_widget(button)
                row.add_row(action_row)
                action_row.set_sensitive(sensitive and self.edit_key(request) not in self.pending)
                button.connect('clicked', lambda *_: self.confirm_command(request, False)
                               if request['method'] == 'unpairDevice' else self.submit(request))
                listed['actions'][(mac, request['method'])] = button
            if discovering:
                add_action(dict(target='app', method='connectDevice', args=[device['name'], mac]),
                           self.tr('Connect', '连接'), self.client.ready and not (busy or connected))
            else:
                playing = self.tr('Current playback device', '当前播放设备') if device.get('playback') else ''
                status_text = dynamic('connected' if device.get('connected') else 'disconnected', self.locale)
                row.set_subtitle(' · '.join(filter(None, (mac, status_text, playing))))
                for field in fields:
                    if field.get('source') != 'devices':
                        continue
                    method = field['method']
                    eligible = choices(state, field)
                    if any(item['value'] == mac for item in eligible):
                        add_action(command(field, mac), title(field['label'], self.locale), enabled(state, field))
        if not devices:
            if discovering:
                hint = self.tr('Connect your headphones in Bluetooth settings, then refresh.',
                               '请先在蓝牙设置中连接耳机，然后刷新。') if not busy and not connected and self.client.ready and not connection_error(state, self.locale) else ''
            else:
                hint = (self.tr('The headphones have not reported paired devices.', '耳机尚未报告已配对设备。')
                        if connected else self.tr('Connect headphones to manage paired devices.', '连接耳机后可管理已配对设备。'))
            if hint:
                row = Adw.ActionRow(title=hint)
                listed['rows'].append(row)
                group_widget.add(row)

    def refresh(self):
        if not self.client:
            return
        self.syncing = True
        state = self.client.state
        if self.confirmation and state.get('connection', {}).get('state') != 'connected':
            self.confirmation.response('cancel')
        prefs = state.get('preferences', {})
        locale = prefs.get('locale', self.locale)
        if locale != self.locale:
            self.locale = locale
            self.retranslate()
        self.apply_theme(prefs.get('themeMode', self.theme or 'system'))
        message = (dynamic('Backend error: %1', self.locale).replace('%1', dynamic(self.client.error, self.locale)) if self.client.error else '') or connection_error(state, self.locale) or self.tr('Live device' if self.args.live else 'Isolated device preview', '实时设备' if self.args.live else '独立设备预览')
        if self.client.diagnostic and not self.client.ready:
            message += '\n' + self.client.diagnostic
        self.status.set_label(message)
        self.quick_status.set_label(message)
        busy = state.get('connection', {}).get('state') in ('scanning', 'connecting', 'initializing')
        self.progress.set_visible(busy)
        self.progress.set_spinning(busy)
        self.retry.set_label(dynamic('backend-retry', self.locale))
        self.retry.set_visible(not self.client.ready)
        self.retry.set_sensitive(self.client.error != 'backend-starting')
        for b in self.bindings:
            field, widget, row = b['field'], b['widget'], b['row']
            current = value(state, field)
            kind = field['kind']
            pending = self.pending.get((field['group'], field['method'], field.get('index')))
            reason = '' if kind == 'label' else unavailable_reason(state, field, self.locale)
            if pending:
                reason = self.tr('Applying setting…', '正在应用设置…')
                if kind != 'button':
                    current = pending['args'][-1]
            if field['key'] == 'errorDetail':
                current = connection_error(state, self.locale)
            row.set_visible(visible(state, field))
            if kind == 'toggle':
                widget.set_active(bool(current))
            elif kind == 'slider':
                if not self.client.ready or reason:
                    b['dirty'] = False
                if not b['dirty']:
                    widget.set_value(current if current is not None else field['min'])
                    b['number'].set_value(widget.get_value())
            elif kind == 'label':
                if field['group'] not in ('app', 'connection') and state.get('connection', {}).get('state') != 'connected':
                    current = None
                raw = field['key'] in ('title', 'artist', 'album', 'modelName', 'firmware', 'codec', 'errorDetail')
                row.set_subtitle(str(current) if raw and current is not None else display(current, self.locale))
            elif kind == 'choice':
                items = choices(state, field)
                if items and value(state, field) is None:
                    # AdwComboRow can force selection of the first model item.
                    # Keep an explicit non-command item until a real state arrives.
                    placeholder = self.tr('State not reported', '状态尚未确认') if field.get('knownKey') else '—'
                    items = [dict(value=None, label=placeholder), *items]
                names = [dynamic(i['label'], self.locale) if isinstance(i['label'], str)
                         else title(i['label'], self.locale) for i in items]
                b['choices'] = items
                if b.get('names') != names:
                    b['names'] = names
                    widget.set_model(Gtk.StringList.new(names))
                index = next((i for i, item in enumerate(items) if item['value'] == current), Gtk.INVALID_LIST_POSITION)
                if widget.get_selected() != index:
                    widget.set_selected(index)
                if not items and not reason:
                    reason = self.tr('Waiting for available choices from the headphones.', '正在等待耳机提供可用选项。')
            if kind != 'label':
                row.set_sensitive(not reason)
                unknown = field.get('knownKey') and current is None and not reason
                row.set_subtitle(self.tr('State not reported. Choose Off or On.', '状态尚未确认，请选择关或开。') if unknown else reason)
        for route in self.device_lists:
            self.refresh_devices(route, state)
        for name, method in [('play', 'togglePlayback'), ('next', 'nextTrack')]:
            field = next(b['field'] for b in self.bindings if b['field']['method'] == method)
            self.lookup_action(name).set_enabled(enabled(state, field))
        self.syncing = False

    def apply_theme(self, theme):
        if theme == self.theme: return
        self.theme = theme
        Adw.StyleManager.get_default().set_color_scheme({
            'system': Adw.ColorScheme.DEFAULT, 'light': Adw.ColorScheme.FORCE_LIGHT,
            'dark': Adw.ColorScheme.FORCE_DARK}[theme])

    def update_preference_info(self):
        self.preference_info.set_title(self.tr('Keyboard shortcuts', '键盘快捷键'))
        text = self.tr('Ctrl+Alt+P: Play/pause · Ctrl+Alt+N: Next · Ctrl+Alt+M: Quick controls · Ctrl+Q: Quit',
                       'Ctrl+Alt+P：播放/暂停 · Ctrl+Alt+N：下一曲 · Ctrl+Alt+M：快捷面板 · Ctrl+Q：退出')
        if not self.args.live:
            text += self.tr(' · Preview preferences apply to this session.', ' · 预览偏好仅本次生效。')
        self.preference_info.set_subtitle(text)

    def retranslate(self):
        self.update_preference_info()
        self.nav_button.set_tooltip_text(self.tr('Pages', '页面'))
        self.close_button.set_tooltip_text(self.tr('Close', '关闭'))
        self.quick.set_title(self.tr('Quick controls', '快捷控制'))
        quick_label = self.tr('Quick controls', '快捷控制')
        self.quick_label.set_label(quick_label)
        self.quick_button.set_tooltip_text(quick_label)
        self.quick_button.update_property([Gtk.AccessibleProperty.LABEL], [quick_label])
        self.settings_button.set_label(self.tr('All settings', '全部设置'))
        for spec, page, group in self.page_titles:
            label = title(spec['label'], self.locale)
            page.set_title(label); group.set_title(label)
            if spec['id'] != 'quick':
                self.stack.get_page(page).set_title(label)
                self.nav_rows[spec['id']].set_title(label)
        for b in self.bindings:
            label = title(b['field']['label'], self.locale)
            b['row'].set_title(label)
            b['widget'].set_tooltip_text(label)

    def smoke(self):
        try:
            assert self.client.state.get('app', {}).get('previewMode'), self.client.error
            assert self.client.sent == 0, 'Startup caused device writes'
            assert set(PAGE_ICONS) == set(self.nav_rows)
            assert all(Gtk.IconTheme.get_for_display(self.window.get_display()).has_icon(icon)
                       for icon in PAGE_ICONS.values())
            assert self.get_application_id() == APP_ID and Gtk.Window.get_default_icon_name() == ICON
            for page in CATALOG['pages']:
                self.stack.set_visible_child_name(page['id'])
            # Exercise actual widget signal paths, including repeated state refresh.
            toggle = next(b for b in self.bindings if b['field']['key'] == 'dseeEnabled')
            before = self.client.sent
            toggle['widget'].set_active(not toggle['widget'].get_active())
            assert self.client.sent == before + 1
            self.refresh(); assert self.client.sent == before + 1
            slider = next(b for b in self.bindings if b['field']['key'] == 'volume')
            slider['widget'].set_value(12); slider['commit'](); slider['commit']()
            assert self.client.sent == before + 2
            self.dispatch(lambda: None)
            language = next(b for b in self.bindings if b['field']['key'] == 'locale')
            self.expected_locale = 'en' if self.locale == 'zh_CN' else 'zh_CN'
            index = next(i for i, item in enumerate(language['choices']) if item['value'] == self.expected_locale)
            language['widget'].set_selected(index)
            appearance = next(b for b in self.bindings if b['field']['key'] == 'themeMode')
            appearance['widget'].set_selected(2)
            assert self.client.sent == 4, 'Preferences must write exactly once'
            GLib.timeout_add(600, self.smoke_preferences)
            return GLib.SOURCE_REMOVE
        except Exception as error:
            self.exit_code = 1
            print('GNOME smoke failed:', error, file=sys.stderr)
        self.quit()
        return GLib.SOURCE_REMOVE


    def smoke_preferences(self):
        try:
            assert self.locale == self.expected_locale
            assert self.theme == 'dark'
            assert self.client.state['preferences']['persistent'] is False
            assert self.client.sent == 4, 'Retranslation emitted device writes'
            assert dynamic('Device function 99', 'zh_CN') == '设备功能 99'
            assert dynamic('Connection failed: test', 'zh_CN') == '连接失败：test'
            self.lookup_action('play').activate(None)
            assert self.client.sent == 5
            self.lookup_action('panel').activate(None)
            panel = self.quick
            self.lookup_action('panel').activate(None)
            self.settings_button.emit('clicked')
            assert self.quick is panel and panel.get_visible() and self.window.get_visible()
            panel.close()
            assert panel.get_visible()
            self.close_button.emit('clicked')
            assert not panel.get_visible() and self.window.get_visible()
            assert dynamic('rock', self.locale) == ('摇滚' if self.locale == 'zh_CN' else 'Rock')
            dialog = self.confirm_command(dict(target='playback', method='powerOff', args=[]), True)
            dialog.response('cancel')
            assert Adw.StyleManager.get_default().get_color_scheme() == Adw.ColorScheme.FORCE_DARK
            self.refresh(); assert self.client.sent == 5
            print('GNOME native controls: PASS (all pages, preference roundtrip, dynamic language, theme, shortcuts, window fallback, single writes)', flush=True)
        except Exception as error:
            self.exit_code = 1
            print('Native preference smoke failed:', repr(error), file=sys.stderr)
        if self.exit_code:
            self.quit()
        else:
            self.client.restart()
            assert not self.client.ready and self.retry.get_visible()
            assert not enabled(self.client.state, dict(kind='button', group='app'))
            GLib.timeout_add_seconds(2, self.smoke_restarted)
        return GLib.SOURCE_REMOVE

    def smoke_restarted(self):
        try:
            assert self.client.ready, self.client.error
            assert self.client.sent == 5, 'Recovery replayed a command'
            assert not self.retry.get_visible()
            print('PASS: native retry, disabled offline commands, fresh snapshot and no replay', flush=True)
        except Exception as error:
            self.exit_code = 1
            print('Native recovery smoke failed:', repr(error), file=sys.stderr)
        self.quit()
        return GLib.SOURCE_REMOVE

if __name__ == '__main__':
    GLib.set_application_name('Sound Connect')
    app = App(options())
    result = app.run([sys.argv[0]] + (['--panel'] if app.args.panel else []))
    sys.exit(app.exit_code or result)
