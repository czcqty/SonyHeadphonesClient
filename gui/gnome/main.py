#!/usr/bin/env python3
"""GNOME presentation using stock GTK 4/libadwaita widgets."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'native'))
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Adw, Gdk, Gio, GLib, Gtk
from client import CATALOG, Client, choices, command, enabled, options, title, value


class App(Adw.Application):
    def __init__(self, args):
        super().__init__(application_id='io.github.czcqty.SoundConnect.GnomePreview', flags=Gio.ApplicationFlags.NON_UNIQUE)
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.connect('activate', self.activate_ui)
        self.connect('shutdown', lambda *_: self.client and self.client.close())

    def tr(self, en, zh):
        return zh if self.args.locale == 'zh_CN' else en

    def activate_ui(self, *_):
        if self.client:
            self.window.present()
            return
        self.window = Adw.ApplicationWindow(application=self, title='Sound Connect', default_width=960, default_height=740)
        self.window.connect('close-request', lambda *_: self.quit())
        toolbar = Adw.ToolbarView()
        header = Adw.HeaderBar()
        quick = Gtk.Button(label=self.tr('Quick controls', '快捷控制'))
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
            self.stack.add_titled(self.make_page(page), page['id'], title(page['label'], self.args.locale))
        self.stack.set_visible_child_name(self.args.route)
        self.quick = Adw.ApplicationWindow(application=self, title=self.tr('Quick controls', '快捷控制'), default_width=460, default_height=550)
        quick_toolbar = Adw.ToolbarView(); quick_header = Adw.HeaderBar()
        settings = Gtk.Button(label=self.tr('All settings', '全部设置'))
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

    def close_quick(self, *_):
        if self.window.get_visible():
            self.quick.set_visible(False)
        else:
            self.quit()
        return True

    def send(self, field, new_value=None):
        if not self.syncing and self.client:
            self.client.send(command(field, new_value))

    def make_page(self, spec):
        page = Adw.PreferencesPage(title=title(spec['label'], self.args.locale))
        group = Adw.PreferencesGroup(title=title(spec['label'], self.args.locale))
        page.add(group)
        for field in spec['fields']:
            label = title(field['label'], self.args.locale)
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
        return page

    def refresh(self):
        if not self.client:
            return
        self.syncing = True
        state = self.client.state
        message = self.client.error or self.tr('Live device' if self.args.live else 'Isolated device preview', '实时设备' if self.args.live else '独立设备预览')
        self.status.set_label(message); self.quick_status.set_label(message)
        for b in self.bindings:
            field, widget, row = b['field'], b['widget'], b['row']
            current = value(state, field)
            row.set_sensitive(enabled(state, field))
            if field['kind'] == 'toggle':
                widget.set_active(bool(current))
            elif field['kind'] == 'slider':
                if not b['dirty']:
                    widget.set_value(current or 0)
            elif field['kind'] == 'label':
                row.set_subtitle(str(current) if current is not None else '—')
            elif field['kind'] in ('choice', 'devices'):
                items = (current or []) if field['kind'] == 'devices' else choices(state, field)
                names = [item['name'] if field['kind'] == 'devices' else title(item['label'], self.args.locale) for item in items]
                b['choices'] = items
                widget.set_model(Gtk.StringList.new(names))
                index = next((i for i, item in enumerate(items) if item.get('value') == current), Gtk.INVALID_LIST_POSITION)
                widget.set_selected(index)
                if field['kind'] == 'devices':
                    row.set_sensitive(bool(items))
        self.syncing = False

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
            print('GNOME native controls: PASS (all pages, startup isolation, toggle/slider write count)', flush=True)
        except Exception as error:
            self.exit_code = 1
            print('GNOME smoke failed:', error, file=sys.stderr)
        self.quit()
        return GLib.SOURCE_REMOVE


if __name__ == '__main__':
    app = App(options())
    result = app.run([sys.argv[0]])
    sys.exit(app.exit_code or result)
