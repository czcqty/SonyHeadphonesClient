"""Generate presentation-neutral bindings, never widget styling or geometry."""
import json
from pathlib import Path


def text(en, zh):
    return {'en': en, 'zh_CN': zh}


def field(kind, en, zh, group, key='', method='', capability='', **kw):
    return dict(kind=kind, label=text(en, zh), group=group, key=key,
                method=method, capability=capability, **kw)


def toggle(en, zh, key, cap, group='settings'):
    return field('toggle', en, zh, group, key, 'set' + key[0].upper() + key[1:], cap)


def slider(en, zh, group, key, method, cap, low, high, **kw):
    return field('slider', en, zh, group, key, method, cap, min=low, max=high, **kw)


def choice(en, zh, group, key, method, cap, values):
    return field('choice', en, zh, group, key, method, cap,
                 options=[{'value': v, 'label': text(a, b)} for v, a, b in values])


def page(id, en, zh, fields):
    return dict(id=id, label=text(en, zh), fields=fields)


volume = slider('Volume', '音量', 'playback', 'volume', 'setVolume', 'volumeSupported', 0, 30)
noise = choice('Noise control', '噪声控制', 'noiseControl', 'mode', 'setMode', 'supported',
               [(0, 'Off', '关闭'), (1, 'Noise cancelling', '降噪'), (2, 'Ambient sound', '环境声')])
# Ambient has a separate capability from ANC. All frontends filter this option.
noise['options'][2]['capability'] = 'ambientSupported'
play = field('button', 'Play / pause', '播放 / 暂停', 'playback', method='togglePlayback', capability='playbackSupported')
pages = [
    page('home', 'Overview', '概览', [
        field('label', 'Model', '型号', 'app', 'modelName'),
        field('label', 'Battery (%)', '电量 (%)', 'playback', 'batteryLeft'),
        field('label', 'Track', '曲目', 'playback', 'title'),
        field('label', 'Artist', '艺术家', 'playback', 'artist'),
        field('label', 'Codec', '编码', 'playback', 'codec'), volume, play,
        field('button', 'Previous track', '上一曲', 'playback', method='previousTrack', capability='playbackSupported'),
        field('button', 'Next track', '下一曲', 'playback', method='nextTrack', capability='playbackSupported')]),
    page('connection', 'Connection', '设备连接', [
        field('label', 'Status', '状态', 'connection', 'state'),
        field('label', 'Device error', '设备错误', 'connection', 'errorDetail'),
        field('button', 'Refresh paired devices', '刷新已配对设备', 'app', method='refreshDevices'),
        field('devices', 'Paired devices', '已配对设备', 'connection', 'devices'),
        field('button', 'Disconnect', '断开连接', 'app', method='disconnectDevice')]),
    page('sound', 'Sound', '声音', [
        toggle('DSEE', 'DSEE', 'dseeEnabled', 'dseeSupported'),
        toggle('Prioritize sound quality', '音质优先', 'soundQualityPrior', 'connectionQualitySupported'),
        toggle('Background music', '背景音乐', 'listeningModeEnabled', 'listeningModeSupported'),
        toggle('Cinema mode', '影院模式', 'cinemaModeEnabled', 'listeningModeSupported'),
        choice('Room size', '房间大小', 'settings', 'listeningRoomSize', 'setListeningRoomSize', 'listeningModeSupported',
               [(0, 'Small', '小'), (1, 'Medium', '中'), (2, 'Large', '大')]),
        toggle('Adaptive volume', '自适应音量', 'adaptiveVolumeEnabled', 'adaptiveVolumeSupported'),
        toggle('Safe listening', '安全聆听', 'safeListeningEnabled', 'safeListeningSupported')]),
    page('noise', 'Noise control', '降噪 / 环境声', [noise,
        slider('Ambient level', '环境声等级', 'noiseControl', 'ambientLevel', 'setAmbientLevel', 'ambientSupported', 1, 20),
        toggle('Focus on voice', '聚焦人声', 'focusOnVoice', 'ambientSupported', 'noiseControl'),
        toggle('Adaptive noise control', '自适应降噪', 'adaptiveEnabled', 'adaptiveSupported', 'noiseControl')]),
    page('equalizer', 'Equalizer', '均衡器', [
        field('choice', 'Preset', '预设', 'equalizer', 'presetId', 'selectPreset', 'supported',
              source='presetOptions', valueKey='id'),
        *[slider(label, label, 'equalizer', 'bands', 'setBand', 'supported', -10, 10, index=i)
          for i, label in enumerate(['400 Hz', '1 kHz', '2.5 kHz', '6.3 kHz', '16 kHz'])],
        slider('Clear Bass', '清晰低音', 'equalizer', 'clearBass', 'setClearBass', 'supported', -10, 10)]),
    page('behavior', 'Headphone behavior', '耳机行为', [
        toggle('Speak to chat', '智能免摘', 'speakToChatEnabled', 'speakToChatSupported'),
        choice('Speech sensitivity', '语音检测灵敏度', 'settings', 'speakToChatSensitivity', 'setSpeakToChatSensitivity', 'speakToChatSupported',
               [(0, 'Automatic', '自动'), (1, 'High', '高'), (2, 'Low', '低')]),
        toggle('Auto pause', '自动暂停', 'autoPauseEnabled', 'autoPauseSupported'),
        toggle('Auto power off', '自动关机', 'autoPowerOffEnabled', 'autoPowerOffSupported'),
        toggle('Head gestures', '头部手势', 'headGestureEnabled', 'headGestureSupported'),
        toggle('Capture voice', '捕捉语音', 'captureVoiceEnabled', 'captureVoiceSupported')]),
    page('touch', 'Touch controls', '触控', [toggle('Touch sensor control panel', '触摸传感器控制面板',
        'touchPanelEnabled', 'touchPanelSupported')] + [
        field('choice', en, zh, 'settings', key, method, 'touchSupported', source='touchOptions', valueKey='value')
        for en, zh, key, method in [('Left', '左侧', 'touchLeft', 'setTouchLeft'), ('Right', '右侧', 'touchRight', 'setTouchRight')]]),
    page('voice', 'Voice guidance', '语音提示', [
        toggle('Voice guidance', '语音提示', 'voiceGuidanceEnabled', 'voiceGuidanceSupported'),
        slider('Voice guidance volume', '语音提示音量', 'settings', 'voiceGuidanceVolume', 'setVoiceGuidanceVolume', 'voiceGuidanceVolumeSupported', -2, 2)]),
    page('devices', 'Device management', '设备管理', [
        toggle('Connect to two devices simultaneously', '同时连接两台设备', 'multipointEnabled', 'multipointSupported'),
        toggle('Pairing mode', '配对模式', 'pairingMode', 'supported', 'deviceManager'),
        field('label', 'Connected devices', '连接设备', 'deviceManager', 'devices')])]
# Complete the existing business surface; widgets remain toolkit-owned.
by_id = {p['id']: p for p in pages}
by_id['home']['fields'] += [
    field('label', en, zh, 'playback', key) for en, zh, key in [
        ('Right battery (%)', '右耳电量 (%)', 'batteryRight'),
        ('Case battery (%)', '充电盒电量 (%)', 'batteryCase'),
        ('Left charging', '左耳充电中', 'batteryLeftCharging'),
        ('Right charging', '右耳充电中', 'batteryRightCharging'),
        ('Case charging', '充电盒充电中', 'batteryCaseCharging'),
        ('Firmware', '固件', 'firmware'), ('Album', '专辑', 'album'),
        ('Device alert', '设备提示', 'alertMessage'),
        ('Sound pressure', '声压', 'safeListeningPressure')]]
by_id['home']['fields'].append(field('button', 'Power off headphones', '关闭耳机电源',
    'playback', method='powerOff', capability='powerOffSupported'))
by_id['noise']['fields'].append(choice('Adaptive sensitivity', '自适应降噪灵敏度', 'noiseControl',
    'adaptiveSensitivity', 'setAdaptiveSensitivity', 'adaptiveSupported',
    [(0, 'Standard', '标准'), (1, 'High', '高'), (2, 'Low', '低')]))
by_id['behavior']['fields'].append(choice('Speech mode timeout', '智能免摘关闭时间', 'settings',
    'speakToChatModeOut', 'setSpeakToChatModeOut', 'speakToChatSupported',
    [(0, 'Fast', '快'), (1, 'Normal', '正常'), (2, 'Slow', '慢'), (3, 'Never', '从不')]))
power = choice('Auto power off time', '自动关机时间', 'settings', 'autoPowerOffValue',
    'setAutoPowerOffValue', 'autoPowerOffSupported',
    [(0, '5 minutes', '5 分钟'), (4, '15 minutes', '15 分钟'), (1, '30 minutes', '30 分钟'),
     (2, '60 minutes', '60 分钟'), (3, '180 minutes', '180 分钟'),
     (16, 'When removed', '摘下时'), (17, 'Disabled', '关闭')])
power['options'][5]['capability'] = 'autoPowerOffWearingDetectionSupported'
by_id['behavior']['fields'].append(power)
by_id['touch']['fields'].append(choice('NC / Ambient button', '降噪 / 环境声按钮', 'settings',
    'ncButtonFunction', 'setNcButtonFunction', 'ncButtonSupported',
    [(1, 'NC / Ambient / Off', '降噪 / 环境声 / 关闭'), (2, 'NC / Ambient', '降噪 / 环境声'),
     (3, 'NC / Off', '降噪 / 关闭'), (4, 'Ambient / Off', '环境声 / 关闭')]))
by_id['devices']['fields'] += [field('choice', en, zh, 'deviceManager', method=method,
    capability=capability, source='devices', valueKey='mac') for en, zh, method, capability in [
        ('Switch playback source', '切换播放设备', 'switchDevice', 'sourceSwitchSupported'),
        ('Connect paired device', '连接已配对设备', 'connectDevice', 'supported'),
        ('Disconnect paired device', '断开已配对设备', 'disconnectDevice', 'supported'),
        ('Unpair device', '取消设备配对', 'unpairDevice', 'supported')]]
pages.append(page('application', 'App settings', '应用设置', [
    choice('Language', '语言', 'preferences', 'locale', 'setLocale', '',
           [('en', 'English', 'English'), ('zh_CN', '简体中文', '简体中文')]),
    choice('Appearance', '外观', 'preferences', 'themeMode', 'setThemeMode', '',
           [('system', 'Follow system', '跟随系统'), ('light', 'Light', '浅色'), ('dark', 'Dark', '深色')])]))
catalog = dict(version=1, pages=pages,
               messages=json.loads(Path(__file__).with_name('messages.json').read_text(encoding='utf-8')),
               quick=page('quick', 'Quick controls', '快捷控制', [pages[0]['fields'][0], pages[0]['fields'][1], volume, noise, play]))
if __name__ == '__main__':
    Path(__file__).with_name('catalog.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
