#!/usr/bin/env python3
"""Xfce presentation using stock GTK 3 and libxfce4ui."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'native'))
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Libxfce4ui', '2.0')
from gi.repository import Gdk, GLib, Gtk, Libxfce4ui
from client import CATALOG, Client, choices, command, enabled, options, title, value


class App:
    def __init__(self, args):
        self.args, self.bindings, self.syncing, self.client = args, [], False, None
        self.exit_code = 0
        self.window = Libxfce4ui.TitledDialog(title='Sound Connect')
        self.window.set_subtitle(self.tr('Headphone settings', '耳机设置'))
        self.window.set_default_size(860, 650)
        self.window.connect('delete-event', self.close_main)
        self.window.add_button(self.tr('Quick controls', '快捷控制'), 1)
        self.window.add_button(self.tr('Close', '关闭'), Gtk.ResponseType.CLOSE)
        self.window.connect('response', lambda _, response: self.show_quick() if response == 1 else self.close_main())
        area = self.window.get_content_area(); area.set_spacing(10); area.set_border_width(12)
        self.status = Gtk.Label(xalign=0, wrap=True); area.pack_start(self.status, False, False, 0)
        self.notebook = Gtk.Notebook(); self.notebook.set_tab_pos(Gtk.PositionType.LEFT)
        area.pack_start(self.notebook, True, True, 0)
        for page in CATALOG['pages']:
            self.notebook.append_page(self.make_page(page), Gtk.Label(label=title(page['label'], args.locale)))
        self.quick = Gtk.Window(title=self.tr('Quick controls', '快捷控制'))
        self.quick.set_type_hint(Gdk.WindowTypeHint.UTILITY); self.quick.set_default_size(440, 420)
        self.quick.connect('delete-event', self.close_quick)
        self.quick.connect('key-press-event', lambda _, event: self.close_quick() if event.keyval == Gdk.KEY_Escape else False)
        quick_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin=12)
        self.quick.add(quick_box)
        self.quick_status = Gtk.Label(xalign=0, wrap=True); quick_box.pack_start(self.quick_status, False, False, 0)
        quick_box.pack_start(self.make_page(CATALOG['quick']), True, True, 0)
        settings = Gtk.Button(label=self.tr('All settings…', '全部设置…'))
        settings.connect('clicked', self.show_settings); quick_box.pack_start(settings, False, False, 0)
        # GTK's native XEmbed status icon is supported by Xfce's X11 notification
        # area. If no host embeds it, closing settings terminates normally.
        self.tray = Gtk.StatusIcon.new_from_icon_name('audio-headphones')
        self.tray.set_tooltip_text('Sound Connect'); self.tray.set_visible(True)
        self.tray.connect('activate', lambda *_: self.close_quick() if self.quick.get_visible() else self.show_quick())
        self.menu = Gtk.Menu()
        for label, callback in [(self.tr('All settings', '全部设置'), self.show_settings),
                                (self.tr('Quick controls', '快捷控制'), self.show_quick),
                                (self.tr('Quit', '退出'), self.quit)]:
            item = Gtk.MenuItem(label=label); item.connect('activate', callback); self.menu.append(item)
        self.menu.show_all()
        self.tray.connect('popup-menu', lambda _, button, time: self.menu.popup(None, None, Gtk.StatusIcon.position_menu, self.tray, button, time))
        self.client = Client(args, self.dispatch, self.refresh)
        self.window.show_all(); self.notebook.set_current_page(next(i for i, page in enumerate(CATALOG['pages']) if page['id'] == args.route))
        if args.panel:
            self.window.hide(); self.show_quick()
        self.refresh()
        if args.smoke_test:
            GLib.timeout_add_seconds(3, self.smoke)

    def tr(self, en, zh):
        return zh if self.args.locale == 'zh_CN' else en

    @staticmethod
    def dispatch(callback):
        def run():
            callback()
            return GLib.SOURCE_REMOVE
        GLib.idle_add(run)

    def show_settings(self, *_):
        self.window.show_all(); self.window.present(); self.quick.hide()

    def show_quick(self, *_):
        self.quick.show_all(); self.quick.present()

    def close_main(self, *_):
        if self.tray.is_embedded():
            self.window.hide()
        else:
            self.quit()
        return True

    def close_quick(self, *_):
        if self.window.get_visible() or self.tray.is_embedded():
            self.quick.hide()
        else:
            self.quit()
        return True

    def quit(self, *_):
        self.tray.set_visible(False)
        self.client.close()
        Gtk.main_quit()

    def send(self, field, new_value=None):
        if not self.syncing and self.client:
            self.client.send(command(field, new_value))

    def make_page(self, spec):
        scroll = Gtk.ScrolledWindow(); scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        grid = Gtk.Grid(column_spacing=16, row_spacing=16, margin=18, valign=Gtk.Align.START)
        scroll.add(grid)
        for row, field in enumerate(spec['fields']):
            label = title(field['label'], self.args.locale)
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
            b['widget'] = widget; self.bindings.append(b)
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
        message = self.client.error or self.tr('Live device' if self.args.live else 'Isolated device preview', '实时设备' if self.args.live else '独立设备预览')
        self.status.set_text(message); self.quick_status.set_text(message)
        for b in self.bindings:
            field, widget = b['field'], b['widget']; current = value(state, field)
            widget.set_sensitive(enabled(state, field))
            if field['kind'] == 'toggle':
                widget.set_active(bool(current))
            elif field['kind'] == 'slider':
                if not b['dirty']:
                    widget.set_value(current or 0)
            elif field['kind'] == 'label':
                widget.set_text(str(current) if current is not None else '—')
            elif field['kind'] in ('choice', 'devices'):
                items = (current or []) if field['kind'] == 'devices' else choices(state, field)
                b['choices'] = items; widget.remove_all()
                for item in items:
                    widget.append_text(item['name'] if field['kind'] == 'devices' else title(item['label'], self.args.locale))
                widget.set_active(next((i for i, item in enumerate(items) if item.get('value') == current), -1))
                if field['kind'] == 'devices':
                    widget.set_sensitive(bool(items))
        self.syncing = False

    def smoke(self):
        try:
            assert self.client.state.get('app', {}).get('previewMode'), self.client.error
            assert self.client.sent == 0, 'Startup caused device writes'
            assert self.notebook.get_n_pages() == len(CATALOG['pages'])
            for index in range(self.notebook.get_n_pages()):
                self.notebook.set_current_page(index)
            toggle = next(b for b in self.bindings if b['field']['key'] == 'dseeEnabled')
            toggle['widget'].set_active(not toggle['widget'].get_active())
            assert self.client.sent == 1
            self.refresh(); assert self.client.sent == 1
            slider = next(b for b in self.bindings if b['field']['key'] == 'volume')
            slider['widget'].set_value(12); slider['commit'](); slider['commit']()
            assert self.client.sent == 2
            print('Xfce native controls: PASS (all pages, startup isolation, toggle/slider write count)', flush=True)
        except Exception as error:
            self.exit_code = 1
            print('Xfce smoke failed:', error, file=sys.stderr)
        self.quit()
        return GLib.SOURCE_REMOVE


if __name__ == '__main__':
    app = App(options())
    Gtk.main()
    sys.exit(app.exit_code)
