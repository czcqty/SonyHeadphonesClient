import AppKit
import Foundation
import CoreFoundation

typealias Object = [String: Any]
func object(_ value: Any?) -> Object { value as? Object ?? [:] }
func string(_ value: Any?) -> String { value.map { String(describing: $0) } ?? "" }
func integer(_ value: Any?) -> Int { (value as? NSNumber)?.intValue ?? 0 }

struct Options {
    var backend = "", catalog = "", locale = "", theme = "", profile = "WH-1000XM6", route = "home"
    var live = false, panel = false
    init() {
        var args = Array(CommandLine.arguments.dropFirst())
        while !args.isEmpty {
            let key = args.removeFirst()
            if key == "--live" { live = true; continue }
            if key == "--panel" { panel = true; continue }
            guard !args.isEmpty else { fatalError("Missing argument for \(key)") }
            let value = args.removeFirst()
            switch key {
            case "--backend": backend = value
            case "--catalog": catalog = value
            case "--locale": locale = value
            case "--theme": theme = value
            case "--profile": profile = value
            case "--route": route = value
            default: fatalError("Unknown option: \(key)")
            }
        }
        if catalog.isEmpty { catalog = Bundle.main.path(forResource: "catalog", ofType: "json") ?? "clients/common/catalog.json" }
        guard !backend.isEmpty, ["", "en", "zh_CN"].contains(locale), ["", "system", "light", "dark"].contains(theme) else {
            fatalError("Usage: SoundConnect --backend /absolute/host [--locale en|zh_CN] [--profile ID] [--panel] [--live]")
        }
    }
}

final class Action: NSObject {
    let body: (Any?) -> Void
    init(_ body: @escaping (Any?) -> Void) { self.body = body }
    @objc func run(_ sender: Any?) { body(sender) }
}

final class Backend {
    var state: Object = [:], error = ""
    var changed: (() -> Void)?
    private var process: Process?
    private var input: Pipe?
    private var closing = false
    private var sent = 0
    func start(_ options: Options) {
        let process = Process(), input = Pipe(), output = Pipe(), diagnostics = Pipe()
        self.process = process; self.input = input
        process.executableURL = URL(fileURLWithPath: options.backend)
        process.arguments = options.live ? ["--live"] : ["--profile", options.profile]
        if !options.locale.isEmpty { process.arguments! += ["--locale", options.locale] }
        if !options.theme.isEmpty { process.arguments! += ["--theme", options.theme] }
        process.standardInput = input; process.standardOutput = output; process.standardError = diagnostics
        diagnostics.fileHandleForReading.readabilityHandler = { handle in _ = handle.availableData }
        process.terminationHandler = { [weak self] _ in
            DispatchQueue.main.async {
                guard let self = self, !self.closing else { return }
                self.error = "Backend stopped"; self.state = [:]; self.changed?()
            }
        }
        do {
            try process.run()
            DispatchQueue.global(qos: .userInitiated).async { [weak self] in
                var buffer = Data()
                while true {
                    let chunk = output.fileHandleForReading.availableData
                    if chunk.isEmpty { break }
                    buffer.append(chunk)
                    while let newline = buffer.firstIndex(of: 10) {
                        let line = buffer.prefix(upTo: newline)
                        buffer.removeSubrange(...newline)
                        guard let event = (try? JSONSerialization.jsonObject(with: line)) as? Object else { continue }
                        DispatchQueue.main.async {
                            guard let self = self, !self.closing else { return }
                            if string(event["type"]) == "state" { if integer(event["version"]) == 1 { self.state = event } else { self.state = [:]; self.error = "unsupported-version" } }
                            if string(event["type"]) == "result" {
                                self.error = (event["ok"] as? Bool == true) ? "" : string(event["errorCode"])
                            }
                            self.changed?()
                        }
                    }
                }
                diagnostics.fileHandleForReading.readabilityHandler = nil
            }
        } catch { self.error = error.localizedDescription; changed?() }
    }
    func send(_ target: String, _ method: String, _ args: [Any] = []) {
        guard let process = process, process.isRunning, !closing else { return }
        do {
            sent += 1
            var data = try JSONSerialization.data(withJSONObject: ["version": 1, "id": String(sent), "target": target, "method": method, "args": args])
            data.append(10)
            try input?.fileHandleForWriting.write(contentsOf: data)
        } catch { self.error = error.localizedDescription; changed?() }
    }
    func stop() {
        closing = true
        try? input?.fileHandleForWriting.close()
        // EOF normally exits immediately; do not leave a Bluetooth helper orphaned.
        if let process = process {
            let deadline = Date().addingTimeInterval(2)
            while process.isRunning && Date() < deadline { Thread.sleep(forTimeInterval: 0.02) }
            if process.isRunning { process.terminate() }
        }
    }
}

final class App: NSObject, NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate {
    var options = Options()
    let backend = Backend()
    var locale = "en", theme = "system"
    var catalog: Object = [:], pages: [Object] = [], actions: [Action] = []
    var updates: [() -> Void] = [], pageViews: [NSView] = []
    var window: NSWindow!, status: NSStatusItem!, popover = NSPopover(), table = NSTableView()
    let message = NSTextField(wrappingLabelWithString: "")
    let quickMessage = NSTextField(wrappingLabelWithString: "")
    func label(_ data: Any?) -> String { let labels = object(data); return string(labels[locale] ?? labels["en"] ?? data) }
    func tr(_ en: String, _ zh: String) -> String { locale == "zh_CN" ? zh : en }
    func dynamic(_ value: String) -> String {
        let messages = object(catalog["messages"])
        if let item = messages[value] { return label(item) }
        for (source, item) in messages {
            let parts = source.components(separatedBy: "%1")
            if parts.count == 2 && value.hasPrefix(parts[0]) && value.hasSuffix(parts[1]) {
                let detail = String(value.dropFirst(parts[0].count).dropLast(parts[1].count))
                return label(item).replacingOccurrences(of: "%1", with: detail)
            }
        }
        return value
    }
    func connectionError() -> String {
        let connection = object(backend.state["connection"])
        let code = string(connection["errorCode"]), detail = string(connection["errorDetail"])
        return code.isEmpty ? detail : dynamic(code) + (detail.isEmpty ? "" : ": " + detail)
    }
    func display(_ value: Any?, _ key: String) -> String {
        if key == "errorDetail" { return connectionError() }
        guard let value = value else { return "—" }
        if let n = value as? NSNumber, CFGetTypeID(n) == CFBooleanGetTypeID() { return dynamic(n.boolValue ? "Yes" : "No") }
        if let items = value as? [Object] { return items.map { string($0["name"]) }.joined(separator: " · ") }
        return ["state", "errorDetail", "alertMessage"].contains(key) ? dynamic(string(value)) : string(value)
    }
    func bind(_ control: NSControl, _ body: @escaping (Any?) -> Void) {
        let action = Action(body); actions.append(action); control.target = action; control.action = #selector(Action.run(_:))
    }
    func button(_ text: String, _ body: @escaping () -> Void) -> NSButton {
        let b = NSButton(title: text, target: nil, action: nil); b.bezelStyle = .rounded
        bind(b) { _ in body() }; return b
    }
    func stack(_ orientation: NSUserInterfaceLayoutOrientation) -> NSStackView {
        let s = NSStackView(); s.orientation = orientation; s.alignment = orientation == .vertical ? .leading : .centerY; s.spacing = 12
        return s
    }
    func applicationDidFinishLaunching(_ notification: Notification) {
        do {
            catalog = object(try JSONSerialization.jsonObject(with: Data(contentsOf: URL(fileURLWithPath: options.catalog))))
        } catch { fatalError("Cannot load binding catalog: \(error)") }
        locale = options.locale.isEmpty ? "en" : options.locale
        pages = catalog["pages"] as? [Object] ?? []
        installMenu()
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 880, height: 660), styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "Sound Connect"; window.minSize = NSSize(width: 720, height: 480); window.isReleasedWhenClosed = false
        let split = NSSplitView(); split.isVertical = true; split.dividerStyle = .thin
        let navigation = NSScrollView(); navigation.hasVerticalScroller = true
        table.addTableColumn(NSTableColumn(identifier: NSUserInterfaceItemIdentifier("page")))
        table.headerView = nil; table.dataSource = self; table.delegate = self; table.rowHeight = 34
        table.style = .sourceList; navigation.documentView = table
        let content = stack(.vertical); content.edgeInsets = NSEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        message.maximumNumberOfLines = 3; content.addArrangedSubview(message)
        for page in pages {
            let view = makePage(page)
            content.addArrangedSubview(view); pageViews.append(view)
            view.widthAnchor.constraint(equalTo: content.widthAnchor, constant: -32).isActive = true
            view.heightAnchor.constraint(equalTo: content.heightAnchor, constant: -80).isActive = true
        }
        split.addArrangedSubview(navigation); split.addArrangedSubview(content)
        navigation.widthAnchor.constraint(greaterThanOrEqualToConstant: 160).isActive = true
        window.contentView = split; split.setPosition(190, ofDividerAt: 0)
        table.selectRowIndexes(IndexSet(integer: pages.firstIndex { string($0["id"]) == options.route } ?? 0), byExtendingSelection: false)
        selectPage()
        status = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        status.button?.image = NSImage(systemSymbolName: "headphones", accessibilityDescription: "Sound Connect")
        status.button?.target = self; status.button?.action = #selector(togglePanel)
        status.button?.sendAction(on: [.leftMouseUp, .rightMouseUp])
        let quick = NSViewController(); quick.view = stack(.vertical)
        let quickStack = quick.view as! NSStackView
        quickStack.edgeInsets = NSEdgeInsets(top: 16, left: 16, bottom: 16, right: 16)
        quickMessage.maximumNumberOfLines = 3; quickStack.addArrangedSubview(quickMessage)
        let quickPage = makePage(object(catalog["quick"])); quickStack.addArrangedSubview(quickPage)
        quickPage.widthAnchor.constraint(equalTo: quickStack.widthAnchor, constant: -32).isActive = true
        quickPage.heightAnchor.constraint(equalToConstant: 280).isActive = true
        let settingsButton = button(tr("All settings…", "全部设置…")) { [weak self] in self?.showSettings() }
        let quitButton = button(tr("Quit", "退出")) { NSApp.terminate(nil) }
        quickStack.addArrangedSubview(settingsButton); quickStack.addArrangedSubview(quitButton)
        updates.append { [weak self] in
            guard let self = self else { return }
            settingsButton.title = self.tr("All settings…", "全部设置…"); quitButton.title = self.tr("Quit", "退出")
        }
        popover.contentViewController = quick; popover.contentSize = NSSize(width: 420, height: 460); popover.behavior = .transient
        backend.changed = { [weak self] in self?.refresh() }; backend.start(options); refresh()
        window.center()
        if options.panel { togglePanel() } else { showSettings() }
    }
    func installMenu() {
        let menu = NSMenu(), appMenu = NSMenu(), item = NSMenuItem()
        menu.addItem(item); item.submenu = appMenu
        appMenu.addItem(withTitle: tr("Settings", "设置"), action: #selector(showSettings), keyEquivalent: ",").target = self
        for (en, zh, selector, key) in [("Play / Pause", "播放 / 暂停", #selector(play), "p"),
                                         ("Next track", "下一曲", #selector(next), "n"),
                                         ("Quick controls", "快捷控制", #selector(togglePanel), "m")] {
            let action = appMenu.addItem(withTitle: tr(en, zh), action: selector, keyEquivalent: key)
            action.keyEquivalentModifierMask = [.control, .option]; action.target = self
        }
        appMenu.addItem(NSMenuItem.separator())
        appMenu.addItem(withTitle: tr("Quit Sound Connect", "退出 Sound Connect"), action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        NSApp.mainMenu = menu
    }
    @objc func play() { backend.send("playback", "togglePlayback") }
    @objc func next() { backend.send("playback", "nextTrack") }
    @objc func disconnect() { backend.send("app", "disconnectDevice") }
    func numberOfRows(in tableView: NSTableView) -> Int { pages.count }
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        NSTextField(labelWithString: label(pages[row]["label"]))
    }
    func tableViewSelectionDidChange(_ notification: Notification) { selectPage() }
    func selectPage() { for (index, view) in pageViews.enumerated() { view.isHidden = index != max(0, table.selectedRow) } }
    @objc func showSettings() { popover.close(); window.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true) }
    @objc func togglePanel() {
        if NSApp.currentEvent?.type == .rightMouseUp, let button = status.button {
            popover.close()
            let menu = NSMenu()
            for (en, zh, selector) in [("All settings", "全部设置", #selector(showSettings)),
                                       ("Play / Pause", "播放 / 暂停", #selector(play)),
                                       ("Next track", "下一曲", #selector(next)),
                                       ("Disconnect", "断开连接", #selector(disconnect))] {
                menu.addItem(withTitle: tr(en, zh), action: selector, keyEquivalent: "").target = self
            }
            menu.addItem(NSMenuItem.separator())
            menu.addItem(withTitle: tr("Quit", "退出"), action: #selector(NSApplication.terminate(_:)), keyEquivalent: "")
            menu.popUp(positioning: nil, at: NSPoint(x: 0, y: button.bounds.minY), in: button)
            return
        }
        if popover.isShown { popover.close() }
        else if status.isVisible, let button = status.button { popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY) }
        else { showSettings() }
    }
    func refresh() {
        let prefs = object(backend.state["preferences"])
        if let next = prefs["locale"] as? String, next != locale {
            let selected = table.selectedRow
            locale = next; installMenu(); table.reloadData()
            if selected >= 0 { table.selectRowIndexes(IndexSet(integer: selected), byExtendingSelection: false) }
        }
        if let mode = prefs["themeMode"] as? String {
            theme = mode
            NSApp.appearance = mode == "system" ? nil : NSAppearance(named: mode == "dark" ? .darkAqua : .aqua)
        }
        message.stringValue = backend.error.isEmpty ? (options.live ? tr("Live device", "实时设备") : tr("Isolated device preview", "独立设备预览")) : dynamic(backend.error)
        if backend.error.isEmpty && !connectionError().isEmpty { message.stringValue = connectionError() }
        quickMessage.stringValue = message.stringValue
        updates.forEach { $0() }
    }
    func sendCommand(_ group: String, _ method: String, _ args: [Any] = []) {
        guard method == "powerOff" || method == "unpairDevice" else { backend.send(group, method, args); return }
        let alert = NSAlert(), power = method == "powerOff"
        alert.messageText = power ? tr("Power off headphones?", "是否关闭耳机电源？") : tr("Unpair device?", "是否取消设备配对？")
        alert.addButton(withTitle: tr("Cancel", "取消"))
        alert.addButton(withTitle: power ? tr("Power off", "关机") : tr("Unpair", "取消配对"))
        alert.beginSheetModal(for: window) { [weak self] response in
            if response == .alertSecondButtonReturn { self?.backend.send(group, method, args) }
        }
    }
    func makePage(_ page: Object) -> NSView {
        let form = stack(.vertical); form.spacing = 18
        let heading = NSTextField(labelWithString: label(page["label"])); heading.font = .preferredFont(forTextStyle: .title2)
        form.addArrangedSubview(heading)
        updates.append { [weak self] in heading.stringValue = self?.label(page["label"]) ?? "" }
        for field in page["fields"] as? [Object] ?? [] {
            let group = string(field["group"]), key = string(field["key"]), kind = string(field["kind"])
            let row = stack(.horizontal), name = NSTextField(wrappingLabelWithString: label(field["label"]))
            name.widthAnchor.constraint(equalToConstant: 170).isActive = true; row.addArrangedSubview(name)
            var control: NSControl
            var values: [Any] = []
            if kind == "toggle" { control = NSSwitch() }
            else if kind == "slider" {
                let slider = NSSlider(value: 0, minValue: Double(integer(field["min"])), maxValue: Double(integer(field["max"])), target: nil, action: nil)
                slider.isContinuous = false; slider.numberOfTickMarks = integer(field["max"]) - integer(field["min"]) + 1
                slider.allowsTickMarkValuesOnly = true; control = slider
            } else if kind == "choice" || kind == "devices" { control = NSPopUpButton(frame: .zero, pullsDown: false) }
            else if kind == "button" { control = button(label(field["label"])) { [weak self] in self?.sendCommand(group, string(field["method"])) }; name.stringValue = "" }
            else { control = NSTextField(wrappingLabelWithString: "—") }
            control.setAccessibilityLabel(label(field["label"]))
            control.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
            row.addArrangedSubview(control); form.addArrangedSubview(row)
            row.widthAnchor.constraint(equalTo: form.widthAnchor).isActive = true
            if kind != "button" && kind != "label" {
                bind(control) { [weak self, weak control] _ in
                    guard let self = self, let control = control else { return }
                    var args: [Any] = []
                    if let toggle = control as? NSSwitch { args = [toggle.state == .on] }
                    if let slider = control as? NSSlider { args = [slider.integerValue] }
                    if let popup = control as? NSPopUpButton {
                        guard values.indices.contains(popup.indexOfSelectedItem) else { return }
                        if kind == "devices" {
                            let device = object(values[popup.indexOfSelectedItem]); self.backend.send("app", "connectDevice", [string(device["name"]), string(device["mac"])]); return
                        }
                        args = [values[popup.indexOfSelectedItem]]
                    }
                    if let index = field["index"] { args.insert(index, at: 0) }
                    self.sendCommand(group, string(field["method"]), args)
                }
            }
            updates.append { [weak self, weak control] in
                guard let self = self, let control = control else { return }
                let label = self.label(field["label"])
                name.stringValue = kind == "button" ? "" : label
                if kind == "button", let button = control as? NSButton { button.title = label }
                control.setAccessibilityLabel(label)
                let state = object(self.backend.state[group]); var current = state[key]
                if let index = field["index"] as? Int { let array = current as? [Any] ?? []; current = array.indices.contains(index) ? array[index] : nil }
                let cap = string(field["capability"])
                control.isEnabled = kind == "label" || group == "app" || group == "preferences" || (string(object(self.backend.state["connection"])["state"]) == "connected" && (cap.isEmpty || state[cap] as? Bool == true) && (field["index"] == nil || current != nil))
                if kind == "devices" { control.isEnabled = !(state[key] as? [Any] ?? []).isEmpty && !["connected", "connecting"].contains(string(object(self.backend.state["connection"])["state"])) }
                if let toggle = control as? NSSwitch { toggle.state = current as? Bool == true ? .on : .off }
                if let slider = control as? NSSlider, self.window.firstResponder !== slider { slider.integerValue = integer(current) }
                if let text = control as? NSTextField { text.stringValue = self.display(current, key) }
                if let popup = control as? NSPopUpButton {
                    var names: [String] = []; values = []
                    if kind == "devices" {
                        for device in state[key] as? [Object] ?? [] { names.append(string(device["name"])); values.append(device) }
                    } else if let source = field["source"] as? String {
                        for item in state[source] as? [Object] ?? [] { names.append(source == "devices" ? string(item["name"]) : self.dynamic(string(item["name"]))); values.append(item[string(field["valueKey"])] ?? "") }
                    } else {
                        for item in field["options"] as? [Object] ?? [] {
                            let cap = string(item["capability"])
                            if !cap.isEmpty && state[cap] as? Bool != true { continue }
                            names.append(self.label(item["label"])); values.append(item["value"] ?? "")
                        }
                    }
                    if popup.itemTitles != names { popup.removeAllItems(); popup.addItems(withTitles: names) }
                    if kind == "devices" { popup.selectItem(at: -1) }
                    else { popup.selectItem(at: values.firstIndex { string($0) == string(current) } ?? -1) }
                }
            }
        }
        let scroll = NSScrollView(); scroll.hasVerticalScroller = true; scroll.drawsBackground = false
        form.translatesAutoresizingMaskIntoConstraints = false; scroll.documentView = form
        NSLayoutConstraint.activate([form.leadingAnchor.constraint(equalTo: scroll.contentView.leadingAnchor), form.topAnchor.constraint(equalTo: scroll.contentView.topAnchor), form.widthAnchor.constraint(equalTo: scroll.contentView.widthAnchor)])
        return scroll
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { status?.isVisible != true }
    func applicationWillTerminate(_ notification: Notification) { backend.stop() }
}

let application = NSApplication.shared
let delegate = App()
application.setActivationPolicy(.regular)
application.delegate = delegate
application.run()
