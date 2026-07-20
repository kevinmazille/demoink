# DemoInk — macOS port spec

Decision (2026-06-29): the Mac version is a **native Swift/AppKit rewrite**, not
a port of the Win32 C++ code. The C++ is glued to the Windows API end to end
(GDI+ rendering, Win32 message loop, registry, Shell tray, accelerator tables);
none of it compiles on macOS. What carries over is the **business logic**, which
is small, well understood, and listed below.

Code lives in `mac/` inside this repo (kevinmazille/demoink). The Windows source
keeps the upstream layout untouched so it stays mergeable with stefankueng.

This doc is the day-one checklist. It is written on Windows; nothing here was
compiled. Build/verify happens on the Mac, one stage at a time, per the usual
working agreement (stages + build between each + one commit per stage on a
dedicated branch).

## Validate the hard parts FIRST (POC before any feature work)

These three are the only real technical risks. Build a throwaway POC that proves
all three before writing a single feature. If one doesn't work, the design
changes — better to know on day one.

1. **Transparent, click-through, always-on-top overlay covering the screen.**
   - `NSWindow` borderless, `backgroundColor = .clear`, `isOpaque = false`,
     `level = .screenSaver` (above normal windows), `ignoresMouseEvents` toggled
     per mode (drawing = false, passive = true).
   - `collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary]`
     so it follows the active Space and sits over fullscreen apps (Meet shared
     in fullscreen is the main use case).
   - Drawing surface: an `NSView` with `draw(_:)` using Core Graphics / `NSBezierPath`,
     or SwiftUI `Canvas`. Decide after the POC; AppKit `NSView` is closer to the
     current immediate-mode model.

2. **Global hotkey to toggle draw mode** while another app is focused.
   - `RegisterEventHotKey` (Carbon, still supported) or a small wrapper lib.
   - Requires **Accessibility** permission (TCC prompt). Handle the
     not-yet-granted state gracefully.

3. **Screen capture of the desktop behind the overlay** (for auto-screenshot and
   for the "freeze the desktop" snapshot used by the board/theme modes).
   - `ScreenCaptureKit` (macOS 12.3+). `CGWindowListCreateImage` is deprecated.
   - Requires **Screen Recording** permission (TCC prompt). First launch will
     prompt; the app must survive a denied/late grant.

## Win32 → AppKit mapping

| Concern | Windows (current) | macOS target |
|---|---|---|
| Window / overlay | `CWindow` + Win32 msg loop, `WS_EX_LAYERED` | `NSWindow` borderless + `NSView` |
| Rendering | GDI+ (`Gdiplus::Graphics`) | Core Graphics / `NSBezierPath` (or SwiftUI Canvas) |
| Tray icon + menu | `Shell_NotifyIcon`, `WM_COMMAND` | `NSStatusItem` + `NSMenu` |
| Global hotkey | `RegisterHotKey` / `DRAW_HOTKEY` | `RegisterEventHotKey` (Accessibility) |
| Accelerator table | `CreateAcceleratorTable` from INI | `NSEvent` local monitor / `keyDown` in view |
| Autostart at logon | `HKCU\...\Run` registry value | `SMAppService.mainApp.register()` (macOS 13+) |
| Single instance | named mutex `DemoInk_SingleInstance_Mutex` | `NSRunningApplication` check / lock file |
| Settings store | `DemoInk.ini` (`CIniSettings`) | `UserDefaults` (suite = bundle id) |
| Color picker (Colors tab) | `ChooseColor` | `NSColorPanel` / `NSColorWell` |
| Folder picker (Screenshot tab) | `SHBrowseForFolder` | `NSOpenPanel` (directories) |
| Options dialog (tabbed) | Win32 `PropertySheet`, 7 pages | SwiftUI `TabView` Settings scene, or `NSTabViewController` |
| Detect Meet client name | `GetWindowText` on Chrome window | Accessibility API window title, or AppleScript to Chrome |
| Screenshot encode/save | GDI+ `Bitmap.Save` PNG | `NSBitmapImageRep` → PNG, or `CGImageDestination` |
| Save root | `%USERPROFILE%\Pictures\DemoInk` | `~/Pictures/DemoInk` |

## Business logic that carries over unchanged (portable, no Win32)

These are the parts worth re-implementing faithfully; they hold most of the
product value and are tiny.

- **Annotation model.** `DrawLine` = { lineType (Hand/Straight/Arrow/Rectangle/
  Ellipse/Text), points[], colorIndex, penWidth, alpha, text, fontSize,
  fontName }. Stored in an ordered list (was `std::deque`). Live `LineType`
  switch during a drag via modifier keys.
- **Wipe rule (shared by theme `Q` and board `Z`).** Only leaving the *pristine
  Transparent* state clears annotations to start on a fresh canvas; every later
  switch (Q↔Z, Z↔Z, Q↔Q, Light↔Dark) **preserves** them. (Commands.cpp:188-231.)
- **Themes.** Transparent / Light / Dark. Light palette shared with Transparent;
  Dark palette tuned for black. Two 10-color palettes, editable + reset.
  Defaults: see `DEFAULT_COLORS_LIGHT` / `DEFAULT_COLORS_DARK` in MainWindow.h.
- **Board styles.** None / FrameA (light whiteboard) / FrameB (dark slate),
  painted under the annotations. Each frame is vector-drawn OR replaced by a
  user image (`imagelight`=A, `imagedark`=B). Z cycles 2 steps.
- **Background solid fill.** Q theme fill colors, editable; defaults white/black.
  "Start opaque" forces full-alpha strokes at launch even in light/transparent.
- **Keyboard mapping (current Windows defaults).** `A`=text · `W`=erase all ·
  `Q`=solid background · `Z`=board frame · `X`=opaque↔alpha · `Backspace`=undo ·
  `0-9`=colors · `↑↓`=pen width · `←→`=color · wheel=size · `Esc`=exit.
  The 5 letters are rebindable; the rest are fixed.
- **Auto-screenshot tree.** On exit, if anything was drawn, save PNG under the
  root (default `~/Pictures/DemoInk`):
  - `By date/YYYY-MM-DD/[<client>/]HH-MM-SS.png` — always written.
  - `By client/<client>/YYYY-MM-DD/HH-MM-SS.png` — only when a Meet name found.
  - Client name parsed from the active Chrome tab titled `Meet - <name> - Google Chrome`.
    On Mac the title comes via Accessibility/AppleScript; the parsing rule is the same.
- **Text mode.** Type on canvas, blinking caret on the baseline, alpha follows
  theme (semi-transparent light, solid dark). Font picker list with fallback.
- **Smoothing.** Freehand strokes drawn as a cardinal spline (tension 0.5) —
  `NSBezierPath` Catmull-Rom equivalent.

## macOS-specific gotchas

- **Code signing + notarization.** Without it, Gatekeeper blocks the app and
  `SMAppService` autostart is unreliable. Needs an Apple Developer account
  ($99/yr). **Confirm with Kevin before the build day.** A self-signed/unsigned
  build works for local use but shows the "unidentified developer" wall.
- **TCC permissions** (Screen Recording + Accessibility) prompt on first use and
  must be re-granted after each rebuild during dev (the binary hash changes).
  Expect to toggle them in System Settings repeatedly while iterating.
- **Multi-monitor / Spaces.** Decide whether the overlay is one window per screen
  or follows the active screen. Start with the active screen only.
- **Retina.** Work in points, let the backing scale handle pixels; screenshots
  must capture at native resolution.

## Suggested stage plan (on the Mac)

0. POC: overlay + hotkey + capture (the 3 risks above). Throwaway, prove it works.
1. Tray app skeleton: `NSStatusItem`, menu, single-instance, hotkey toggles a
   blank transparent overlay in/out.
2. Freehand draw + color/width + undo + erase + Esc/exit. Core of the tool.
3. Shapes (straight/arrow/rect/ellipse) with modifier-key live switching + smoothing.
4. Themes (Transparent/Light/Dark) + wipe rule + palettes.
5. Board frames A/B (vector) + optional board images + solid background colors.
6. Text mode + caret + font.
7. Auto-screenshot tree + Meet detection.
8. Settings UI (tabs) wired to UserDefaults; rebindable shortcuts.
9. Autostart (`SMAppService`), packaging, signing + notarization, .dmg.

Build and commit per stage, same as the Windows history.

## Decisions finalized (2026-07-09)

- **No Apple Developer account** → unsigned build, personal use only. Gatekeeper
  warnings expected; workaround: right-click + Open on first launch. Distribution
  to others not planned for now.
- **Target: macOS 13 Ventura+** (`ScreenCaptureKit` + `SMAppService` both available).
- **Settings: UserDefaults** (native macOS idiom). No `.ini` parser needed.

## Post-1.2 fixes

### 1.2.1 (2026-07-16) — freeze the screen on draw-mode entry

**Problem.** Entering draw mode activates DemoInk, which deactivates the front
app and dismisses any pointer-attached popup/tooltip. With the Transparent
theme showing the *live* desktop, that popup was gone before you could draw on
it, and the exit screenshot re-captured the live desktop without it.

**Fix.** Grab the display synchronously as the very first step of the ⌘⇧D
handler (`Screenshot.freezeDisplay` → `CGDisplayCreateImage`, a few ms, before
focus is stolen) and paint that still as the Transparent-theme backdrop
(`OverlayView.frozenDesktop`), reused for the exit screenshot. The synchronous
grab beats async ScreenCaptureKit here: no window-enumeration delay, so the
popup survives in far more apps (e.g. Tableau Desktop, which dropped it before).
The overlay is ordered in and rendered *before* activating, so the deactivation
flicker hides behind the opaque still. Falls back to the live desktop when
Screen Recording isn't granted.

**Known trade-offs.**
- The Transparent backdrop is now a *frozen* still while you draw — a video
  behind the overlay appears paused. Accepted.
- A brief transition ("bump") shows as the frozen overlay appears; inherent to
  capture-then-show. Accepted.
- **Screen Recording permission is lost on every rebuild** (ad-hoc signature →
  new binary hash → TCC treats it as a new app). During dev, re-grant via ⌘⇧P
  and relaunch after each build, or `tccutil reset ScreenCapture
  com.kevinmazille.DemoInk` to start clean. Durable fix (deferred): sign with a
  stable identity so TCC recognises the app across builds.

### Release / tooling notes

- `gh` default repo for this fork: run `gh repo set-default kevinmazille/demoink`
  once (stored in `.git/config` as `gh-resolved = base`). Otherwise `gh` resolves
  to `stefankueng/demohelper` (the `upstream` remote) and release/PR commands hit
  the wrong repo. Per-command escape hatch: `--repo kevinmazille/demoink`.
- Release: `mac/package.sh` builds Release + `.dmg` (version read from the built
  app's `Info.plist`; bump `MARKETING_VERSION` in the Xcode project). Then tag +
  `gh release create v<ver> mac/dist/DemoInk-<ver>.dmg`.
