#!/usr/bin/env python3
"""Xfce presentation using stock GTK 3 and libxfce4ui."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'clients' / 'common'))
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
gi.require_version('Libxfce4ui', '2.0')
from gi.repository import Gdk, GdkX11, GLib, Gtk, Libxfce4ui
from client import CATALOG, Client, choices, command, enabled, options, title, value, dynamic, display, connection_error


class App:
    def __init__(self, args):
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.locale = args.locale or 'en'
        self.theme = None
        self.page_titles = []
        self.window = Libxfce4ui.TitledDialog.new()
        self.window.set_title('Sound Connect')
        self.window.set_subtitle(self.tr('Headphone settings', '耳机设置'))
        self.window.set_default_size(860, 650)
        self.window.connect('delete-event', self.close_main)
        # Required by libxfce4ui 4.18; a compatibility no-op on 4.20+.
        self.window.create_action_area()
        self.quick_button = self.window.add_button(self.tr('Quick controls', '快捷控制'), 1)
        self.close_button = self.window.add_button(self.tr('Close', '关闭'), Gtk.ResponseType.CLOSE)
        self.window.connect('response', lambda _, response: self.show_quick() if response == 1 else self.close_main())
        area = self.window.get_content_area(); area.set_spacing(10); area.set_border_width(12)
        self.status = Gtk.Label(xalign=0, wrap=True); area.pack_start(self.status, False, False, 0)
        self.retry = Gtk.Button(label=dynamic('backend-retry', self.locale))
        self.retry.connect('clicked', lambda *_: self.client.restart())
        area.pack_start(self.retry, False, False, 0)
        self.notebook = Gtk.Notebook(); self.notebook.set_tab_pos(Gtk.PositionType.LEFT)
        area.pack_start(self.notebook, True, True, 0)
        for page in CATALOG['pages']:
            label = Gtk.Label(label=title(page['label'], self.locale))
            self.page_titles.append((page, label))
            self.notebook.append_page(self.make_page(page), label)
        self.quick = Gtk.Window(title=self.tr('Quick controls', '快捷控制'))
        self.quick.set_type_hint(Gdk.WindowTypeHint.UTILITY); self.quick.set_default_size(440, 420)
        self.quick.connect('delete-event', self.close_quick)
        self.quick.connect('key-press-event', lambda _, event: self.close_quick() if event.keyval == Gdk.KEY_Escape else False)
        quick_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin=12)
        self.quick.add(quick_box)
        self.quick_status = Gtk.Label(xalign=0, wrap=True); quick_box.pack_start(self.quick_status, False, False, 0)
        quick_box.pack_start(self.make_page(CATALOG['quick']), True, True, 0)
        self.settings_button = settings = Gtk.Button(label=self.tr('All settings…', '全部设置…'))
        settings.connect('clicked', self.show_settings); quick_box.pack_start(settings, False, False, 0)
        # GTK's native XEmbed status icon is supported by Xfce's X11 notification
        # area. If no host embeds it, closing settings terminates normally.
        self.tray = Gtk.StatusIcon.new_from_icon_name('audio-headphones') if isinstance(Gdk.Display.get_default(), GdkX11.X11Display) else None
        if self.tray:
            self.tray.set_tooltip_text('Sound Connect'); self.tray.set_visible(True)
            self.tray.connect('activate', lambda *_: self.close_quick() if self.quick.get_visible() else self.show_quick())
        self.menu = Gtk.Menu()
        self.menu_labels = []
        for en, zh, callback in [('All settings', '全部设置', self.show_settings),
                                 ('Quick controls', '快捷控制', self.show_quick),
                                 ('Play / Pause', '播放 / 暂停', lambda *_: self.shortcut('p')),
                                 ('Next track', '下一曲', lambda *_: self.shortcut('n')),
                                 ('Disconnect', '断开连接', lambda *_: self.client.send(dict(target='app', method='disconnectDevice', args=[]))),
                                 ('Quit', '退出', self.quit)]:
            item = Gtk.MenuItem(label=self.tr(en, zh)); item.connect('activate', callback); self.menu.append(item)
            self.menu_labels.append((item, en, zh))
        self.menu.show_all()
        if self.tray:
            self.tray.connect('popup-menu', lambda _, button, time: self.menu.popup(None, None, Gtk.StatusIcon.position_menu, self.tray, button, time))
        self.focus_timer = 0
        self.quick.connect('focus-out-event', self.schedule_dismiss)
        if self.tray: self.tray.connect('notify::embedded', self.tray_changed)
        for window in (self.window, self.quick):
            window.connect('key-press-event', self.key_pressed)
        self.client = Client(args, self.dispatch, self.refresh)
        self.window.show_all(); self.notebook.set_current_page(next(i for i, page in enumerate(CATALOG['pages']) if page['id'] == args.route))
        if args.panel:
            self.window.hide(); self.show_quick()
        self.refresh()
        if args.smoke_test:
            GLib.timeout_add_seconds(3, self.smoke)

    def tr(self, en, zh):
        return zh if self.locale == 'zh_CN' else en

    @staticmethod
    def dispatch(callback):
        def run():
            callback()
            return GLib.SOURCE_REMOVE
        GLib.idle_add(run)

    def show_settings(self, *_):
        self.window.show_all(); self.window.present(); self.quick.hide(); self.refresh()

    def show_quick(self, *_):
        self.quick.show_all(); self.position_quick(); self.quick.present()

    def has_tray(self):
        return self.tray is not None and self.tray.is_embedded()

    def close_main(self, *_):
        if self.has_tray():
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
        if self.focus_timer: GLib.source_remove(self.focus_timer); self.focus_timer = 0
        if self.tray: self.tray.set_visible(False)
        self.client.close()
        Gtk.main_quit()

    def send(self, field, new_value=None):
        if not self.syncing and self.client:
            if field['method'] in ('powerOff', 'unpairDevice'):
                self.confirm_command(command(field, new_value), field['method'] == 'powerOff')
            else:
                self.client.send(command(field, new_value))

    def confirm_command(self, request, power):
        dialog = Gtk.MessageDialog(transient_for=self.window, modal=True, destroy_with_parent=True,
            message_type=Gtk.MessageType.QUESTION, buttons=Gtk.ButtonsType.OK_CANCEL,
            text=self.tr('Power off headphones?' if power else 'Unpair device?', '是否关闭耳机电源？' if power else '是否取消设备配对？'))
        dialog.set_default_response(Gtk.ResponseType.CANCEL)
        def response(widget, result):
            widget.destroy()
            if result == Gtk.ResponseType.OK: self.client.send(request)
        dialog.connect('response', response); dialog.show()
        return dialog

    def make_page(self, spec):
        scroll = Gtk.ScrolledWindow(); scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        grid = Gtk.Grid(column_spacing=16, row_spacing=16, margin=18, valign=Gtk.Align.START)
        scroll.add(grid)
        for row, field in enumerate(spec['fields']):
            label = title(field['label'], self.locale)
            name = Gtk.Label(label=label, xalign=0, wrap=True)
            name.set_max_width_chars(24)
            kind = field['kind']; b = dict(field=field, choices=[], dirty=False, widget=None)
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
            elif kind in ('choice', 'devices'):
                widget = Gtk.ComboBoxText()
                def selected(w, b=b):
                    index = w.get_active()
                    if self.syncing or index < 0 or index >= len(b['choices']):
                        return
                    item = b['choices'][index]
                    if b['field']['kind'] == 'devices':
                        self.client.send(dict(target='app', method='connectDevice', args=[item['name'], item['mac']]))
                    else:
                        self.send(b['field'], item['value'])
                widget.connect('changed', selected)
            elif kind == 'button':
                widget = Gtk.Button(label=label); name.set_text('')
                widget.connect('clicked', lambda _, f=field: self.send(f))
            else:
                widget = Gtk.Label(xalign=0, wrap=True); widget.set_selectable(True); widget.set_max_width_chars(40)
            widget.set_hexpand(True); widget.set_tooltip_text(label)
            name.set_mnemonic_widget(widget)
            grid.attach(name, 0, row, 1, 1); grid.attach(widget, 1, row, 1, 1)
            b['widget'] = widget; b['name'] = name; self.bindings.append(b)
        if spec['id'] == 'application':
            self.preference_info = Gtk.Label(xalign=0, wrap=True)
            self.preference_info.set_max_width_chars(60)
            grid.attach(self.preference_info, 0, len(spec['fields']), 2, 1)
            self.update_preference_info()
        return scroll

    @staticmethod
    def queue_commit(callback):
        GLib.idle_add(callback)
        return False

    def refresh(self):
        if not self.client:
            return
        self.syncing = True
        state = self.client.state
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
        for item, en, _ in self.menu_labels:
            if en in ('Play / Pause', 'Next track', 'Disconnect'):
                item.set_sensitive(self.client.ready)
        for b in self.bindings:
            field, widget = b['field'], b['widget']; current = value(state, field)
            if field['key'] == 'errorDetail': current = connection_error(state, self.locale)
            widget.set_sensitive(enabled(state, field))
            if field['kind'] == 'toggle':
                widget.set_active(bool(current))
            elif field['kind'] == 'slider':
                if not b['dirty']:
                    widget.set_value(current or 0)
            elif field['kind'] == 'label':
                widget.set_text((str(current) if field['key'] in ('title', 'artist', 'album', 'modelName', 'firmware', 'codec') and current is not None else display(current, self.locale)))
            elif field['kind'] in ('choice', 'devices'):
                items = (current or []) if field['kind'] == 'devices' else choices(state, field)
                b['choices'] = items
                names = [item['name'] if field['kind'] == 'devices' else (dynamic(item['label'], self.locale) if field.get('source') != 'devices' and isinstance(item['label'], str) else title(item['label'], self.locale)) for item in items]
                if b.get('names') != names:
                    b['names'] = names
                    widget.remove_all()
                    for name in names: widget.append_text(name)
                widget.set_active(next((i for i, item in enumerate(items) if item.get('value') == current), -1))
                if field['kind'] == 'devices':
                    widget.set_sensitive(bool(items) and state.get('connection', {}).get('state') not in ('connected', 'connecting'))
        self.syncing = False

    def shortcut(self, key):
        if key == 'm':
            if self.quick.get_visible(): self.close_quick()
            else: self.show_quick()
        elif self.client:
            self.client.send(dict(target='playback', method='togglePlayback' if key == 'p' else 'nextTrack', args=[]))

    def key_pressed(self, _, event):
        mask = Gdk.ModifierType.CONTROL_MASK | Gdk.ModifierType.MOD1_MASK
        key = Gdk.keyval_name(Gdk.keyval_to_lower(event.keyval))
        if event.state & mask == mask and key in ('p', 'n', 'm'):
            self.shortcut(key); return True
        return False

    def tray_changed(self, *_):
        if not self.has_tray() and not self.window.get_visible(): self.show_settings()

    def schedule_dismiss(self, *_):
        if not self.focus_timer: self.focus_timer = GLib.timeout_add(150, self.dismiss_if_unfocused)
        return False

    def dismiss_if_unfocused(self):
        # Only tray panels dismiss on focus loss. No tray => reachable normal window.
        if self.quick.is_active() or not self.quick.get_visible() or not self.has_tray():
            self.focus_timer = 0; return GLib.SOURCE_REMOVE
        popup = self.menu.get_mapped() or Gtk.grab_get_current() is not None or any(
            isinstance(b['widget'], Gtk.ComboBox) and b['widget'].get_property('popup-shown') for b in self.bindings)
        if popup: return GLib.SOURCE_CONTINUE
        self.quick.hide(); self.focus_timer = 0
        return GLib.SOURCE_REMOVE

    def position_quick(self):
        # Xfce's XEmbed tray supplies a real anchor on X11. Wayland leaves window
        # placement to the compositor and uses an ordinary window without a tray.
        if not self.has_tray(): return
        ok, screen, area, orientation = self.tray.get_geometry()
        if not ok: return
        display = self.quick.get_display()
        monitor = display.get_monitor_at_point(area.x, area.y)
        rect = monitor.get_workarea(); width, height = self.quick.get_size()
        self.quick.move(max(rect.x, min(area.x, rect.x + rect.width - width)),
                        max(rect.y, min(area.y - height, rect.y + rect.height - height)))

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
        for spec, label in self.page_titles: label.set_text(title(spec['label'], self.locale))
        for item, en, zh in self.menu_labels: item.set_label(self.tr(en, zh))
        for b in self.bindings:
            label = title(b['field']['label'], self.locale)
            if b['field']['kind'] in ('toggle', 'button'): b['widget'].set_label(label)
            else: b['name'].set_text(label)
            b['widget'].set_tooltip_text(label)

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
            if not self.has_tray():
                self.dismiss_if_unfocused()
                assert self.quick.get_visible(), 'No-tray window was stranded'
            self.close_quick()
            assert not self.quick.get_visible() and self.window.get_visible()
            assert dynamic('rock', self.locale) == ('摇滚' if self.locale == 'zh_CN' else 'Rock')
            dialog = self.confirm_command(dict(target='playback', method='powerOff', args=[]), True)
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
    app = App(options())
    Gtk.main()
    sys.exit(app.exit_code)
