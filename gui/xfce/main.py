#!/usr/bin/env python3
"""Xfce presentation using stock GTK 3 and libxfce4ui."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'clients' / 'common'))
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
gi.require_version('GdkX11', '3.0')
gi.require_version('Libxfce4ui', '2.0')
from gi.repository import Gdk, GdkPixbuf, GdkX11, GLib, Gtk, Libxfce4ui
from client import (CATALOG, Client, choices, command, connection_error, display, dynamic,
                    enabled, options, quick_page, title, unavailable_reason, value, visible)


APP_ICON = 'io.github.czcqty.SoundConnect'
PAGE_ICONS = {
    'home': 'go-home-symbolic', 'connection': 'bluetooth-symbolic',
    'sound': 'audio-volume-high-symbolic', 'noise': 'audio-headphones-symbolic',
    'equalizer': 'applications-multimedia-symbolic', 'behavior': 'system-run-symbolic',
    'touch': 'input-touchpad-symbolic', 'voice': 'audio-input-microphone-symbolic',
    'devices': 'computer-symbolic', 'application': 'preferences-system-symbolic',
}


def load_identity():
    # Installed packages supply the scalable hicolor icon. Register the identical
    # source artwork for developer launches outside the installed icon theme.
    if not Gtk.IconTheme.get_default().has_icon(APP_ICON):
        path = Path(__file__).resolve().parents[1] / 'assets' / 'icons' / 'app-icon.svg'
        if path.is_file():
            for size in (16, 24, 32, 48, 64, 128):
                image = GdkPixbuf.Pixbuf.new_from_file_at_scale(str(path), size, size, True)
                Gtk.IconTheme.add_builtin_icon(APP_ICON, size, image)
    Gtk.Window.set_default_icon_name(APP_ICON)


def panel_geometry(work, anchor, width, height):
    width, height = min(width, work.width), min(height, work.height)
    right = 2 * anchor.x + anchor.width >= 2 * work.x + work.width
    bottom = 2 * anchor.y + anchor.height >= 2 * work.y + work.height
    return (work.x + (work.width - width if right else 0),
            work.y + (work.height - height if bottom else 0), width, height)


class App:
    def __init__(self, args):
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.locale = args.locale or 'en'
        self.theme = None
        self.page_titles = []
        self.device_lists = {}
        self.device_buttons = {}
        self.pending = set()
        self.confirmation = None
        self.quitting = False
        load_identity()
        self.window = Libxfce4ui.TitledDialog.new()
        # This is the primary settings window. xfwm otherwise keeps dialog
        # windows above the independent quick panel even after it gains focus.
        self.window.set_type_hint(Gdk.WindowTypeHint.NORMAL)
        self.window.set_title('Sound Connect')
        self.window.set_icon_name(APP_ICON)
        self.window.set_subtitle(self.tr('Headphone settings', '耳机设置'))
        self.window.set_default_size(860, 650)
        self.window.connect('delete-event', self.close_main)
        # Required by libxfce4ui 4.18; a compatibility no-op on 4.20+.
        self.window.create_action_area()
        self.quick_button = self.window.add_button(self.tr('Quick controls', '快捷控制'), 1)
        self.close_button = self.window.add_button(self.tr('Close', '关闭'), Gtk.ResponseType.CLOSE)
        self.window.connect('response', lambda _, response: self.show_quick() if response == 1 else self.close_main())
        area = self.window.get_content_area(); area.set_spacing(10); area.set_border_width(12)
        heading = Gtk.Box(spacing=8)
        heading.pack_start(Gtk.Image.new_from_icon_name(APP_ICON, Gtk.IconSize.LARGE_TOOLBAR), False, False, 0)
        self.status = Gtk.Label(xalign=0, wrap=True)
        heading.pack_start(self.status, True, True, 0)
        area.pack_start(heading, False, False, 0)
        self.retry = Gtk.Button(label=dynamic('backend-retry', self.locale))
        self.retry.connect('clicked', lambda *_: self.client.restart())
        area.pack_start(self.retry, False, False, 0)
        self.notebook = Gtk.Notebook(scrollable=True)
        self.notebook.set_tab_pos(Gtk.PositionType.LEFT)
        self.window.connect('size-allocate', self.resize_navigation)
        area.pack_start(self.notebook, True, True, 0)
        for page in CATALOG['pages']:
            label = Gtk.Label(label=title(page['label'], self.locale))
            tab = Gtk.Box(spacing=6)
            icon = Gtk.Image.new_from_icon_name(PAGE_ICONS[page['id']], Gtk.IconSize.MENU)
            tab.pack_start(icon, False, False, 0)
            tab.pack_start(label, False, False, 0)
            tab.set_tooltip_text(label.get_text())
            tab.get_accessible().set_name(label.get_text())
            self.page_titles.append((page, label, tab))
            self.notebook.append_page(self.make_page(page), tab)
            tab.show_all()
        self.quick = Gtk.Window(title=self.tr('Quick controls', '快捷控制'))
        self.quick.set_icon_name(APP_ICON)
        self.quick.set_default_size(440, 480)
        self.quick.set_deletable(False)
        self.quick.connect('delete-event', lambda *_: True)
        quick_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin=12)
        self.quick.add(quick_box)
        header = Gtk.Box(spacing=8)
        header.pack_start(Gtk.Image.new_from_icon_name(APP_ICON, Gtk.IconSize.LARGE_TOOLBAR), False, False, 0)
        self.quick_title = Gtk.Label(label=self.quick.get_title(), xalign=0)
        header.pack_start(self.quick_title, True, True, 0)
        self.quick_close = Gtk.Button.new_from_icon_name('window-close-symbolic', Gtk.IconSize.BUTTON)
        self.quick_close.connect('clicked', self.close_quick)
        self.quick_close.set_tooltip_text(self.tr('Close', '关闭'))
        self.quick_close.get_accessible().set_name(self.tr('Close', '关闭'))
        header.pack_end(self.quick_close, False, False, 0)
        quick_box.pack_start(header, False, False, 0)
        self.quick_status = Gtk.Label(xalign=0, wrap=True); quick_box.pack_start(self.quick_status, False, False, 0)
        quick_box.pack_start(self.make_page(quick_page()), True, True, 0)
        self.settings_button = settings = Gtk.Button(label=self.tr('All settings…', '全部设置…'))
        settings.connect('clicked', self.show_settings); quick_box.pack_start(settings, False, False, 0)
        # GTK's native XEmbed status icon is supported by Xfce's X11 notification
        # area. If no host embeds it, closing settings terminates normally.
        self.tray_was_embedded = False
        self.tray = Gtk.StatusIcon.new_from_icon_name(APP_ICON) if isinstance(Gdk.Display.get_default(), GdkX11.X11Display) else None
        if self.tray:
            self.tray.set_tooltip_text('Sound Connect'); self.tray.set_visible(True)
            self.tray.connect('activate', self.show_quick)
        self.menu = Gtk.Menu()
        self.menu_labels = []
        for en, zh, callback in [('All settings', '全部设置', self.show_settings),
                                 ('Quick controls', '快捷控制', self.show_quick),
                                 ('Play / Pause', '播放 / 暂停', lambda *_: self.shortcut('p')),
                                 ('Next track', '下一曲', lambda *_: self.shortcut('n')),
                                 ('Disconnect', '断开连接', lambda *_: self.menu_action('app', 'disconnectDevice')),
                                 ('Quit', '退出', self.quit)]:
            item = Gtk.MenuItem(label=self.tr(en, zh)); item.connect('activate', callback); self.menu.append(item)
            self.menu_labels.append((item, en, zh))
        self.menu.show_all()
        if self.tray:
            self.tray.connect('popup-menu', lambda _, button, time: self.menu.popup(None, None, Gtk.StatusIcon.position_menu, self.tray, button, time))
        if self.tray: self.tray.connect('notify::embedded', self.tray_changed)
        for window in (self.window, self.quick):
            window.connect('key-press-event', self.key_pressed)
        self.client = Client(args, self.dispatch, self.refresh)
        self.window.show_all(); self.notebook.set_current_page(next(i for i, page in enumerate(CATALOG['pages']) if page['id'] == args.route))
        if args.panel:
            self.window.hide(); self.show_quick()
        self.refresh()
        if args.parity_test:
            from parity import run_parity
            GLib.idle_add(run_parity, self)
        elif args.smoke_test:
            GLib.timeout_add_seconds(3, self.smoke)

    def resize_navigation(self, _, allocation):
        for _, label, _ in self.page_titles:
            label.set_visible(allocation.width >= 700)

    def tr(self, en, zh):
        return zh if self.locale == 'zh_CN' else en

    @staticmethod
    def dispatch(callback):
        def run():
            callback()
            return GLib.SOURCE_REMOVE
        GLib.idle_add(run)

    def show_settings(self, *_):
        self.window.show(); self.window.present(); self.refresh()

    def show_quick(self, *_):
        self.quick.set_decorated(not self.has_tray())
        self.quick.show_all()
        self.refresh()
        self.position_quick()
        self.quick.present()

    def has_tray(self):
        return self.tray is not None and self.tray.is_embedded()

    def close_main(self, *_):
        if self.has_tray() or self.quick.get_visible():
            self.window.hide()
        else:
            self.quit()
        return True

    def close_quick(self, *_):
        if self.window.get_visible() or self.has_tray():
            self.quick.hide()
        else:
            self.quit()
        return True

    def quit(self, *_):
        self.quitting = True
        if self.tray: self.tray.set_visible(False)
        self.client.close()
        Gtk.main_quit()

    def send(self, field, new_value=None):
        if self.syncing or not self.client or not enabled(self.client.state, field):
            return
        self.submit(command(field, new_value), (field['group'], field['method'], field.get('index')))

    def submit(self, request, key):
        if key in self.pending or not self.client.ready:
            return
        if request['method'] in ('powerOff', 'unpairDevice'):
            self.confirm_command(request, key)
        else:
            self.write(request, key)

    def write(self, request, key):
        if key in self.pending or not self.client.ready:
            return
        self.pending.add(key)

        def complete(_):
            self.pending.discard(key)
            self.refresh()

        if not self.client.send(request, complete):
            self.pending.discard(key)
        self.refresh()

    def confirm_command(self, request, key):
        if self.confirmation:
            self.confirmation.present()
            return
        power = request['method'] == 'powerOff'
        dialog = Gtk.MessageDialog(transient_for=self.window, modal=True, destroy_with_parent=True,
            message_type=Gtk.MessageType.QUESTION, buttons=Gtk.ButtonsType.OK_CANCEL,
            text=self.tr('Power off headphones?' if power else 'Unpair device?', '是否关闭耳机电源？' if power else '是否取消设备配对？'))
        dialog.set_default_response(Gtk.ResponseType.CANCEL)
        self.confirmation = dialog

        def response(widget, result):
            self.confirmation = None
            widget.destroy()
            if result == Gtk.ResponseType.OK and self.client.state.get('connection', {}).get('state') == 'connected':
                self.write(request, key)

        dialog.connect('response', response)
        dialog.show()
        return dialog

    def make_page(self, spec):
        scroll = Gtk.ScrolledWindow(); scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        grid = Gtk.Grid(column_spacing=16, row_spacing=16, margin=18, valign=Gtk.Align.START)
        scroll.add(grid)
        for row, field in enumerate(spec['fields']):
            if spec['id'] in ('connection', 'devices') and field['key'] == 'devices':
                box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
                self.device_lists[spec['id']] = {'box': box, 'signature': None}
                grid.attach(box, 0, row * 2, 2, 1)
                continue
            if field.get('source') == 'devices':
                continue
            label = title(field['label'], self.locale)
            name = Gtk.Label(label=label, xalign=0, wrap=True)
            name.set_max_width_chars(24)
            kind = field['kind']; b = dict(field=field, page=spec['id'], choices=[], dirty=False, widget=None)
            if kind == 'toggle':
                widget = Gtk.CheckButton(label=label); name.set_text('')
                widget.connect('toggled', lambda w, f=field: self.send(f, w.get_active()))
            elif kind == 'slider':
                widget = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, field['min'], field['max'], 1)
                widget.set_digits(0); widget.set_draw_value(True); widget.set_size_request(160, -1)
                def changed(_, b=b):
                    if not self.syncing:
                        b['dirty'] = True
                def commit(*_, b=b):
                    if b['dirty'] and not self.syncing:
                        b['dirty'] = False
                        self.send(b['field'], round(b['widget'].get_value()))
                    return False
                widget.connect('value-changed', changed)
                widget.connect_after('button-release-event', commit)
                widget.connect_after('key-release-event', commit)
                widget.connect_after('focus-out-event', commit)
                widget.connect_after('scroll-event', lambda *_, cb=commit: self.queue_commit(cb))
                b['commit'] = commit
                if spec['id'] != 'quick':
                    number = Gtk.SpinButton.new_with_range(field['min'], field['max'], 1)
                    number.set_numeric(True)
                    number.set_width_chars(3)
                    number.get_accessible().set_name(label)
                    number.connect('value-changed', lambda w, f=field: self.send(f, w.get_value_as_int()))
                    b['number'] = number
            elif kind == 'choice':
                widget = Gtk.ComboBoxText()
                def selected(w, b=b):
                    index = w.get_active()
                    if self.syncing or index < 0 or index >= len(b['choices']):
                        return
                    item = b['choices'][index]
                    if item['value'] != value(self.client.state, b['field']):
                        self.send(b['field'], item['value'])
                widget.connect('changed', selected)
                widget.connect('notify::popup-shown', self.picker_changed)
            elif kind == 'button':
                widget = Gtk.Button(label=label); name.set_text('')
                widget.connect('clicked', lambda _, f=field: self.send(f))
            else:
                widget = Gtk.Label(xalign=0, wrap=True); widget.set_selectable(True); widget.set_max_width_chars(40)
            widget.set_hexpand(True); widget.set_tooltip_text(label)
            name.set_mnemonic_widget(widget)
            controls = Gtk.Box(spacing=6)
            controls.pack_start(widget, True, True, 0)
            if 'number' in b:
                controls.pack_end(b['number'], False, False, 0)
            grid.attach(name, 0, row * 2, 1, 1); grid.attach(controls, 1, row * 2, 1, 1)
            b['controls'] = controls
            b['reason'] = Gtk.Label(xalign=0, wrap=True)
            b['reason'].get_style_context().add_class('dim-label')
            grid.attach(b['reason'], 0, row * 2 + 1, 2, 1)
            b['widget'] = widget; b['name'] = name; self.bindings.append(b)
        if spec['id'] == 'application':
            self.preference_info = Gtk.Label(xalign=0, wrap=True)
            self.preference_info.set_max_width_chars(60)
            grid.attach(self.preference_info, 0, len(spec['fields']) * 2, 2, 1)
            self.update_preference_info()
        return scroll

    @staticmethod
    def queue_commit(callback):
        GLib.idle_add(callback)
        return False

    def picker_changed(self, widget, _):
        if not widget.get_property('popup-shown'):
            GLib.idle_add(self.refresh)

    def refresh(self):
        if not self.client:
            return
        self.syncing = True
        state = self.client.state
        connected = state.get('connection', {}).get('state') == 'connected'
        if not connected and self.confirmation:
            self.confirmation.response(Gtk.ResponseType.CANCEL)
        prefs = state.get('preferences', {})
        locale = prefs.get('locale', self.locale)
        if locale != self.locale:
            self.locale = locale
            self.retranslate()
        self.apply_theme(prefs.get('themeMode', self.theme or 'system'))
        message = (dynamic('Backend error: %1', self.locale).replace('%1', dynamic(self.client.error, self.locale)) if self.client.error else '') or connection_error(state, self.locale) or self.tr('Live device' if self.args.live else 'Isolated device preview', '实时设备' if self.args.live else '独立设备预览')
        if self.client.diagnostic and not self.client.ready:
            message += '\n' + self.client.diagnostic
        self.status.set_text(message); self.quick_status.set_text(message)
        self.retry.set_label(dynamic('backend-retry', self.locale))
        self.retry.set_visible(not self.client.ready)
        self.retry.set_sensitive(self.client.error != 'backend-starting')
        menu_fields = {'Play / Pause': ('playback','togglePlayback'), 'Next track': ('playback','nextTrack'),
                       'Disconnect': ('app','disconnectDevice')}
        for item, en, _ in self.menu_labels:
            if en in menu_fields:
                field = self.action_field(*menu_fields[en])
                item.set_sensitive(enabled(state, field) and (*menu_fields[en], None) not in self.pending)
        for b in self.bindings:
            field, widget = b['field'], b['widget']
            shown = visible(state, field)
            b['name'].set_visible(shown)
            b['controls'].set_visible(shown)
            current = value(state, field)
            if field['key'] == 'errorDetail':
                current = connection_error(state, self.locale)
            if field['kind'] == 'label' and field['group'] not in ('connection', 'app') and not connected:
                current = None
            pending = (field['group'], field['method'], field.get('index')) in self.pending
            reason = unavailable_reason(state, field, self.locale) if field['kind'] != 'label' else ''
            items = choices(state, field) if field['kind'] == 'choice' else []
            if field['kind'] == 'choice' and not reason and not items:
                reason = self.tr('Waiting for available choices from the headphones.', '正在等待耳机提供可用选项。')
            interactive = not pending and not reason
            b['controls'].set_sensitive(interactive)
            widget.set_sensitive(interactive)
            if pending:
                reason = self.tr('Applying setting…', '正在应用设置…')
            elif not reason and field.get('knownKey') and current is None:
                reason = self.tr('State not reported. Choose Off or On.', '状态尚未确认，请选择关或开。')
            b['reason'].set_text(reason)
            b['reason'].set_visible(shown and bool(reason))
            if pending:
                continue
            if not interactive:
                b['dirty'] = False
            if field['kind'] == 'toggle':
                widget.set_active(bool(current))
            elif field['kind'] == 'slider':
                if not b['dirty']:
                    widget.set_value(current or 0)
                    if 'number' in b and b['number'].get_value_as_int() != (current or 0):
                        b['number'].set_value(current or 0)
            elif field['kind'] == 'label':
                widget.set_text((str(current) if field['key'] in ('title', 'artist', 'album', 'modelName', 'firmware', 'codec') and current is not None else display(current, self.locale)))
            elif field['kind'] == 'choice' and not widget.get_property('popup-shown'):
                b['choices'] = items
                names = [dynamic(item['label'], self.locale) if isinstance(item['label'], str)
                         else title(item['label'], self.locale) for item in items]
                if b.get('names') != names:
                    b['names'] = names
                    widget.remove_all()
                    for name in names:
                        widget.append_text(name)
                widget.set_active(next((i for i, item in enumerate(items) if item['value'] == current), -1))
        for page, binding in self.device_lists.items():
            self.refresh_devices(page, binding)
        self.syncing = False

    def refresh_devices(self, page, binding):
        state = self.client.state
        connection = state.get('connection', {}).get('state')
        discovery = page == 'connection'
        group = 'app' if discovery else 'deviceManager'
        manager = state.get('deviceManager', {})
        items = state.get('connection', {}).get('devices', []) if discovery else manager.get('devices', [])
        if not discovery and connection != 'connected':
            items = []
        signature = (self.locale, self.client.ready, connection, manager.get('supported'),
                     manager.get('sourceSwitchSupported'), [item.copy() for item in items],
                     frozenset(key for key in self.pending if key[0] == group))
        if signature == binding['signature']:
            return
        binding['signature'] = signature
        box = binding['box']
        for child in box.get_children():
            child.destroy()
        rows = self.device_buttons[page] = {}
        for item in items:
            mac, name = item['mac'], item.get('name') or item['mac']
            box.pack_start(Gtk.Label(label=name, xalign=0, wrap=True), False, False, 0)
            detail = mac
            if not discovery:
                detail += ' · ' + dynamic('connected' if item['connected'] else 'disconnected', self.locale)
                if item.get('playback'):
                    detail += self.tr(' · Current playback device', ' · 当前播放设备')
            box.pack_start(Gtk.Label(label=detail, xalign=0, wrap=True), False, False, 0)
            def action(en, zh, method, args, allowed, mac=mac, name=name):
                key = (group, method, mac)
                button = Gtk.Button(label=self.tr(en, zh))
                button.get_accessible().set_name(self.tr(en, zh) + ' ' + name)
                button.set_sensitive(bool(allowed) and key not in self.pending)
                request = dict(target=group, method=method, args=args)
                button.connect('clicked', lambda *_: self.submit(request, key))
                box.pack_start(button, False, False, 0)
                rows[(mac, method)] = button
            if discovery:
                action('Connect', '连接', 'connectDevice', [item.get('name', ''), mac],
                       self.client.ready and connection not in ('connected', 'scanning', 'connecting', 'initializing'))
            else:
                if item['connected']:
                    action('Disconnect', '断开连接', 'disconnectDevice', [mac], manager.get('supported'))
                    if manager.get('sourceSwitchSupported') and not item.get('playback'):
                        action('Use for playback', '用于播放', 'switchDevice', [mac], True)
                else:
                    action('Connect', '连接', 'connectDevice', [mac], manager.get('supported'))
                action('Unpair', '取消配对', 'unpairDevice', [mac], manager.get('supported'))
        if not items:
            hint = (self.tr('Pair headphones in Bluetooth settings, then refresh.', '请先在蓝牙设置中配对耳机，然后刷新。')
                    if discovery else self.tr('No paired devices reported.', '耳机尚未报告已配对设备。'))
            box.pack_start(Gtk.Label(label=hint, xalign=0, wrap=True), False, False, 0)
        box.show_all()

    def action_field(self, group, method):
        return next(b['field'] for b in self.bindings if b['field']['group'] == group and b['field']['method'] == method)

    def menu_action(self, group, method):
        self.send(self.action_field(group, method))

    def shortcut(self, key):
        if key == 'm':
            self.show_quick()
        else:
            self.menu_action('playback', 'togglePlayback' if key == 'p' else 'nextTrack')

    def key_pressed(self, _, event):
        mask = Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.MOD1_MASK
        key = Gdk.keyval_name(Gdk.keyval_to_lower(event.keyval))
        if event.state & mask == mask and key in ('p', 'n', 'm'):
            self.shortcut(key); return True
        return False

    def tray_changed(self, *_):
        embedded = self.has_tray()
        if self.tray_was_embedded and not embedded:
            GLib.idle_add(self.restore_after_tray_loss)
        if self.quick.get_visible():
            self.quick.set_decorated(not embedded)
            self.position_quick()
        self.tray_was_embedded = embedded

    def restore_after_tray_loss(self):
        if not self.quitting and not self.has_tray() and not self.window.get_visible():
            self.show_settings()
        return GLib.SOURCE_REMOVE

    def position_quick(self):
        if not self.has_tray():
            return
        ok, screen, anchor, _ = self.tray.get_geometry()
        if not ok:
            return
        display = screen.get_display()
        monitor = display.get_monitor_at_point(anchor.x + anchor.width // 2, anchor.y + anchor.height // 2)
        work = monitor.get_workarea()
        x, y, width, height = panel_geometry(work, anchor, *self.quick.get_size())
        self.quick.resize(width, height)
        self.quick.move(x, y)

    def apply_theme(self, theme):
        if theme == self.theme: return
        self.theme = theme
        settings = Gtk.Settings.get_default()
        if theme == 'system': settings.reset_property('gtk-application-prefer-dark-theme')
        else: settings.set_property('gtk-application-prefer-dark-theme', theme == 'dark')

    def update_preference_info(self):
        self.preference_info.set_text(self.tr(
            'Ctrl+Alt+P: Play/pause · Ctrl+Alt+N: Next · Ctrl+Alt+M: Quick controls. Preview preferences are session only.',
            'Ctrl+Alt+P：播放/暂停 · Ctrl+Alt+N：下一曲 · Ctrl+Alt+M：快捷面板。预览偏好仅本次生效。'))

    def retranslate(self):
        self.update_preference_info()
        self.window.set_subtitle(self.tr('Headphone settings', '耳机设置'))
        self.quick.set_title(self.tr('Quick controls', '快捷控制'))
        self.quick_button.set_label(self.tr('Quick controls', '快捷控制'))
        self.close_button.set_label(self.tr('Close', '关闭'))
        self.settings_button.set_label(self.tr('All settings…', '全部设置…'))
        self.quick_title.set_text(self.quick.get_title())
        self.quick_close.set_tooltip_text(self.tr('Close', '关闭'))
        self.quick_close.get_accessible().set_name(self.tr('Close', '关闭'))
        for spec, label, tab in self.page_titles:
            label.set_text(title(spec['label'], self.locale))
            tab.set_tooltip_text(label.get_text())
            tab.get_accessible().set_name(label.get_text())
        for item, en, zh in self.menu_labels: item.set_label(self.tr(en, zh))
        for b in self.bindings:
            label = title(b['field']['label'], self.locale)
            if b['field']['kind'] in ('toggle', 'button'): b['widget'].set_label(label)
            else: b['name'].set_text(label)
            b['widget'].set_tooltip_text(label)
            if 'number' in b:
                b['number'].get_accessible().set_name(label)

    def smoke(self):
        try:
            assert self.client.state.get('app', {}).get('previewMode'), self.client.error
            assert self.client.sent == 0, 'Startup caused device writes'
            assert self.notebook.get_n_pages() == len(CATALOG['pages'])
            assert self.quick_button is not None and self.close_button is not None
            for index in range(self.notebook.get_n_pages()):
                self.notebook.set_current_page(index)
            toggle = next(b for b in self.bindings if b['field']['key'] == 'dseeEnabled')
            toggle['widget'].set_active(not toggle['widget'].get_active())
            assert self.client.sent == 1
            self.refresh(); assert self.client.sent == 1
            slider = next(b for b in self.bindings if b['field']['key'] == 'volume')
            slider['widget'].set_value(12); slider['commit'](); slider['commit']()
            assert self.client.sent == 2
            language = next(b for b in self.bindings if b['field']['key'] == 'locale')
            self.expected_locale = 'en' if self.locale == 'zh_CN' else 'zh_CN'
            index = next(i for i, item in enumerate(language['choices']) if item['value'] == self.expected_locale)
            language['widget'].set_active(index)
            appearance = next(b for b in self.bindings if b['field']['key'] == 'themeMode')
            appearance['widget'].set_active(2)
            assert self.client.sent == 4, 'Preferences must write exactly once'
            GLib.timeout_add(600, self.smoke_preferences)
            return GLib.SOURCE_REMOVE
        except Exception as error:
            self.exit_code = 1
            print('Xfce smoke failed:', error, file=sys.stderr)
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
            event = Gdk.Event.new(Gdk.EventType.KEY_PRESS)
            event.keyval = Gdk.KEY_p
            event.state = Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.MOD1_MASK
            assert self.key_pressed(self.window, event)
            assert self.client.sent == 5
            self.show_settings(); self.show_quick()
            quick = self.quick
            self.shortcut('m'); self.settings_button.clicked()
            assert self.quick is quick and quick.get_visible() and self.window.get_visible()
            self.quick_close.clicked()
            assert not self.quick.get_visible() and self.window.get_visible()
            assert dynamic('rock', self.locale) == ('摇滚' if self.locale == 'zh_CN' else 'Rock')
            dialog = self.confirm_command(dict(target='playback', method='powerOff', args=[]), ('playback', 'powerOff', None))
            dialog.response(Gtk.ResponseType.CANCEL)
            assert Gtk.Settings.get_default().get_property('gtk-application-prefer-dark-theme')
            self.refresh(); assert self.client.sent == 5
            print('XFCE native controls: PASS (all pages, preference roundtrip, dynamic language, theme, shortcuts, window fallback, single writes)', flush=True)
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
    args = options()
    GLib.set_prgname(APP_ICON + '.xfce')
    GLib.set_application_name('Sound Connect')
    Gdk.set_program_class(APP_ICON + '.xfce')
    if not Gtk.init_check()[0]:
        print('Cannot open a GTK display', file=sys.stderr)
        sys.exit(2)
    app = App(args)
    Gtk.main()
    sys.exit(app.exit_code)
