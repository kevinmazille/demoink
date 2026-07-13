import Carbon.HIToolbox
import AppKit

final class HotkeyManager {
    private var hotKeyRef: EventHotKeyRef?
    private var eventHandler: EventHandlerRef?
    private let onPress: () -> Void
    private let signatureCode: OSType

    init?(keyCode: UInt32, modifiers: UInt32, signature: String, onPress: @escaping () -> Void) {
        self.onPress = onPress
        self.signatureCode = OSType(fourCharCode: signature)

        var eventType = EventTypeSpec(eventClass: OSType(kEventClassKeyboard), eventKind: UInt32(kEventHotKeyPressed))
        let selfPtr = Unmanaged.passUnretained(self).toOpaque()

        let handlerStatus = InstallEventHandler(
            GetApplicationEventTarget(),
            { _, event, userData -> OSStatus in
                guard let userData else { return noErr }
                let manager = Unmanaged<HotkeyManager>.fromOpaque(userData).takeUnretainedValue()

                // Every HotkeyManager installs a handler on the same target and
                // they all listen for any hotkey press, so we must check which
                // hotkey fired and only react to our own. Crucially, when it's
                // not ours we return eventNotHandledErr so Carbon keeps
                // propagating the event to the other handlers — returning noErr
                // would swallow it and the matching manager would never run.
                var firedID = EventHotKeyID()
                let status = GetEventParameter(
                    event,
                    EventParamName(kEventParamDirectObject),
                    EventParamType(typeEventHotKeyID),
                    nil,
                    MemoryLayout<EventHotKeyID>.size,
                    nil,
                    &firedID
                )
                guard status == noErr, firedID.signature == manager.signatureCode else {
                    return OSStatus(eventNotHandledErr)
                }

                manager.onPress()
                return noErr
            },
            1,
            &eventType,
            selfPtr,
            &eventHandler
        )
        guard handlerStatus == noErr else { return nil }

        let hotKeyID = EventHotKeyID(signature: signatureCode, id: 1)
        let registerStatus = RegisterEventHotKey(
            keyCode,
            modifiers,
            hotKeyID,
            GetApplicationEventTarget(),
            0,
            &hotKeyRef
        )
        guard registerStatus == noErr else { return nil }
    }

    deinit {
        if let hotKeyRef {
            UnregisterEventHotKey(hotKeyRef)
        }
        if let eventHandler {
            RemoveEventHandler(eventHandler)
        }
    }
}

private extension OSType {
    init(fourCharCode string: String) {
        var result: UInt32 = 0
        for byte in string.utf8.prefix(4) {
            result = (result << 8) + UInt32(byte)
        }
        self = result
    }
}
