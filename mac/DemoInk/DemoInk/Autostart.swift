import ServiceManagement

/// Launch-at-login support, the macOS equivalent of the Windows autostart
/// (`HKCU\...\Run` value `DemoInk`). Uses `SMAppService.mainApp`, which registers
/// the app itself as a login item (macOS 13+); the user can also toggle it from
/// System Settings › General › Login Items, and this stays in sync via `status`.
///
/// No helper bundle or privileged daemon is involved — it's the tray app itself
/// starting at login, matching the Windows "app tray, pas un service" choice.
enum Autostart {
    /// Whether the app is currently registered to start at login.
    static var isEnabled: Bool {
        SMAppService.mainApp.status == .enabled
    }

    /// Registers or unregisters the app as a login item. Returns false if the
    /// system call throws (e.g. the user disabled it in System Settings and the
    /// state is locked); the caller can then resync the UI from `isEnabled`.
    @discardableResult
    static func setEnabled(_ enabled: Bool) -> Bool {
        do {
            if enabled {
                // register() is idempotent but throws if already required by the
                // system; guard on the current status to keep it quiet.
                if SMAppService.mainApp.status != .enabled {
                    try SMAppService.mainApp.register()
                }
            } else {
                try SMAppService.mainApp.unregister()
            }
            return true
        } catch {
            NSLog("DemoInk: autostart \(enabled ? "register" : "unregister") failed: \(error)")
            return false
        }
    }
}
