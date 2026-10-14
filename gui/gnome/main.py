#!/usr/bin/env python3
"""GNOME presentation using stock GTK 4/libadwaita widgets."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'clients' / 'common'))
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Adw, Gdk, Gio, GLib, Gtk
from client import CATALOG, Client, choices, command, enabled, options, title, value, dynamic, display, connection_error


class App(Adw.Application):
    def __init__(self, args):
        super().__init__(application_id='io.github.czcqty.SoundConnect.GnomePreview', flags=Gio.ApplicationFlags.NON_UNIQUE)
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.locale = args.locale or 'en'
        self.theme = None
        self.page_titles = []
        self.connect('activate', self.activate_ui)
        self.connect('shutdown', lambda *_: self.client and self.client.close())

    def tr(self, en, zh):
        return zh if self.locale == 'zh_CN' else en

    def activate_ui(self, *_):
        if self.client:
            self.window.present()
            return
        self.window = Adw.ApplicationWindow(application=self, title='Sound Connect', default_width=960, default_height=740)
        self.window.connect('close-request', self.close_main)
        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        self.quick_button = quick = Gtk.Button(label=self.tr('Quick controls', '快捷控制'))
        quick.connect('clicked', lambda *_: self.quick.present())
        header.pack_end(quick)
        toolbar.add_top_bar(header)
        self.status = Gtk.Label(wrap=True, margin_start=12, margin_end=12, margin_top=6, margin_bottom=6)
        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        content.append(self.status)
        pane = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL, wide_handle=True, vexpand=True)
        self.stack = Gtk.Stack(hexpand=True, vexpand=True)
        sidebar = Gtk.StackSidebar(stack=self.stack, width_request=170)
        pane.set_start_child(sidebar); pane.set_end_child(self.stack)
        pane.set_resize_start_child(False); pane.set_shrink_start_child(False)
        content.append(pane); toolbar.set_content(content); self.window.set_content(toolbar)
        for page in CATALOG['pages']:
            self.stack.add_titled(self.make_page(page), page['id'], title(page['label'], self.locale))
        self.stack.set_visible_child_name(self.args.route)
        self.quick = Adw.ApplicationWindow(application=self, title=self.tr('Quick controls', '快捷控制'), default_width=460, default_height=550)
        quick_toolbar = Adw.ToolbarView(); quick_header = Adw.HeaderBar()
        self.settings_button = settings = Gtk.Button(label=self.tr('All settings', '全部设置'))
        settings.connect('clicked', self.show_settings)
        quick_header.pack_start(settings); quick_toolbar.add_top_bar(quick_header)
        quick_content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.quick_status = Gtk.Label(wrap=True, margin_start=12, margin_end=12, margin_top=6, margin_bottom=6)
        quick_content.append(self.quick_status)
        quick_page = self.make_page(CATALOG['quick']); quick_page.set_vexpand(True)
        quick_content.append(quick_page)
        quick_toolbar.set_content(quick_content); self.quick.set_content(quick_toolbar)
        self.quick.connect('close-request', self.close_quick)
        keys = Gtk.EventControllerKey()
        keys.connect('key-pressed', lambda _, key, *__: self.close_quick() if key == Gdk.KEY_Escape else False)
        self.quick.add_controller(keys)
        for name, key, callback in [('play', 'p', lambda: self.client.send(dict(target='playback', method='togglePlayback', args=[]))),
                                    ('next', 'n', lambda: self.client.send(dict(target='playback', method='nextTrack', args=[]))),
                                    ('panel', 'm', self.toggle_quick)]:
            action = Gio.SimpleAction.new(name, None)
            action.connect('activate', lambda *_, cb=callback: cb())
            self.add_action(action)
            self.set_accels_for_action('app.' + name, ['<Control><Alt>' + key])
        self.client = Client(self.args, self.dispatch, self.refresh)
        self.refresh()
        (self.quick if self.args.panel else self.window).present()
        if self.args.smoke_test:
            GLib.timeout_add_seconds(3, self.smoke)

    @staticmethod
    def dispatch(callback):
        def run():
            callback()
            return GLib.SOURCE_REMOVE
        GLib.idle_add(run)

    def show_settings(self, *_):
        self.window.present(); self.quick.set_visible(False)

    def close_main(self, *_):
        self.quit()
        return True

    def toggle_quick(self):
        if self.quick.get_visible(): self.close_quick()
        else: self.quick.present()

    def close_quick(self, *_):
        if self.window.get_visible():
            self.quick.set_visible(False)
        else:
            self.show_settings()
        return True

    def send(self, field, new_value=None):
        if not self.syncing and self.client:
            if field['method'] in ('powerOff', 'unpairDevice'):
                self.confirm_command(command(field, new_value), field['method'] == 'powerOff')
            else:
                self.client.send(command(field, new_value))

    def confirm_command(self, request, power):
        dialog = Adw.MessageDialog.new(self.window,
            self.tr('Power off headphones?' if power else 'Unpair device?', '是否关闭耳机电源？' if power else '是否取消设备配对？'),
            self.tr('The Bluetooth connection will be closed.', '蓝牙连接将会断开。'))
        dialog.add_response('cancel', self.tr('Cancel', '取消'))
        dialog.add_response('confirm', self.tr('Power off' if power else 'Unpair', '关机' if power else '取消配对'))
        dialog.set_response_appearance('confirm', Adw.ResponseAppearance.DESTRUCTIVE)
        dialog.set_default_response('cancel'); dialog.set_close_response('cancel')
        dialog.connect('response', lambda _, response: self.client.send(request) if response == 'confirm' else None)
        dialog.present()
        return dialog

    def make_page(self, spec):
        page = Adw.PreferencesPage(title=title(spec['label'], self.locale))
        group = Adw.PreferencesGroup(title=title(spec['label'], self.locale))
        page.add(group)
        self.page_titles.append((spec, page, group))
        for field in spec['fields']:
            label = title(field['label'], self.locale)
            kind = field['kind']
            row = Adw.ActionRow(title=label)
            binding = dict(field=field, row=row, widget=None, choices=[], dirty=False)
            if kind == 'toggle':
                widget = Gtk.Switch(valign=Gtk.Align.CENTER)
                row.add_suffix(widget); row.set_activatable_widget(widget)
                widget.connect('notify::active', lambda w, _, f=field: self.send(f, w.get_active()))
            elif kind == 'slider':
                widget = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, field['min'], field['max'], 1)
                widget.set_digits(0); widget.set_draw_value(True); widget.set_hexpand(True); widget.set_size_request(180, -1)
                widget.set_tooltip_text(label); row.add_suffix(widget)
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
            elif kind in ('choice', 'devices'):
                row = Adw.ComboRow(title=label)
                widget = row; binding['row'] = row
                def selected(w, _, b=binding):
                    index = w.get_selected()
                    if self.syncing or index >= len(b['choices']):
                        return
                    item = b['choices'][index]
                    if b['field']['kind'] == 'devices':
                        self.client.send(dict(target='app', method='connectDevice', args=[item['name'], item['mac']]))
                    else:
                        self.send(b['field'], item['value'])
                row.connect('notify::selected', selected)
            elif kind == 'button':
                widget = Gtk.Button(label=label, valign=Gtk.Align.CENTER)
                widget.connect('clicked', lambda _, f=field: self.send(f))
                row.add_suffix(widget); row.set_activatable_widget(widget)
            else:
                widget = row
            binding['widget'] = widget; self.bindings.append(binding); group.add(row)
        if spec['id'] == 'application':
            info = Adw.ActionRow()
            group.add(info)
            self.preference_info = info
            self.update_preference_info()
        return page

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
        self.apply_theme(prefs.get('themeMode', 'system'))
        message = (dynamic('Backend error: %1', self.locale).replace('%1', dynamic(self.client.error, self.locale)) if self.client.error else '') or connection_error(state, self.locale) or self.tr('Live device' if self.args.live else 'Isolated device preview', '实时设备' if self.args.live else '独立设备预览')
        self.status.set_label(message); self.quick_status.set_label(message)
        for b in self.bindings:
            field, widget, row = b['field'], b['widget'], b['row']
            current = value(state, field)
            if field['key'] == 'errorDetail': current = connection_error(state, self.locale)
            row.set_sensitive(enabled(state, field))
            if field['kind'] == 'toggle':
                widget.set_active(bool(current))
            elif field['kind'] == 'slider':
                if not b['dirty']:
                    widget.set_value(current or 0)
            elif field['kind'] == 'label':
                row.set_subtitle((str(current) if field['key'] in ('title', 'artist', 'album', 'modelName', 'firmware', 'codec') and current is not None else display(current, self.locale)))
            elif field['kind'] in ('choice', 'devices'):
                items = (current or []) if field['kind'] == 'devices' else choices(state, field)
                names = [item['name'] if field['kind'] == 'devices' else (dynamic(item['label'], self.locale) if field.get('source') != 'devices' and isinstance(item['label'], str) else title(item['label'], self.locale)) for item in items]
                b['choices'] = items
                if b.get('names') != names:
                    b['names'] = names
                    widget.set_model(Gtk.StringList.new(names))
                index = next((i for i, item in enumerate(items) if item.get('value') == current), Gtk.INVALID_LIST_POSITION)
                widget.set_selected(index)
                if field['kind'] == 'devices':
                    row.set_sensitive(bool(items) and state.get('connection', {}).get('state') not in ('connected', 'connecting'))
        self.syncing = False

    def apply_theme(self, theme):
        if theme == self.theme: return
        self.theme = theme
        Adw.StyleManager.get_default().set_color_scheme({
            'system': Adw.ColorScheme.DEFAULT, 'light': Adw.ColorScheme.FORCE_LIGHT,
            'dark': Adw.ColorScheme.FORCE_DARK}[theme])

    def update_preference_info(self):
        self.preference_info.set_title(self.tr('Keyboard shortcuts', '键盘快捷键'))
        self.preference_info.set_subtitle(self.tr(
            'Ctrl+Alt+P: Play/pause · Ctrl+Alt+N: Next · Ctrl+Alt+M: Quick controls. Preview preferences are session only.',
            'Ctrl+Alt+P：播放/暂停 · Ctrl+Alt+N：下一曲 · Ctrl+Alt+M：快捷面板。预览偏好仅本次生效。'))

    def retranslate(self):
        self.update_preference_info()
        self.quick.set_title(self.tr('Quick controls', '快捷控制'))
        self.quick_button.set_label(self.tr('Quick controls', '快捷控制'))
        self.settings_button.set_label(self.tr('All settings', '全部设置'))
        for spec, page, group in self.page_titles:
            label = title(spec['label'], self.locale)
            page.set_title(label); group.set_title(label)
            if spec['id'] != 'quick': self.stack.get_page(page).set_title(label)
        for b in self.bindings:
            label = title(b['field']['label'], self.locale)
            b['row'].set_title(label)
            if b['field']['kind'] == 'button': b['widget'].set_label(label)
            b['widget'].set_tooltip_text(label)

    def smoke(self):
        try:
            assert self.client.state.get('app', {}).get('previewMode'), self.client.error
            assert self.client.sent == 0, 'Startup caused device writes'
            assert len(self.bindings) == sum(len(p['fields']) for p in CATALOG['pages']) + len(CATALOG['quick']['fields'])
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
            self.window.present(); self.quick.present(); self.close_quick()
            assert not self.quick.get_visible() and self.window.get_visible()
            assert dynamic('rock', self.locale) == ('摇滚' if self.locale == 'zh_CN' else 'Rock')
            dialog = self.confirm_command(dict(target='playback', method='powerOff', args=[]), True)
            dialog.response('cancel')
            assert Adw.StyleManager.get_default().get_color_scheme() == Adw.ColorScheme.FORCE_DARK
            self.refresh(); assert self.client.sent == 5
            print('GNOME native controls: PASS (all pages, preference roundtrip, dynamic language, theme, shortcuts, window fallback, single writes)', flush=True)
        except Exception as error:
            self.exit_code = 1
            print('Native preference smoke failed:', repr(error), file=sys.stderr)
        self.quit()
        return GLib.SOURCE_REMOVE

if __name__ == '__main__':
    app = App(options())
    result = app.run([sys.argv[0]])
    sys.exit(app.exit_code or result)
