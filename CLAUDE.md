# DemoInk — Fork personnel

Fork du projet [stefankueng/demohelper](https://github.com/stefankueng/demohelper)
(renommé **DemoInk** pour le produit, le 2026-05-30) pour ajouter des
fonctionnalités personnelles, principalement autour du mode dessin.

**Convention de renommage** : seul le *produit* est renommé (exe, titres,
chemins runtime, docs). Les *noms de fichiers source*
(`DemoHelper.cpp/.h/.sln/.vcxproj/.rc`…) sont **conservés tels quels** pour
rester mergeable avec l'upstream. Le nom de l'exe est piloté par
`<TargetName>DemoInk</TargetName>` dans `src/DemoHelper.vcxproj`.

## Build

Compilateur : MSVC v143 (Visual Studio 2022 Community) + Windows 11 SDK.

```bash
build.bat
```

Sortie : `bin/Release/x64/DemoInk.exe`

Le sous-module `sktoolslib/` est obligatoire — récupérable via :
```bash
git submodule update --init --recursive
```

## Architecture

Application Win32 C++ pure (pas de MFC, pas de Qt).

Depuis la v2.3.0 (`feature/strip-down`), l'app a été élaguée pour
ne garder QUE le mode Draw (+ mode Texte). Les modes Zoom, Lens,
inline-zoom, ainsi que tous les overlays clavier/souris et le ripple
de clic ont été retirés.

- **Point d'entrée** : `src/DemoHelper.cpp` (`wWinMain`)
- **Cœur** : `src/MainWindow.cpp/h` — gère le mode Draw + mode Texte,
  rendu GDI+
- **Commandes** : `src/Commands.cpp` — handlers pour les actions
  (undo, couleurs, épaisseur, texte, etc.) via accélérateurs clavier
- **Options dialog** : `src/OptionsDlg.cpp` — réduit à hotkey draw,
  fade seconds, monitor selection
- **Sous-module** : `sktoolslib/` — utilitaires (BaseWindow, IniSettings,
  hyperlink, etc.)
- **Rendu** : GDI+ pour tout le dessin (lignes, formes, texte)

## Mode Draw — détails clés

Toutes les annotations sont stockées dans `std::deque<DrawLine> m_drawLines`
(`MainWindow.h:204`). Chaque `DrawLine` a un `LineType` (`MainWindow.h:46-53`) :
Hand, Straight, Arrow, Rectangle, Ellipse.

Le `LineType` peut changer **pendant** le drag selon les modificateurs
clavier — voir `WM_MOUSEMOVE` (`MainWindow.cpp:814-934`).

Le rendu se fait dans `WM_PAINT` (`MainWindow.cpp:651-742`) qui itère sur
`m_drawLines` et utilise GDI+ (`Gdiplus::Graphics`).

Les raccourcis du mode draw sont définis dans la table d'accélérateurs Win32
(`src/DemoHelper.rc:59-86`).

## Modifications planifiées

Voir [docs/modifications.md](docs/modifications.md).

## État actuel — pour reprendre

**Version courante : 2.3.0** — strip-down mergé dans `main`.

Acquis :
- Mode texte (touche `Q`) implémenté en v2.2.0
- Strip-down v2.3.0 : retiré Zoom, Lens, inline-zoom, overlays
  clavier/souris, ripple visualizer, hooks bas-niveau, dialog
  Configure colors. Exe passé de ~400 KB à ~346 KB.
- Build MSVC v143 vérifié OK (`build.bat` produit
  `bin/Release/x64/DemoInk.exe`)
- Setup git : `origin` = fork perso, `upstream` = stefankueng/demohelper
- Branches `feature/text-mode` et `feature/strip-down` conservées,
  fusionnées dans `main`

- Thèmes Light/Dark + cycle Transparent via touche `B` (palettes
  parallèles, recolore les annotations).
- Cadres "tableau" via touche `N` (`feature/board-frames`) : cadre A
  (whiteboard clair) ↔ cadre B (slate sombre), dessinés en vectoriel
  GDI+ dans `PaintBoardFrame()`. Cadre peint sous les annotations.
  Règle de wipe partagée avec `B` : seul le départ depuis Transparent
  pristine vide les dessins ; B↔N / N↔N / B↔B préservent.
  Concepts de référence dans `background image/`.
- Auto-screenshot (`feature/auto-screenshot`) : à la sortie du mode draw
  (Esc ou hotkey) si annotations, capture fond+dessins en PNG sous
  `%USERPROFILE%\Pictures\DemoInk\`, double arbo `Par client\<meet>\
  AAAA-MM-JJ\` et `Par date\AAAA-MM-JJ\<meet>\`. Nom du client extrait
  du titre de la fenêtre Chrome `Meet - <nom> - Google Chrome`.
  `SaveScreenshot()` / `GetMeetName()` dans `MainWindow.cpp`.
- Renommage produit **DemoInk** (`feature/rename-demoink`) : exe, titres,
  About/Help, version info, chemins runtime (`DemoInk.ini`,
  `Pictures\DemoInk`). Noms de fichiers source gardés pour les merges
  upstream. Build vérifié → `bin/Release/x64/DemoInk.exe`.

**CHANTIER EN COURS (2026-06-08) — Installable + autostart au démarrage Windows.**
Plan : `~/.claude/plans/starry-munching-kitten.md`. Cible = ouverture de session
(app tray, pas un service). Branches **chaînées, pas encore mergées/poussées** :
`main` → `feat/softer-text-font` → `feature/single-instance` →
`feature/autostart-toggle` (courante).
- ✅ **Palier 1** (`feature/single-instance`, `7e2d9c1`) : mutex nommé
  `DemoInk_SingleInstance_Mutex` en tête de `wWinMain`. Build + test OK.
- ✅ **Palier 2** (`feature/autostart-toggle`, `a434d90`) : item tray
  « Start with Windows » (`ID_TRAYCONTEXT_AUTOSTART`=32808), helpers
  `IsAutostartEnabled()`/`SetAutostart()` (clé `HKCU\...\Run`, valeur `DemoInk`).
  Build OK ; test visuel de la coche à confirmer par Kevin.
- ⬜ **Palier 3** (`feature/installer` à créer) : `installer/DemoInk.iss` Inno
  Setup, install per-user `{localappdata}\Programs\DemoInk`, AppVersion 2.3.0,
  task autostart (même valeur Run), pas de redist (CRT statique). Inno PAS
  installé → `winget install --id JRSoftware.InnoSetup -e` d'abord.
- Ensuite : merger la chaîne dans `main` (décider d'abord le sort de
  `feat/softer-text-font` après test police), push.

Idées plus tard :
- **Renommer le repo GitHub** `kevinmazille/demohelper` → `demoink` (par Kevin),
  puis remote `origin`. Avant le post LinkedIn #5 "DemoInk".
- Renommer le dossier local `perso/demohelper` → `demoink` (vérifier `includeIf`).
- Re-bind des raccourcis autour de Ctrl+Shift + repère visuel du mode courant.
  Voir docs/modifications.md.

## Pièges à connaître (env de l'utilisateur)

**Comptes GitHub multiples** : la machine a deux comptes `gh` configurés
(`kevinmazille` perso, `kmazille_sfemu` pro). Pour ce repo perso :
- Le remote `origin` embarque `kevinmazille@` dans l'URL
  (`https://kevinmazille@github.com/...`) pour que Git Credential Manager
  choisisse automatiquement le bon token sans demander.
- Identité git du commit : configurée via `~/.gitconfig` global (perso),
  `includeIf` bascule sur l'identité pro pour les repos sous
  `claude-projects/pro/`.
- Avant tout `gh repo …` perso : vérifier `gh auth status` et au besoin
  `gh auth switch --user kevinmazille`.

**Build** : Visual Studio 2022 Community avec workload C++ desktop
installé. Si erreur `cl.exe not found`, relancer
`vs_installer.exe modify --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64`.

## Remotes git

- `origin` → fork personnel (`kevinmazille/demohelper`)
- `upstream` → repo original (`stefankueng/demohelper`)

Pour récupérer les màj amont :
```bash
git fetch upstream
git merge upstream/main
```

## Conventions

- Style C++ : suit le code existant (Allman braces, camelCase pour méthodes,
  préfixe `m_` pour membres). Ne pas reformatter le code existant.
- Pas de réécriture pour la réécriture : respecter la structure du repo
  upstream pour faciliter les futurs merges.
- Toute nouvelle feature → branche dédiée (ex: `feature/text-mode`).
