"""Scan a private BlueZ ObjectManager from the actual live Linux backend."""
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading

try:
    from gi.repository import Gio, GLib
except ImportError:
    print('PyGObject required for the private BlueZ fixture')
    sys.exit(77)


def bluez_fixture():
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    interface = Gio.DBusNodeInfo.new_for_xml("""
        <node><interface name="org.freedesktop.DBus.ObjectManager">
        <method name="GetManagedObjects"><arg direction="out" type="a{oa{sa{sv}}}"/></method>
        </interface></node>""").interfaces[0]
    hci = '/org/bluez/hci0'
    inactive = '/org/bluez/hci1'
    def adapter(powered):
        return {'org.bluez.Adapter1': {'Powered': GLib.Variant('b', powered)}}
    def device(controller, address, paired, connected):
        return {'org.bluez.Device1': {'Adapter': GLib.Variant('o', controller),
                                      'Name': GLib.Variant('s', '耳' * 60),
                                      'Paired': GLib.Variant('b', paired),
                                      'Connected': GLib.Variant('b', connected),
                                      'Address': GLib.Variant('s', address)}}
    populated = {hci + '/dev_00': device(hci, '12:34:56:78:9A:BC', True, False),
                 inactive + '/dev_01': device(inactive, '12:34:56:78:9A:BD', True, True),
                 hci + '/dev_02': device(hci, '12:34:56:78:9A:BE', False, False),
                 hci + '/dev_03': device(hci, '12:34:56:78:9A:BF', False, True),
                 hci: adapter(True), inactive: adapter(False)}
    replies = iter([{}, {hci: adapter(False)}, {hci: adapter(True)}, populated, None, populated])
    def query(_bus, _sender, _path, _interface, _method, _parameters, invocation):
        reply = next(replies)
        if reply is None:
            invocation.return_dbus_error('org.freedesktop.DBus.Error.AccessDenied', 'Test scan denied')
        else:
            invocation.return_value(GLib.Variant('(a{oa{sa{sv}}})', (reply,)))
    bus.register_object('/', interface, query, None, None)
    Gio.bus_own_name_on_connection(bus, 'org.bluez', Gio.BusNameOwnerFlags.NONE,
                                   lambda *_: print('ready', flush=True), None)
    GLib.MainLoop().run()


def exercise(executable, directory):
    env = dict(os.environ, DBUS_SYSTEM_BUS_ADDRESS=os.environ['DBUS_SESSION_BUS_ADDRESS'],
               XDG_CONFIG_HOME=directory, XDG_DATA_HOME=directory)
    process = subprocess.Popen([executable, '--live'], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, env=env, text=True, encoding='utf-8')
    frames = queue.Queue()
    threading.Thread(target=lambda: [frames.put(json.loads(line)) for line in process.stdout], daemon=True).start()
    assert frames.get(timeout=5)['type'] == 'state'
    def scan():
        process.stdin.write('{"target":"app","method":"refreshDevices","args":[]}\n')
        process.stdin.flush()
        while True:
            event = frames.get(timeout=8)
            if event['type'] == 'result':
                assert event['ok'], event
                break
        while True:
            connection = frames.get(timeout=8)['connection']
            if connection['state'] == 'disconnected':
                return connection
    fixture = None
    try:
        assert scan()['errorCode'] == 'bluetooth-backend-unavailable'
        fixture = subprocess.Popen([sys.executable, __file__, '--fixture'], stdout=subprocess.PIPE)
        assert fixture.stdout.readline() == b'ready\n'
        for _ in range(2):
            assert scan()['errorCode'] == 'bluetooth-adapter-unavailable'
        empty = scan()
        assert not empty['errorCode'] and empty['devices'] == [], empty
        expected = [{'name': '耳' * 42, 'mac': '12:34:56:78:9A:' + suffix} for suffix in ('BC', 'BF')]
        assert scan()['devices'] == expected
        rejected = scan()
        assert rejected['errorCode'] == 'bluetooth-scan-failed' and 'Test scan denied' in rejected['errorDetail']
        recovered = scan()
        assert not recovered['errorCode'] and recovered['devices'] == expected
        process.stdin.close()
        assert process.wait(timeout=5) == 0
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        if fixture:
            fixture.terminate()
            fixture.wait(timeout=5)


if __name__ == '__main__':
    if sys.argv[1] == '--fixture':
        bluez_fixture()
    else:
        with tempfile.TemporaryDirectory() as directory:
            exercise(str(Path(sys.argv[1]).resolve()), directory)
        print('PASS: missing BlueZ, no/off/empty/active adapters, bounded names, scan error and recovery')
