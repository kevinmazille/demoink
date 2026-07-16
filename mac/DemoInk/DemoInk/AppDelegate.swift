import AppKit
import Carbon.HIToolbox

class AppDelegate: NSObject, NSApplicationDelegate {
    private static let bundleIdentifier = "com.kevinmazille.DemoInk"

    private var statusItem: NSStatusItem!
    private var overlayWindow: OverlayWindow?
    private var hotkeyManager: HotkeyManager?
    private var permissionsHotkey: HotkeyManager?
    private var autostartItem: NSMenuItem!

    func applicationDidFinishLaunching(_ notification: Notification) {
        SingleInstance.ensureOnlyInstance(bundleIdentifier: Self.bundleIdentifier)

        NSApp.setActivationPolicy(.accessory)

        setUpStatusItem()
        hotkeyManager = HotkeyManager(
            keyCode: UInt32(kVK_ANSI_D),
            modifiers: UInt32(cmdKey | shiftKey),
            signature: "DINK"
        ) { [weak self] in
            self?.toggleOverlay()
        }

        // A fixed shortcut to the permissions panel so the grant flow is always
        // reachable and never lost in System Settings.
        permissionsHotkey = HotkeyManager(
            keyCode: UInt32(kVK_ANSI_P),
            modifiers: UInt32(cmdKey | shiftKey),
            signature: "DPRM"
        ) {
            PermissionsWindowController.shared.show()
        }
    }

    private func setUpStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        if let button = statusItem.button {
            // Prefer an SF Symbol (renders reliably as a template image); fall
            // back to a text glyph if the symbol is unavailable.
            if let image = NSImage(systemSymbolName: "pencil.tip", accessibilityDescription: "DemoInk") {
                image.isTemplate = true
                button.image = image
            } else {
                button.title = "DemoInk"
            }
        }

        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Toggle Draw Mode (⌘⇧D)", action: #selector(toggleOverlay), keyEquivalent: ""))
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Settings…", action: #selector(showSettings), keyEquivalent: ","))
        menu.addItem(NSMenuItem(title: "Permissions… (⌘⇧P)", action: #selector(showPermissions), keyEquivalent: ""))
        menu.addItem(.separator())
        autostartItem = NSMenuItem(title: "Start at Login", action: #selector(toggleAutostart), keyEquivalent: "")
        menu.addItem(autostartItem)
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: "Quit", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q"))
        menu.delegate = self
        statusItem.menu = menu
    }

    @objc private func toggleAutostart() {
        // Toggle relative to the live system state, then resync the checkmark.
        Autostart.setEnabled(!Autostart.isEnabled)
        autostartItem.state = Autostart.isEnabled ? .on : .off
    }

    @objc private func showSettings() {
        PreferencesWindowController.shared.show()
    }

    @objc private func showPermissions() {
        PermissionsWindowController.shared.show()
    }

    @objc private func toggleOverlay() {
        if let overlayWindow {
            // Auto-save the annotated screen (no-op if nothing was drawn) while
            // the window is still on-screen — the desktop below it must be
            // capturable for the Transparent theme. Only then close it.
            let view = overlayWindow.contentView as? OverlayView
            self.overlayWindow = nil
            // Remember the last-used color/width/font as the new launch defaults.
            view?.persistSessionDefaults()
            let closeIt: () -> Void = { overlayWindow.close() }
            if let view {
                view.saveScreenshotIfNeeded(completion: closeIt)
            } else {
                closeIt()
            }
        } else {
            guard let screen = screenUnderMouse() else { return }
            // Freeze the desktop as the VERY FIRST thing, synchronously, before we
            // touch focus or windows. Activating DemoInk deactivates the front app,
            // which dismisses any transient popup/tooltip attached to the pointer —
            // and some apps drop it on the first key/focus twitch. Every millisecond
            // between the hotkey and the grab is a chance for the popup to vanish,
            // so we grab immediately on this thread (CGDisplayCreateImage, a few ms)
            // rather than via async ScreenCaptureKit (which enumerates all windows
            // first — tens of ms, long enough to lose the popup in some apps).
            // Returns nil if Screen Recording isn't granted → falls back to live.
            let still = Screenshot.freezeDisplay(screen: screen)

            let window = OverlayWindow(screen: screen)
            let view = window.contentView as? OverlayView
            view?.frozenDesktop = still
            view?.onExit = { [weak self] in
                self?.toggleOverlay()
            }
            overlayWindow = window

            // Order the overlay in and render the frozen still FIRST, while the
            // front app is still active. Only then activate/take focus. Activating
            // deactivates that app — which dismisses its popup on the live desktop
            // and shuffles window focus; doing it before the overlay is painted let
            // that live flicker show for one frame (the visible "bump"). With the
            // opaque frozen still already covering the screen at .screenSaver level,
            // the deactivation happens entirely behind it and is invisible.
            window.orderFrontRegardless()
            window.displayIfNeeded()
            NSApp.activate(ignoringOtherApps: true)
            window.makeKey()
        }
    }

    /// The screen the pointer is currently on, so the overlay opens where the
    /// user is working (multi-monitor) rather than always on the main display.
    /// Falls back to the main screen if none matches.
    private func screenUnderMouse() -> NSScreen? {
        let mouse = NSEvent.mouseLocation
        return NSScreen.screens.first { NSMouseInRect(mouse, $0.frame, false) }
            ?? NSScreen.main
    }
}

extension AppDelegate: NSMenuDelegate {
    /// Resync the autostart checkmark whenever the menu opens — the user may have
    /// toggled the login item in System Settings behind our back.
    func menuWillOpen(_ menu: NSMenu) {
        autostartItem?.state = Autostart.isEnabled ? .on : .off
    }
}
