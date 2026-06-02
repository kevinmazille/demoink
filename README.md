# DemoInk

A lightweight screen-annotation overlay for live demos: scribble, draw
shapes, and type directly on top of whatever is on screen, then exit.

*DemoInk* is a personal fork of
[DemoHelper](https://tools.stefankueng.com/DemoHelper.html) by
[Stefan Küng](https://github.com/stefankueng/demohelper). The fork has been
stripped down to a pure drawing overlay (the original zoom, lens and
keystroke/mouse overlays were removed) and extended with a text mode,
light/dark/board themes, and auto-screenshots filed by client and date.

It runs unobtrusively in the system tray and is activated by a customizable
hotkey or the tray icon context menu. On exit, if anything was drawn, the
annotated screen is auto-saved as a PNG under `Pictures\DemoInk`, filed by
client (from the active Google Meet tab) and by date.

## Credits

All the heavy lifting comes from Stefan Küng's original *DemoHelper* —
see the [upstream project](https://github.com/stefankueng/demohelper).
This fork keeps the original source layout to stay easy to merge with
upstream.
