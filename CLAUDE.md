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
- Mode texte (touche `A` depuis le rebind ; `Q` à l'origine) implémenté en v2.2.0
- Strip-down v2.3.0 : retiré Zoom, Lens, inline-zoom, overlays
  clavier/souris, ripple visualizer, hooks bas-niveau, dialog
  Configure colors. Exe passé de ~400 KB à ~346 KB.
- Build MSVC v143 vérifié OK (`build.bat` produit
  `bin/Release/x64/DemoInk.exe`)
- Setup git : `origin` = fork perso, `upstream` = stefankueng/demohelper
- Branches `feature/text-mode` et `feature/strip-down` conservées,
  fusionnées dans `main`

- Thèmes Light/Dark + cycle Transparent via touche `Q` (palettes
  parallèles, recolore les annotations).
- Cadres "tableau" via touche `Z` (`feature/board-frames`) : cadre A
  (whiteboard clair) ↔ cadre B (slate sombre), dessinés en vectoriel
  GDI+ dans `PaintBoardFrame()`. Cadre peint sous les annotations.
  Règle de wipe partagée avec `Q` : seul le départ depuis Transparent
  pristine vide les dessins ; Q↔Z / Z↔Z / Q↔Q préservent.
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

**LIVRÉ (2026-06-08) — Installable + autostart au démarrage Windows.**
Tout est mergé et poussé sur `main`. Cible = ouverture de session (app tray,
pas un service).
- ✅ **Single-instance** (`7e2d9c1`) : mutex nommé
  `DemoInk_SingleInstance_Mutex` en tête de `wWinMain`.
- ✅ **Autostart** (`a434d90`) : item tray « Start with Windows »
  (`ID_TRAYCONTEXT_AUTOSTART`=32808), helpers
  `IsAutostartEnabled()`/`SetAutostart()` (clé `HKCU\...\Run`, valeur `DemoInk`).
- ✅ **Installeur** (`2489072`) : `installer/DemoInk.iss` Inno Setup, install
  per-user `{localappdata}\Programs\DemoInk`, AppVersion 2.3.0, task autostart
  (même valeur Run `DemoInk`), pas de redist (CRT statique). Build :
  `"…\Inno Setup 6\ISCC.exe" installer\DemoInk.iss` →
  `installer\Output\DemoInk-2.3.0-Setup.exe`.
- ✅ **Police texte plus douce** (`2d63c07`) : Segoe Print + taille par défaut
  plus grande (l'ex-branche `feat/softer-text-font`, intégrée).
- ✅ **Caret clignotant** en mode texte (`5d2a0dc`).

**LIVRÉ (2026-06-11) — Rebind raccourcis + defaults + lissage.** Branches
`feature/rebind-shortcuts` puis `feature/text-options-and-defaults`, mergées
dans `main`. Détail : docs/modifications.md.
- ✅ **Rebind main gauche** + retrait du mode marqueur. Nouveau mapping :
  `A`=texte, `W`=erase all, `Q`=fond uni, `Z`=cadre tableau ; `Backspace`
  undo, `0-9` couleurs, `↑↓`/`←→` épaisseur/couleur, `T` opaque↔alpha,
  `Esc` sortie. Anciennes `e`/`b`/`n` non liées. Pas d'indicateur de mode
  (écarté : on est quasi toujours en draw, le mode texte = caret clignotant).
- ✅ **Caret sur la ligne de base** : le texte repose au bas du caret.
- ✅ **Rouge au lancement** (`colorindex` non persisté).
- ✅ **Alpha du texte suit le thème** (semi-transp. clair, plein sombre).
- ✅ **Lissage du trait** : `DrawCurve` spline cardinale (tension 0.5).

**LIVRÉ (2026-06-11) — Options à onglets + réglages produit.** Branche
`feature/options-tabs`, mergée dans `main`. Cap assumé : DemoInk devient un
**produit publiable** (pas juste l'outil perso). Détail : docs/modifications.md.
Le dialog Options unique est devenu un **PropertySheet à onglets** Win32.
- ✅ **Palier 0** — squelette onglets General/Draw (refactor, comportement
  inchangé) ; `ICC_TAB_CLASSES` ajouté.
- ✅ **Palier 1** — onglet **Text** : police (Segoe Print/UI, Ink Free,
  Consolas) + taille par défaut ; `DrawLine.fontName` ; `ResolveTextFont()`
  fallback Segoe Print.
- ✅ **Palier 2** — onglet **Draw** : couleur (index 0-9) + épaisseur par
  défaut au lancement (`Draw/defaultcolor` / `defaultpenwidth`) ; épaisseur
  non persistée d'une session à l'autre.
- ✅ **Palier 3** — onglet **Colors** : palettes Light/Dark éditables (10
  pastilles, `ChooseColor`, Reset). `ApplyTheme` data-driven (`[Colors]` INI
  + fallback `DEFAULT_COLORS_LIGHT/DARK`).
- ✅ **Palier 4** — onglet **Screenshot** : auto-capture désactivable, dossier
  racine configurable (Browse), détection Meet optionnelle
  (`Screenshot/enabled` / `folder` / `meetdetect`). Sous-dossiers renommés en
  anglais `By client` / `By date`.

**LIVRÉ (2026-06-12) — Molette = taille + réglages sur toile vide.** Branche
`feature/wheel-resizes`. La molette change l'épaisseur (draw) / la police
(texte) au lieu de cycler les couleurs ; cycle couleur sur `←/→` et `0-9`.
Épaisseur **et** couleur réglables avant le premier trait (retrait du garde
`!m_drawLines.empty()`).

**PROCHAINS CHANTIERS :**
- **Shortcuts** (le plus lourd) : rebinder certaines touches → table
  d'accélérateurs dynamique (`CreateAcceleratorTable` depuis l'INI au lieu de
  `LoadAccelerators`). Disposition AZERTY/QWERTY **hors scope** (juste
  réassigner action↔touche).
- **Background custom au clear** : couleur/image de fond configurable.

**RÈGLE DE RELEASE — toujours 2 artefacts.** Un seul `DemoInk.exe` couvre les
deux modes. À chaque publication, fournir : (1) le **portable**
`bin/Release/x64/DemoInk.exe`, et (2) l'**installeur**
`installer/Output/DemoInk-<ver>-Setup.exe` (recompiler le `.iss` après
`build.bat`). Bumper `MyAppVersion` dans le `.iss` au changement de version.
Détail : docs/modifications.md.

**PROCHAIN CHANTIER — Re-bind des raccourcis + repère visuel du mode courant.**
Regrouper les raccourcis autour de Ctrl+Shift et afficher un indicateur du
mode actif. Voir docs/modifications.md.

Acquis (renommages faits) :
- ✅ Repo GitHub renommé `kevinmazille/demoink` ; remote `origin` =
  `https://kevinmazille@github.com/kevinmazille/demoink.git`.
- ✅ Dossier local renommé `perso/demoink`.

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

- `origin` → fork personnel (`kevinmazille/demoink`)
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
