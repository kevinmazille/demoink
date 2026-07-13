import AppKit

/// The DemoInk preferences window — a toolbar-style tabbed sheet (the macOS
/// idiom, equivalent to the Windows tabbed PropertySheet). This first slice
/// carries the Draw and Text tabs; Colors / Background / Screenshot / Shortcuts
/// come in later Stage 8 passes.
///
/// Changes are written straight to `Settings` (UserDefaults) as the user edits;
/// they take effect on the next draw session, matching the Windows "defaults at
/// launch" semantics.
final class PreferencesWindowController: NSWindowController {
    static let shared = PreferencesWindowController()

    private init() {
        let tabVC = NSTabViewController()
        tabVC.tabStyle = .toolbar

        let draw = DrawPrefsViewController()
        draw.title = "Draw"
        let drawItem = NSTabViewItem(viewController: draw)
        drawItem.image = NSImage(systemSymbolName: "pencil.tip", accessibilityDescription: "Draw")
        tabVC.addTabViewItem(drawItem)

        let text = TextPrefsViewController()
        text.title = "Text"
        let textItem = NSTabViewItem(viewController: text)
        textItem.image = NSImage(systemSymbolName: "textformat", accessibilityDescription: "Text")
        tabVC.addTabViewItem(textItem)

        let screenshot = ScreenshotPrefsViewController()
        screenshot.title = "Screenshot"
        let screenshotItem = NSTabViewItem(viewController: screenshot)
        screenshotItem.image = NSImage(systemSymbolName: "camera", accessibilityDescription: "Screenshot")
        tabVC.addTabViewItem(screenshotItem)

        let shortcuts = ShortcutsPrefsViewController()
        shortcuts.title = "Shortcuts"
        let shortcutsItem = NSTabViewItem(viewController: shortcuts)
        shortcutsItem.image = NSImage(systemSymbolName: "keyboard", accessibilityDescription: "Shortcuts")
        tabVC.addTabViewItem(shortcutsItem)

        let window = NSWindow(contentViewController: tabVC)
        window.title = "DemoInk Settings"
        window.styleMask = [.titled, .closable, .miniaturizable]
        window.isReleasedWhenClosed = false
        super.init(window: window)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    /// Shows the window, creating it lazily and bringing it to the front.
    func show() {
        NSApp.activate(ignoringOtherApps: true)
        window?.center()
        showWindow(nil)
        window?.makeKeyAndOrderFront(nil)
    }
}

// MARK: - Shared layout helper

/// A vertical form built from label/control rows, laid out in a grid so the
/// controls align. Keeps the two tab controllers terse.
private func makeForm(rows: [(String, NSView)]) -> NSView {
    let grid = NSGridView(views: rows.map { label, control in
        let l = NSTextField(labelWithString: label)
        l.alignment = .right
        return [l, control]
    })
    grid.rowSpacing = 14
    grid.columnSpacing = 10
    grid.column(at: 0).xPlacement = .trailing
    grid.translatesAutoresizingMaskIntoConstraints = false

    let container = NSView()
    container.addSubview(grid)
    NSLayoutConstraint.activate([
        grid.leadingAnchor.constraint(equalTo: container.leadingAnchor, constant: 24),
        grid.trailingAnchor.constraint(lessThanOrEqualTo: container.trailingAnchor, constant: -24),
        grid.topAnchor.constraint(equalTo: container.topAnchor, constant: 24),
        grid.bottomAnchor.constraint(lessThanOrEqualTo: container.bottomAnchor, constant: -24),
        container.widthAnchor.constraint(greaterThanOrEqualToConstant: 380),
    ])
    return container
}

// MARK: - Draw tab

/// Default color (palette index 0–9) and pen width a new draw session starts at.
private final class DrawPrefsViewController: NSViewController {
    private let colorPopup = NSPopUpButton()
    private let widthSlider = NSSlider()
    private let widthLabel = NSTextField(labelWithString: "")

    override func loadView() {
        // The palette index is theme-independent; show swatch names from the
        // light palette purely as a human-readable label for each index.
        colorPopup.target = self
        colorPopup.action = #selector(colorChanged)
        for i in 0..<10 {
            colorPopup.addItem(withTitle: "Color \(i)")
            if let item = colorPopup.item(at: i) {
                item.image = swatch(for: i)
            }
        }
        colorPopup.selectItem(at: Settings.defaultColorIndex)

        widthSlider.minValue = Double(DrawModel.minPenWidth)
        widthSlider.maxValue = Double(DrawModel.maxPenWidth)
        widthSlider.doubleValue = Double(Settings.defaultPenWidth)
        widthSlider.target = self
        widthSlider.action = #selector(widthChanged)
        widthSlider.widthAnchor.constraint(equalToConstant: 200).isActive = true
        updateWidthLabel()

        let widthRow = NSStackView(views: [widthSlider, widthLabel])
        widthRow.orientation = .horizontal
        widthRow.spacing = 8

        view = makeForm(rows: [
            ("Default color:", colorPopup),
            ("Default pen width:", widthRow),
        ])
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        // A draw session may have persisted new defaults since this tab was
        // built; re-sync the controls so they show the current values.
        colorPopup.selectItem(at: Settings.defaultColorIndex)
        widthSlider.doubleValue = Double(Settings.defaultPenWidth)
        updateWidthLabel()
    }

    private func swatch(for index: Int) -> NSImage {
        let size = NSSize(width: 24, height: 12)
        let img = NSImage(size: size)
        img.lockFocus()
        DrawModel.lightPalette[index].withAlphaComponent(1).setFill()
        NSBezierPath(rect: NSRect(origin: .zero, size: size)).fill()
        img.unlockFocus()
        return img
    }

    private func updateWidthLabel() {
        widthLabel.stringValue = "\(Int(widthSlider.doubleValue.rounded())) px"
    }

    @objc private func colorChanged() {
        Settings.defaultColorIndex = colorPopup.indexOfSelectedItem
    }

    @objc private func widthChanged() {
        let w = widthSlider.doubleValue.rounded()
        Settings.defaultPenWidth = CGFloat(w)
        updateWidthLabel()
    }
}

// MARK: - Text tab

/// Default font and size for the text mode.
private final class TextPrefsViewController: NSViewController {
    private let fontPopup = NSPopUpButton()
    private let sizeStepper = NSStepper()
    private let sizeField = NSTextField()

    override func loadView() {
        fontPopup.target = self
        fontPopup.action = #selector(fontChanged)
        fontPopup.addItems(withTitles: DrawModel.textFonts)
        // Render each item in its own typeface for a quick preview.
        for (i, name) in DrawModel.textFonts.enumerated() {
            if let item = fontPopup.item(at: i), let font = NSFont(name: name, size: 14) {
                item.attributedTitle = NSAttributedString(string: name, attributes: [.font: font])
            }
        }
        if let idx = DrawModel.textFonts.firstIndex(of: Settings.defaultFontName) {
            fontPopup.selectItem(at: idx)
        }

        sizeField.doubleValue = Double(Settings.defaultFontSize)
        sizeField.alignment = .right
        sizeField.target = self
        sizeField.action = #selector(sizeFieldChanged)
        sizeField.widthAnchor.constraint(equalToConstant: 60).isActive = true

        sizeStepper.minValue = Double(DrawModel.minFontSize)
        sizeStepper.maxValue = Double(DrawModel.maxFontSize)
        sizeStepper.increment = Double(DrawModel.fontStep)
        sizeStepper.doubleValue = Double(Settings.defaultFontSize)
        sizeStepper.valueWraps = false
        sizeStepper.target = self
        sizeStepper.action = #selector(sizeStepperChanged)

        let sizeRow = NSStackView(views: [sizeField, sizeStepper])
        sizeRow.orientation = .horizontal
        sizeRow.spacing = 4

        view = makeForm(rows: [
            ("Default font:", fontPopup),
            ("Default size:", sizeRow),
        ])
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        // Re-sync from Settings in case a draw session changed the defaults.
        if let idx = DrawModel.textFonts.firstIndex(of: Settings.defaultFontName) {
            fontPopup.selectItem(at: idx)
        }
        sizeField.doubleValue = Double(Settings.defaultFontSize)
        sizeStepper.doubleValue = Double(Settings.defaultFontSize)
    }

    @objc private func fontChanged() {
        Settings.defaultFontName = DrawModel.textFonts[fontPopup.indexOfSelectedItem]
    }

    @objc private func sizeStepperChanged() {
        applySize(sizeStepper.doubleValue)
    }

    @objc private func sizeFieldChanged() {
        applySize(sizeField.doubleValue)
    }

    /// Clamps to the model range, then writes and syncs both controls.
    private func applySize(_ raw: Double) {
        let clamped = min(max(raw, Double(DrawModel.minFontSize)), Double(DrawModel.maxFontSize))
        Settings.defaultFontSize = CGFloat(clamped)
        sizeField.doubleValue = clamped
        sizeStepper.doubleValue = clamped
    }
}

// MARK: - Screenshot tab

/// Auto-capture on/off, optional Meet detection, and the root folder the
/// two-tree layout is written under. Mirrors the Windows Screenshot tab.
private final class ScreenshotPrefsViewController: NSViewController {
    private let enabledCheck = NSButton(checkboxWithTitle: "Auto-save a screenshot on exit", target: nil, action: nil)
    private let meetCheck = NSButton(checkboxWithTitle: "Detect the client name from a Google Meet tab", target: nil, action: nil)
    private let folderField = NSTextField()

    override func loadView() {
        enabledCheck.target = self
        enabledCheck.action = #selector(enabledChanged)
        meetCheck.target = self
        meetCheck.action = #selector(meetChanged)

        folderField.isEditable = false
        folderField.isSelectable = true
        folderField.lineBreakMode = .byTruncatingMiddle
        folderField.widthAnchor.constraint(equalToConstant: 260).isActive = true

        let browse = NSButton(title: "Browse…", target: self, action: #selector(browseFolder))
        browse.bezelStyle = .rounded
        let reset = NSButton(title: "Default", target: self, action: #selector(resetFolder))
        reset.bezelStyle = .rounded

        let folderRow = NSStackView(views: [folderField, browse, reset])
        folderRow.orientation = .horizontal
        folderRow.spacing = 6

        view = makeForm(rows: [
            ("", enabledCheck),
            ("", meetCheck),
            ("Save folder:", folderRow),
        ])
        syncControls()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        syncControls()
    }

    private func syncControls() {
        enabledCheck.state = Screenshot.isEnabled ? .on : .off
        meetCheck.state = Screenshot.isMeetDetectEnabled ? .on : .off
        // Meet detection only matters when capture is on.
        meetCheck.isEnabled = Screenshot.isEnabled
        folderField.stringValue = Screenshot.rootFolder?.path ?? ""
    }

    @objc private func enabledChanged() {
        Screenshot.isEnabled = (enabledCheck.state == .on)
        syncControls()
    }

    @objc private func meetChanged() {
        Screenshot.isMeetDetectEnabled = (meetCheck.state == .on)
    }

    @objc private func browseFolder() {
        let panel = NSOpenPanel()
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        panel.prompt = "Choose"
        panel.directoryURL = Screenshot.rootFolder
        if panel.runModal() == .OK, let url = panel.url {
            Screenshot.configuredFolderPath = url.path
            syncControls()
        }
    }

    @objc private func resetFolder() {
        Screenshot.configuredFolderPath = nil
        syncControls()
    }
}

// MARK: - Shortcuts tab

/// Rebind the in-overlay letter shortcuts (text / erase / cycle background /
/// cycle board), mirroring the Windows Shortcuts tab. Each row shows the action
/// and a button displaying its current key; clicking the button captures the
/// next letter pressed. Only single letters are accepted, and a letter already
/// bound to another action is rejected (anti-duplicate, like Windows).
private final class ShortcutsPrefsViewController: NSViewController {
    private var buttons: [Settings.ShortcutAction: KeyCaptureButton] = [:]
    private let hint = NSTextField(labelWithString: "")

    override func loadView() {
        var rows: [(String, NSView)] = []
        for action in Settings.ShortcutAction.allCases {
            let button = KeyCaptureButton()
            button.onCapture = { [weak self] key in
                if let key { self?.tryBind(key, to: action) } else { self?.syncButtons() }
            }
            buttons[action] = button
            rows.append(("\(action.title):", button))
        }

        let resetButton = NSButton(title: "Reset to defaults", target: self, action: #selector(resetAll))
        resetButton.bezelStyle = .rounded
        rows.append(("", resetButton))

        hint.font = .systemFont(ofSize: 11)
        hint.textColor = .secondaryLabelColor
        hint.stringValue = "Click a key, then press a letter (A–Z). Arrows, digits, Esc and Delete are fixed."
        rows.append(("", hint))

        view = makeForm(rows: rows)
        syncButtons()
    }

    override func viewWillAppear() {
        super.viewWillAppear()
        syncButtons()
    }

    private func syncButtons() {
        for (action, button) in buttons {
            button.title = Settings.shortcutKey(action).uppercased()
            button.isRecording = false
        }
    }

    /// Binds `key` to `action` unless another action already uses it.
    private func tryBind(_ key: String, to action: Settings.ShortcutAction) {
        let lower = key.lowercased()
        // Reject anything but a single a–z letter.
        guard lower.count == 1, let c = lower.first, c.isLetter, c.isASCII else {
            NSSound.beep()
            syncButtons()
            return
        }
        if let other = Settings.action(forKey: lower), other != action {
            NSSound.beep()
            hint.stringValue = "\"\(lower.uppercased())\" is already used for \(other.title)."
            hint.textColor = .systemRed
            syncButtons()
            return
        }
        Settings.setShortcutKey(lower, for: action)
        hint.stringValue = "Click a key, then press a letter (A–Z). Arrows, digits, Esc and Delete are fixed."
        hint.textColor = .secondaryLabelColor
        syncButtons()
    }

    @objc private func resetAll() {
        Settings.resetShortcuts()
        syncButtons()
    }
}

/// A button that, once clicked, captures the next key pressed and reports it via
/// `onCapture`. While armed it shows "…" and installs a local key-down monitor.
private final class KeyCaptureButton: NSButton {
    /// Reports the captured key, or nil if the capture was cancelled (Esc).
    var onCapture: ((String?) -> Void)?
    private var monitor: Any?

    var isRecording = false {
        didSet {
            if isRecording {
                title = "…"
                arm()
            } else {
                disarm()
            }
        }
    }

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        bezelStyle = .rounded
        setButtonType(.momentaryPushIn)
        target = self
        action = #selector(clicked)
        widthAnchor.constraint(greaterThanOrEqualToConstant: 60).isActive = true
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    @objc private func clicked() {
        isRecording = true
    }

    private func arm() {
        disarm()
        monitor = NSEvent.addLocalMonitorForEvents(matching: .keyDown) { [weak self] event in
            guard let self else { return event }
            let key = event.charactersIgnoringModifiers ?? ""
            self.isRecording = false
            // Esc cancels without rebinding.
            self.onCapture?(event.keyCode == 53 ? nil : key)
            return nil  // swallow the event so it doesn't type into anything
        }
    }

    private func disarm() {
        if let monitor {
            NSEvent.removeMonitor(monitor)
            self.monitor = nil
        }
    }

    deinit { disarm() }
}
