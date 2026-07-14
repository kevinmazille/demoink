# DemoInk

**A lightweight screen-annotation overlay for live demos and presentations.**
Scribble, draw shapes, and type directly on top of whatever is on your screen,
then exit. It runs quietly in the system tray and is triggered by a
customizable hotkey.

<p align="center">
  <a href="https://github.com/kevinmazille/demoink/releases/latest">
    <img src="https://img.shields.io/github/v/release/kevinmazille/demoink?label=download&style=for-the-badge" alt="Download latest release">
  </a>
</p>

## ⬇️ Download

Grab the latest build from the **[Releases page](https://github.com/kevinmazille/demoink/releases/latest)** — available for both Windows and macOS.

**Windows**

| | File | Notes |
|---|---|---|
| **Installer** (recommended) | `DemoInk-1.0.0-Setup.exe` | Per-user install, no admin required, optional start with Windows |
| **Portable** | `DemoInk.exe` | Single executable, nothing to install |

> **⚠️ Windows SmartScreen warning?** DemoInk isn't code-signed, so Windows may
> show *"Windows protected your PC"*. Click **More info → Run anyway**, or
> right-click the file → **Properties → Unblock → OK**. This is expected for
> small, unsigned open-source tools.

**macOS** (13 Ventura or later)

| | File | Notes |
|---|---|---|
| **Disk image** | `DemoInk-1.2.0.dmg` | Open it, then drag **DemoInk** into **Applications** |

> **⚠️ "Unidentified developer" warning?** The macOS build isn't signed, so on
> first launch **right-click the app → Open** to get past Gatekeeper (only needed
> once). When prompted, grant **Screen Recording** — used for the auto-screenshot;
> the in-app **Permissions** panel (⌘⇧P) walks you through it.

## What it does

- **Draw anywhere**: freehand (smoothed strokes), straight lines, arrows,
  rectangles, ellipses — the shape switches live with modifier keys while you
  drag
- **Text annotations** with a blinking caret and configurable font/size
- **Backgrounds**: transparent (over the live desktop), solid light/dark, or
  "board" frames (whiteboard / slate) — each replaceable with your own image
- **Auto-screenshot on exit**: if you drew anything, the annotated screen is
  saved as a PNG under `Pictures\DemoInk`, filed by date (and by Google Meet
  client when detected)
- **Fully configurable**: tabbed Options dialog (General, Draw, Text, Colors,
  Background, Screenshot, Shortcuts) with rebindable keys
- **Tray / menu-bar app**: single instance, optional start at login

## Default keys (draw mode)

| Key | Action | Key | Action |
|---|---|---|---|
| `A` | Text mode | `X` | Opaque ↔ see-through |
| `W` | Erase all | `0`–`9` | Colors |
| `Q` | Solid background | Wheel / `↑` `↓` | Brush size (font size in text) |
| `Z` | Board frame | `←` `→` | Cycle color |
| `Backspace` | Undo | `Esc` | Exit (and auto-screenshot) |

Every letter shortcut is rebindable in **Options → Shortcuts**.

## macOS

DemoInk has a full native **Swift/AppKit** port under [`mac/`](mac/) — every
feature above works on macOS, plus start-at-login from the menu bar. Toggle draw
mode with **⌘⇧D** and open the **Permissions** panel with **⌘⇧P**. Download the
`.dmg` from the [Releases page](https://github.com/kevinmazille/demoink/releases/latest);
see [`docs/mac-port.md`](docs/mac-port.md) for the design and status.

To build it yourself, open `mac/DemoInk/DemoInk.xcodeproj` in Xcode, or package a
release `.dmg` with `mac/package.sh` (requires Xcode command-line tools).

## Building from source (Windows)

Prebuilt binaries for both platforms are on the
[Releases page](https://github.com/kevinmazille/demoink/releases/latest) — you
only need to build if you're developing. (For macOS, see the section above.)

```bash
git submodule update --init --recursive   # fetch sktoolslib
build.bat                                  # -> bin/Release/x64/DemoInk.exe
```

Requires MSVC v143 (Visual Studio 2022, C++ desktop workload) and the
Windows 11 SDK.

## Credits

DemoInk is a personal fork of
[DemoHelper](https://github.com/stefankueng/demohelper) by
[Stefan Küng](https://github.com/stefankueng). This fork strips the tool down
to a pure drawing overlay (the original zoom, lens and keystroke/mouse overlays
were removed) and adds a text mode, light/dark/board themes, and
auto-screenshots. The Windows source keeps the upstream layout to stay easy to
merge with upstream.
