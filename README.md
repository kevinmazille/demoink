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

## ⬇️ Download (Windows)

Grab the latest build from the **[Releases page](https://github.com/kevinmazille/demoink/releases/latest)**:

| | File | Notes |
|---|---|---|
| **Installer** (recommended) | `DemoInk-1.0.0-Setup.exe` | Per-user install, no admin required, optional start with Windows |
| **Portable** | `DemoInk.exe` | Single executable, nothing to install |

> **⚠️ Windows SmartScreen warning?** DemoInk isn't code-signed, so Windows may
> show *"Windows protected your PC"*. Click **More info → Run anyway**, or
> right-click the file → **Properties → Unblock → OK**. This is expected for
> small, unsigned open-source tools.

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
- **Tray app**: single instance, optional start with Windows

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

A native **Swift/AppKit** port is in progress under [`mac/`](mac/): freehand
draw, shapes, live modifier-key switching, light/dark/board themes, and board
frames are working. It is **not yet released as a binary** — build it with
Xcode. See [`docs/mac-port.md`](docs/mac-port.md) for the design and status.

## Building from source (Windows)

Prebuilt binaries are on the
[Releases page](https://github.com/kevinmazille/demoink/releases/latest) — you
only need to build if you're developing.

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
