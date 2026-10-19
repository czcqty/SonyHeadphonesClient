"""Build distro-specific DEBs without modifying the build host's installation."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def run(*args, **kwargs):
    return subprocess.check_output([str(arg) for arg in args], text=True, **kwargs).strip()


def copy(source, destination, executable=False):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    destination.chmod(0o755 if executable else 0o644)


def dependencies(binary, staging):
    control = staging / 'debian/control'
    control.parent.mkdir()
    control.write_text('Source: soundconnect\nSection: sound\nPriority: optional\nMaintainer: Sound Connect contributors <noreply@github.com>\n\nPackage: soundconnect\nArchitecture: any\nDescription: Sound Connect\n')
    result = run('dpkg-shlibdeps', '--ignore-missing-info', '-O', '-e' + str(binary),
                 '-l' + str(binary.parent / 'lib'), cwd=staging)
    shutil.rmtree(control.parent)
    return result.removeprefix('shlibs:Depends=')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--desktop', choices=['kde', 'gnome', 'xfce'], required=True)
    parser.add_argument('--distro', choices=['ubuntu24.04', 'debian13'], required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--qt-root', type=Path)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if run('dpkg', '--print-architecture') != 'amd64':
        raise SystemExit('This package recipe targets amd64')
    version = re.search(r'\bVERSION\s+(\d+\.\d+\.\d+)', (ROOT / 'CMakeLists.txt').read_text())[1]
    version += '-1~' + args.distro
    revision = os.environ.get('SOUNDCONNECT_SOURCE_REVISION') or run('git', '-C', ROOT, 'rev-parse', 'HEAD')
    with tempfile.TemporaryDirectory(prefix='soundconnect-deb-') as temporary:
        temp = Path(temporary)
        packages = {name: temp / name for name in ('soundconnect-common', 'soundconnect-backend', 'soundconnect-' + args.desktop)}
        for directory in packages.values():
            directory.mkdir()
        common = packages['soundconnect-common']
        for name in ('catalog.json', 'client.py', 'process_client.py'):
            copy(ROOT / 'clients/common' / name, common / 'usr/share/soundconnect/clients/common' / name)
        copy(ROOT / 'gui/assets/icons/app-icon.svg', common / 'usr/share/icons/hicolor/scalable/apps/io.github.czcqty.SoundConnect.svg')
        backend = packages['soundconnect-backend']
        executable = backend / 'usr/libexec/soundconnect/SoundConnectBackend'
        copy(build / 'backend/backend/SoundConnectBackend', executable, True)
        if args.distro == 'ubuntu24.04':
            if not args.qt_root:
                raise SystemExit('Ubuntu packaging requires --qt-root for private Qt Core')
            qt_root = args.qt_root.resolve()
            qt_version = run(qt_root / 'bin/qtpaths', '--qt-version')
            if qt_version != '6.8.3':
                raise SystemExit('Expected the pinned Qt 6.8.3 SDK, got ' + qt_version)
            # Bundle only dependencies originating in the private SDK. System
            # libraries stay package dependencies; no GTK/KDE search path changes.
            library = executable.parent / 'lib'
            for line in run('ldd', executable).splitlines():
                match = re.match(r'\s*(\S+) => (.+) \([^)]*\)$', line)
                if match and Path(match[2]).is_relative_to(qt_root):
                    source = Path(match[2])
                    if source.name.startswith('libQt6') and not source.name.startswith('libQt6Core.'):
                        raise SystemExit('Unexpected backend Qt module: ' + source.name)
                    copy(source, library / match[1], True)
                    run('patchelf', '--set-rpath', '$ORIGIN', library / match[1])
            if not (library / 'libQt6Core.so.6').exists():
                raise SystemExit('Qt Core runtime was not discovered')
            run('patchelf', '--set-rpath', '$ORIGIN/lib', executable)
            license_dir = qt_root / 'LICENSES'
            if license_dir.is_dir():
                shutil.copytree(license_dir, backend / 'usr/share/doc/soundconnect-backend/qt-licenses')
        else:
            run('patchelf', '--remove-rpath', executable)
        backend_deps = dependencies(executable, backend)
        frontend = packages['soundconnect-' + args.desktop]
        command = 'soundconnect-' + args.desktop
        if args.desktop == 'kde':
            target = frontend / 'usr/bin' / command
            copy(build / 'kde/SoundConnectKDE', target, True)
            native_deps = dependencies(target, frontend)
        else:
            copy(ROOT / 'gui' / args.desktop / 'main.py', frontend / 'usr/share/soundconnect/gui' / args.desktop / 'main.py')
            target = frontend / 'usr/bin' / command
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('#!/bin/sh\nexec /usr/bin/python3 /usr/share/soundconnect/gui/' + args.desktop + '/main.py "$@"\n')
            target.chmod(0o755)
            native_deps = ('python3, python3-gi, gir1.2-gtk-4.0 (>= 4.10), gir1.2-adw-1 (>= 1.4)'
                           if args.desktop == 'gnome' else 'python3, python3-gi, gir1.2-gtk-3.0 (>= 3.24), gir1.2-libxfce4ui-2.0 (>= 4.18)')
        desktop_file = frontend / 'usr/share/applications' / ('io.github.czcqty.SoundConnect.' + args.desktop + '.desktop')
        desktop_file.parent.mkdir(parents=True)
        desktop_file.write_text('[Desktop Entry]\nType=Application\nName=Sound Connect (' + args.desktop.upper() + ')\n'
            'Comment=Control supported Sony headphones\nExec=' + command + ' --live\n'
            'Icon=io.github.czcqty.SoundConnect\nTerminal=false\nCategories=AudioVideo;Audio;Settings;\n')
        run('desktop-file-validate', desktop_file)
        manifest = dict(version=version, source=revision, distro=args.distro, desktop=args.desktop, packages=[])
        for name, directory in packages.items():
            deps = 'python3' if name == 'soundconnect-common' else backend_deps if name == 'soundconnect-backend' else native_deps + ', soundconnect-backend (= ' + version + '), soundconnect-common (= ' + version + ')'
            metadata = directory / 'DEBIAN'
            metadata.mkdir()
            (metadata / 'control').write_text(f'Package: {name}\nVersion: {version}\nArchitecture: {"all" if name == "soundconnect-common" else "amd64"}\nMaintainer: Sound Connect contributors <noreply@github.com>\nSection: sound\nPriority: optional\nDepends: {deps}\nDescription: Native Sound Connect {name.removeprefix("soundconnect-")}\n Shared local controls for supported Sony headphones.\n')
            copy(ROOT / 'LICENSE', directory / 'usr/share/doc' / name / 'copyright')
            archive = output / (name + '_' + version + '_' + ('all' if name == 'soundconnect-common' else 'amd64') + '.deb')
            run('dpkg-deb', '--root-owner-group', '--build', directory, archive)
            manifest['packages'].append(dict(file=archive.name, dependencies=deps))
        (output / ('manifest-' + args.desktop + '.json')).write_text(json.dumps(manifest, indent=2) + '\n')
        print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
