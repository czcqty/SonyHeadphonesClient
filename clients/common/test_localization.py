"""Catalog coverage for backend-owned dynamic text; no GUI toolkit needed."""
from pathlib import Path
import re
from client import CATALOG, dynamic, display, connection_error

root = Path(__file__).resolve().parents[2]
for path in (root / 'backend').glob('*.cpp'):
    for source in re.findall(r'\btr\("([^"\n]+)"\)', path.read_text(encoding='utf-8')):
        assert source in CATALOG['messages'], (path, source)
viewmodels = (root / 'backend/ViewModels.cpp').read_text(encoding='utf-8')
presets = viewmodels.split('const QStringList ids{')[1].split('};')[0]
for key in re.findall(r'QStringLiteral\("([^"]+)"\)', presets):
    assert key in CATALOG['messages'], key
for page in [*CATALOG['pages'], CATALOG['quick']]:
    for labels in [page['label'], *[f['label'] for f in page['fields']]]:
        assert labels['en'] and labels['zh_CN']
for locale in ('en', 'zh_CN'):
    assert dynamic('Unrecognized device text', locale) == 'Unrecognized device text'
    assert connection_error({'connection': {'errorCode': 'connection-timeout'}}, locale)
assert dynamic('rock', 'zh_CN') == '摇滚'
assert dynamic('Device function AB', 'zh_CN') == '设备功能 AB'
assert dynamic('Connection failed: OS detail', 'zh_CN') == '连接失败：OS detail'
assert display(False, 'zh_CN') == '否'
assert connection_error({'connection': {'errorCode': 'connection-failed', 'errorDetail': 'OS detail'}}, 'zh_CN') == '连接失败: OS detail'
print('PASS: dynamic options, connection errors, booleans and bilingual labels')
