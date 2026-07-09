import AppKit

enum SingleInstance {
    static func ensureOnlyInstance(bundleIdentifier: String) {
        let running = NSRunningApplication.self
        _ = running // silence unused import warning on some toolchains

        let apps = NSWorkspace.shared.runningApplications.filter {
            $0.bundleIdentifier == bundleIdentifier && $0.processIdentifier != ProcessInfo.processInfo.processIdentifier
        }
        guard !apps.isEmpty else { return }

        for other in apps {
            other.activate(options: [])
        }
        exit(0)
    }
}
