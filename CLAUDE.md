# DemoHelper — Fork personnel

Fork du projet [stefankueng/demohelper](https://github.com/stefankueng/demohelper)
pour ajouter des fonctionnalités personnelles, principalement autour du mode dessin.

## Build

Compilateur : MSVC v143 (Visual Studio 2022 Community) + Windows 11 SDK.

```bash
build.bat
```

Sortie : `bin/Release/x64/DemoHelper.exe`

Le sous-module `sktoolslib/` est obligatoire — récupérable via :
```bash
git submodule update --init --recursive
```

## Architecture

Application Win32 C++ pure (pas de MFC, pas de Qt).

- **Point d'entrée** : `src/DemoHelper.cpp` (`wWinMain`)
- **Cœur** : `src/MainWindow.cpp/h` — gère les 3 modes (Draw/Zoom/Lens),
  les hooks clavier/souris globaux, le rendu GDI+
- **Commandes** : `src/Commands.cpp` — handlers pour les actions
  (undo, couleurs, épaisseur, etc.) déclenchées par accélérateurs clavier
- **Sous-module** : `sktoolslib/` — utilitaires (BaseWindow, IniSettings,
  AnimationManager, etc.)
- **Rendu** : GDI+ pour le dessin, Direct2D pour les overlays texte
  (clavier/souris affichés en surimpression)

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

**Branche active : `feature/text-mode`** (créée, vide pour l'instant —
prête à recevoir l'implémentation du mode texte).

Acquis :
- Build MSVC v143 vérifié OK (`build.bat` produit
  `bin/Release/x64/DemoHelper.exe` ~400 KB)
- Exe testé en interactif : tous les modes (Draw/Zoom/Lens) fonctionnent
- Setup git : `origin` = fork perso, `upstream` = stefankueng/demohelper
- Commit initial sur `main` poussé sur le fork (`fac329b`)
- Code source intact : aucune modification appliquée à ce stade

Prochaine étape : implémenter le mode texte selon le plan dans
[docs/modifications.md](docs/modifications.md#en-cours--mode-texte-featuretext-mode).
À décider à la reprise : implémentation en un seul commit
(plus rapide) ou par paliers avec build à chaque étape (plus sûr).

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
