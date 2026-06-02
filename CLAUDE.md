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

Prochaine étape (idées) :
- **Renommer le repo GitHub** `kevinmazille/demohelper` →
  `kevinmazille/demoink` (à faire par Kevin / sur OK), puis mettre à jour
  le remote `origin` local. Garder `upstream` = stefankueng/demohelper.
  À faire avant de publier le post LinkedIn d'annonce (post #5 "DemoInk").
- Renommer le dossier local `claude-projects/perso/demohelper` → `demoink`
  (vérifier le `includeIf` git).
- Re-bind des raccourcis autour de Ctrl+Shift (tout faire d'une main) +
  repère visuel du mode courant. Voir docs/modifications.md.

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
