# Plan des modifications

## Réalisé : Mode texte (`feature/text-mode` → mergé dans main, v2.2.0)

### Spec

Permet d'écrire du texte directement à l'écran depuis le mode draw.

**Workflow** :
1. En mode Draw, appuyer sur `Q` → entre en mode texte
2. Le curseur de saisie suit la souris **en permanence**
3. Frappe libre → les caractères s'affichent attachés à la souris,
   couleur = `m_colorIndex` actif au moment d'entrer en mode texte
4. **Molette** → modifie la taille de la police du texte courant
5. **Backspace** → retire le dernier caractère tapé
6. **Clic gauche** → "pose" le texte à la position actuelle de la souris,
   le texte devient permanent (intégré à `m_drawLines`),
   puis **on sort du mode texte**
7. **Esc** → annule le texte en cours (rien n'est posé) et sort du mode texte

Pour taper un nouveau texte ailleurs, ré-appuyer sur `Q`.

**Pendant la saisie** : tous les autres raccourcis (0-9, e, c, m, t, etc.)
sont désactivés pour ne pas interférer avec la frappe. La table
d'accélérateurs est court-circuitée tant que `m_bTextMode == true`.

**Police** : Segoe UI, taille initiale = `m_currentPenWidth * 4`.

### Implémentation

**`MainWindow.h`** :
- Ajouter `LineType::Text` à l'enum (ligne 46-53)
- Ajouter membres dans `DrawLine` :
  - `std::wstring text`
  - `int fontSize`
- Ajouter membres dans `CMainWindow` :
  - `bool m_bTextMode = false`

**`MainWindow.cpp`** :
- Nouveau handler `WM_CHAR` :
  - Si `m_bTextMode`, append au `text` de la dernière `DrawLine` et
    `InvalidateRect` pour repaint
- Modifier `WM_KEYDOWN` (ou via accélérateur) pour traiter `Backspace` en
  mode texte : retire le dernier caractère au lieu de retirer la dernière
  ligne (`ID_CMD_UNDOLINE` actuel)
- `WM_MOUSEMOVE` : si `m_bTextMode`, mettre à jour `lineStartPoint` de la
  ligne en cours à chaque déplacement (le texte suit en permanence)
- `WM_LBUTTONDOWN` : si `m_bTextMode`, valider le texte et sortir du mode
  (au lieu de démarrer un trait à main levée)
- `WM_MOUSEWHEEL` : si `m_bTextMode`, modifier `fontSize` au lieu de
  couleur/épaisseur
- `WM_PAINT` : nouveau `case LineType::Text` → utiliser
  `Gdiplus::Graphics::DrawString` avec `Gdiplus::Font(L"Segoe UI", fontSize)`

**`DemoHelper.cpp`** (boucle de message principale) :
- Skip `TranslateAccelerator` quand `m_bTextMode == true` pour que les
  caractères arrivent en `WM_CHAR`

**`DemoHelper.rc`** :
- Ajouter accélérateur `"q", ID_CMD_TEXTMODE, ASCII, NOINVERT`

**`resource.h`** :
- Ajouter `#define ID_CMD_TEXTMODE …` (prochain ID libre)

**`Commands.cpp`** :
- Ajouter case `ID_CMD_TEXTMODE` :
  - Si pas déjà en mode texte : push une nouvelle `DrawLine` vide de
    type `Text`, set `m_bTextMode = true`, init `fontSize`
  - Gérer `ID_CMD_QUITMODE` (Esc) pour annuler le texte en cours
    avant de sortir du mode draw

### Tests manuels (validés)

- [x] `Q` en mode draw entre en mode texte
- [x] Tape "Hello" → s'affiche, suit la souris
- [x] Backspace retire les caractères
- [x] Molette change la taille
- [x] Clic pose le texte et sort du mode texte
- [x] Re-appuyer sur `Q` permet de taper un nouveau texte ailleurs
- [x] Esc annule, le texte courant disparaît, mode texte off
- [x] Pendant frappe, taper "extra" ne déclenche pas clear/marker
- [x] Couleur du texte = couleur active au moment du `Q`
- [x] Le texte posé persiste lors du undo (Backspace en mode draw classique)

### Notes d'implémentation

- Raccourci final : `Q` (au lieu de `I` initialement prévu — plus accessible
  près de la zone Ctrl+Shift).
- Skip de `TranslateAccelerator` quand `m_bTextMode == true` via le getter
  public `CMainWindow::IsInTextMode()` (sinon `Q`, `Backspace`, etc. sont
  capturés par la table d'accélérateurs avant `WM_CHAR`).
- `m_bTextMode` est remis à `false` dans `EndPresentationMode()` pour
  garantir un état propre à chaque sortie du mode draw.

## Réalisé : Strip-down (`feature/strip-down` → mergé dans main, v2.3.0)

L'app a été élaguée pour devenir un overlay de dessin pur. Tout ce qui
n'était pas le mode Draw a été retiré.

### Retiré

- **Zoom mode** : raccourci global, animation, `m_bZooming`,
  `StartZoomingMode`/`EndZoomingMode`/`DrawZoom`, `ID_CMD_ACCEPT`
- **Lens / Live mode** : `m_bLensMode`, `m_magnifierWindow`,
  `MagnifierWindow.cpp/h`, `RectSelectionWnd.cpp/h`,
  `MagInitialize`/`MagUninitialize`
- **Inline zoom** (`z` pendant le draw) : `m_bInlineZoom`,
  `StartInlineZoom`, `m_ptInlineZoom*`
- **Overlay keystrokes** : `KeyboardOverlay.cpp/h`,
  `KeyboardOverlayD2D.cpp/h`, `m_infoOverlay`, `m_keySequence`,
  `m_overlayWnds`, `OverlayPosition`, `WndPositions`
- **Overlay mouseclicks** (log) : `MouseOverlay.cpp/h`,
  `m_mouseOverlay`, `m_bMouseClicks`
- **Visualize mouseclicks** (ripple coloré) : `m_bMouseVisuals`,
  `m_mvL/M/RColor`, dialog `IDD_COLORS`, `ColorButton.cpp/h`,
  `ColorDlg.cpp/h`
- **Hooks bas-niveau** : `LowLevelKeyboardProc`, `LowLevelMouseProc`,
  `m_hKeyboardHook`, `m_hMouseHook`, `m_lastHook*`
- **AnimationManager** : plus utilisé (était lié au zoom)
- **Tray menu** : entrée "Zoom"
- **Options dialog** : hotkeys Zoom/Lens, "Show keystrokes/mouseclicks",
  "Visualize mouseclicks", radios position overlay, "Configure colors"

### Conservé

- Mode Draw complet (couleurs, formes, marker, undo, clear, etc.)
- Mode Texte (v2.2.0)
- Tray icon avec Draw / Options / Help / Exit
- Options dialog (réduit à hotkey draw, fade seconds, monitor selection)
- Help dialog mis à jour

### Impact build

- Exe : ~400 KB → **~346 KB** (-54 KB / -13 %)
- Lignes : ~2800 lignes retirées sur 4 commits
- 12 fichiers .cpp/.h supprimés
- Compilation : ~1900 fonctions → ~1060 fonctions

## Réalisé : Cadres "tableau" (`feature/board-frames` → mergé dans main)

Touche `N` : un repère visuel "je présente sur un tableau" tout en
gardant ~95 % de zone de dessin libre. Calqué sur la logique du cycle
`B` (Light ↔ Dark).

### Comportement

- **`N`** applique le **cadre A** (whiteboard clair : dégradé papier, mat
  gris, liseré clay arrondi, équerres de coin), puis cycle **A ↔ B**. Le
  **cadre B** est un tableau ardoise sombre (cadre biseauté, canvas slate
  en dégradé radial, baseline clay). Le cadre est peint **sous** les
  annotations (rendu dans le DC de fond, dessins re-rendus par-dessus en
  `WM_PAINT`).
- **Règle de wipe partagée B/N** : seul le passage qui **quitte l'état
  Transparent pristine** (Transparent → B *ou* Transparent → N) vide les
  annotations pour une toile fraîche. Tout switch ultérieur (B↔N, N↔N,
  B↔B) **préserve** les annotations. `E` reste l'effacement explicite.
- **`B`** remet `m_boardStyle = None` → revient aux fonds unis sans cadre.
- **Esc + relance** → repart en Transparent pristine sans cadre.

### Implémentation

- `enum class BoardStyle { None, FrameA, FrameB }` + membre `m_boardStyle`
  (`MainWindow.h`).
- `ID_CMD_CYCLEBOARD` (`resource.h`), accélérateur `"n"` (`DemoHelper.rc`).
- Handler `ID_CMD_CYCLEBOARD` (`Commands.cpp`) répliquant la logique `B`.
- `PaintBoardFrame()` (`MainWindow.cpp`) appelée depuis
  `PaintThemeBackground()` : dessin **vectoriel GDI+** (pas de PNG embarqué),
  géométrie exprimée dans l'espace 1920×1080 d'origine puis mise à l'échelle
  de l'écran → net à toute résolution / multi-moniteur.
- Helper local `DrawRoundedRect()` (GraphicsPath).
- `m_boardStyle` remis à `None` dans `StartPresentationMode()`.

### Référence visuelle

Concepts SVG + PNG d'origine dans `background image/` (A = whiteboard,
B = slate, C = minimal non retenu). Le code GDI+ reproduit A et B.

## Réalisé : Auto-screenshot à la sortie (`feature/auto-screenshot` → mergé dans main)

Capture automatique de l'écran annoté en sortie du mode draw, rangée par
client (nom du Meet) et par date.

### Comportement

- **Déclenchement** : à chaque sortie du mode draw — **Esc** *ou* re-appui
  sur le **hotkey de dessin** — si `m_drawLines` non vide. Rien dessiné →
  rien sauvé.
- **Contenu** : fond du thème courant (fill B/N, ou bureau si Transparent)
  **+ annotations** recomposées par-dessus.
- **Racine** : `%USERPROFILE%\Pictures\DemoHelper\`.
- **Double arborescence** (même PNG écrit aux deux endroits si client connu) :
  - `Par client\<client>\AAAA-MM-JJ\HH-MM-SS.png`
  - `Par date\AAAA-MM-JJ\<client>\HH-MM-SS.png`
- **Sans client détecté** → arbre date seulement :
  `Par date\AAAA-MM-JJ\HH-MM-SS.png` (rien dans `Par client`).
- `<client>` = nom extrait du titre de la fenêtre Chrome active
  (`Meet - <nom> - Google Chrome`), caractères interdits Windows nettoyés.

### Implémentation

- Refactor : boucle de rendu des annotations extraite de `WM_PAINT` dans
  `RenderAnnotations(Gdiplus::Graphics&)`, réutilisée par le paint et la
  capture (commit séparé, comportement inchangé).
- `SaveScreenshot()` (`MainWindow.cpp`) : compose fond
  (`hDesktopCompatibleDC`) + `RenderAnnotations` dans un bitmap mémoire,
  encode en PNG via l'encodeur GDI+ (`GetPngEncoderClsid` +
  `Bitmap::Save`). Appelée en tête de `EndPresentationMode()` (avant
  libération des DC) → couvre Esc et hotkey d'un coup.
- `GetMeetName()` : `EnumWindows` + match du pattern de titre Chrome,
  `SanitizeForPath()` pour les caractères interdits.
- `EnsureDirectory()` : création récursive des dossiers.
- Dossier cible via `SHGetKnownFolderPath(FOLDERID_Pictures)`.

### Détection Meet — notes

Aucune dépendance externe (pas de flag `--remote-debugging`, pas d'API
Google). Le nom n'est riche que si la réunion est **planifiée/nommée** ;
réunion instantanée → titre = juste "Meet" → fallback arbre date. Signaux
secondaires repérés mais non utilisés : caméra/micro en cours d'usage via
`HKCU\...\CapabilityAccessManager\ConsentStore` (`LastUsedTimeStop == 0`),
et barre flottante de partage d'écran (fenêtre dédiée) pour distinguer
"en réunion" de "en train de partager".

## Réalisé : Renommage produit DemoInk (`feature/rename-demoink`)

Le fork a tellement divergé de l'upstream (essence = gribouiller sur
l'écran en live pour les SE qui font des démos) qu'il mérite son propre
nom. Nom retenu (décidé 2026-05-30) : **DemoInk** (alternatives écartées :
ScreenScribe — déjà pris sur GitHub dans le même créneau ; Inkcast —
libre mais plus abstrait).

### Principe

Seul le **produit** est renommé ; les **noms de fichiers source**
(`DemoHelper.cpp/.h/.sln/.vcxproj/.rc`…) sont **conservés** pour rester
mergeable avec l'upstream `stefankueng/demohelper`.

### Fait

- **Exe** : `<TargetName>DemoInk</TargetName>` dans `src/DemoHelper.vcxproj`
  → la sortie est `bin/Release/x64/DemoInk.exe`. `build.bat`/`.sln`
  inchangés (le nom de l'exe est piloté par `TargetName`, pas par le nom
  du projet).
- **Chaînes visibles** : titre de fenêtre + nom de classe (`IDS_APP_TITLE`),
  string tray (`IDC_DEMOHELPER`), caption Options, titre du Help, en-tête
  du RTF d'aide → DemoInk. Le syslink "About" est reformulé pour créditer
  DemoHelper / Stefan Küng comme upstream.
- **Version info** (`src/DemoHelper.rc2`, UTF-16) : `FileDescription`,
  `InternalName`, `OriginalFilename`, `ProductName` → DemoInk. `CompanyName`
  (URL upstream) et `LegalCopyright` (Stefan Küng) conservés.
- **Chemins runtime** : `DemoHelper.ini` → `DemoInk.ini`,
  `AppData\DemoHelper` → `DemoInk`, `Pictures\DemoHelper` → `DemoInk`.
  **Reset des prefs accepté** (`Draw/colorindex`, `Draw/currentpenwidth`),
  pas de code de migration.
- **Docs** : `README.md`, `src/ReadMe.txt`, `CLAUDE.md` — crédit upstream
  gardé partout.
- Build MSVC v143 vérifié OK.

### Inchangé (volontairement)

- `version.build.in` / `versioninfo.build` / `default.build` : système de
  build nant **upstream non utilisé** par `build.bat` (qui passe par
  MSBuild). Laissés tels quels pour les merges.
- `choco/` (packaging upstream), regpath debug `Software\DemoHelper\Debug`
  (debug-only), commentaires d'en-tête, GUID du projet, URLs stefankueng.

### Reste à faire (hors code, sur OK de Kevin)

- **Repo GitHub `origin`** : renommer `kevinmazille/demohelper` →
  `kevinmazille/demoink` (GitHub redirige l'ancienne URL ; mettre à jour
  le remote local). **Garder `upstream` = stefankueng/demohelper.**
- **Dossier local** : renommer `claude-projects/perso/demohelper` →
  `demoink` et vérifier le `includeIf` git
  (voir [[reference-github-accounts]]).
- À faire **avant** de publier le post LinkedIn d'annonce (post #5
  "DemoInk" dans la DB Notion "LinkedIn Backlog", déjà rédigé), pour que
  le lien du 1er commentaire pointe vers le bon repo.

## Réalisé : Installable + autostart au démarrage Windows

Trois paliers (branches chaînées `feature/single-instance` →
`feature/autostart-toggle` → `feature/installer`). Cible = ouverture de
**session utilisateur** (app tray/GUI, pas un service Windows).

### Palier 1 — Instance unique
Mutex nommé session-local `DemoInk_SingleInstance_Mutex` en tête de
`wWinMain` (`src/DemoHelper.cpp`). Une 2e instance sort en code 0 (pas de
doublon de tray icon). Libéré via `OnOutOfScope`. Prérequis de l'autostart.

### Palier 2 — Toggle autostart in-app
Item de menu tray **« Start with Windows »** (`ID_TRAYCONTEXT_AUTOSTART`)
avec coche dynamique posée avant `TrackPopupMenu` (menu rechargé à chaque
clic droit). Helpers `IsAutostartEnabled()` / `SetAutostart(bool)` dans
`MainWindow.cpp` : clé `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`,
valeur `DemoInk` = `CPathUtils::GetModulePath()` quoté. Pointe vers
l'emplacement courant de l'exe → marche en portable **et** installé.

### Palier 3 — Installeur Inno Setup
`installer/DemoInk.iss` : install **per-user** (`{localappdata}\Programs\
DemoInk`, `PrivilegesRequired=lowest`, pas d'UAC). Bundle le **même** exe
que le mode portable (CRT statique → **aucun redist VC++**). Raccourci menu
Démarrer, task desktop optionnelle, task **autostart** écrivant la **même**
valeur Run `DemoInk` que le toggle in-app (jamais de divergence).
`CloseApplications` ferme une instance en cours avant écrasement ;
`UninstallDelete` retire l'INI runtime pour une désinstall propre.
Build : `winget install --id JRSoftware.InnoSetup -e` puis
`"…\Inno Setup 6\ISCC.exe" installer\DemoInk.iss` →
`installer\Output\DemoInk-2.3.0-Setup.exe` (`Output/` gitignoré).
Test install/désinstall validé (install → tout en place → désinstall →
zéro résidu).

### RÈGLE DE RELEASE — toujours 2 artefacts (portable + installeur)

Un **seul** `DemoInk.exe` couvre les deux modes (l'app retombe sur
`%AppData%\DemoInk` pour l'INI si le dossier de l'exe est read-only ;
l'autostart et l'instance unique sont identiques des deux côtés). Donc à
**chaque publication d'une étape/version**, fournir systématiquement les
**deux** :
1. **Portable** : `bin/Release/x64/DemoInk.exe` tel quel (autonome, zéro DLL).
2. **Installeur** : `installer/Output/DemoInk-<ver>-Setup.exe` (recompiler le
   `.iss` après le build pour qu'il embarque l'exe à jour).

Procédure release : `build.bat` → recompiler le `.iss` → publier les 2
fichiers ensemble (ex. mêmes assets d'une release GitHub). Penser à bumper
`MyAppVersion` dans le `.iss` quand la version change.

## Réalisé : Re-bind des raccourcis main gauche + retrait du marqueur (`feature/rebind-shortcuts`)

Regroupement des actions fréquentes sous la main gauche (touches simples,
sans Ctrl+Shift à maintenir). **Pas d'indicateur visuel de mode** : on est
quasi toujours en mode draw, et le mode texte se reconnaît au caret qui
clignote — l'indicateur initialement envisagé a été écarté.

### Nouveau mapping (mode draw)

| Touche | Action | Ancienne |
|--------|--------|----------|
| `A` | Entrer en mode texte | `Q` |
| `W` | Tout effacer | `E` |
| `Q` | Fond couleur unie (cycle Transparent→Light→Dark) | `B` |
| `Z` | Cadre tableau (whiteboard↔slate) | `N` |
| `Backspace` | Undo dernier objet | inchangé |
| `Delete` | Retirer le premier objet | inchangé |
| `0-9` | Couleur directe | inchangé |
| `↑↓` / `←→` (ou molette) | Épaisseur / couleur | inchangé |
| `T` | Trait opaque ↔ alpha | inchangé |
| `Esc` | Sortir du mode | inchangé |

Remap pur : les anciennes touches `e`/`b`/`n` ne font plus rien (pas
d'alias, pour rester simple). Édité dans la seule table d'accélérateurs
`DemoHelper.rc` ; aucun handler C++ touché (les `case` switchent sur des
IDs de commande inchangés).

### Marqueur retiré

Le mode marqueur (`m`, gros pinceau jaune semi-transparent) était peu
utilisé. Retiré proprement : `case ID_CMD_QUICKTOMARKER`, l'accélérateur
`m`, le `#define`, et les 4 membres dédiés (`m_bMarker`, `m_oldPenWidth`,
`m_oldColorIndex`, `m_oldAlpha`). Cohérent avec l'esprit strip-down.

### Fix annexe : caret du mode texte

Le caret clignotant était ancré sur le haut de la cellule de police
(`lineStartPoint.Y`) alors que `DrawString` réserve un espace vide au-dessus
des glyphes (internal leading, large pour Segoe Print) → le caret dépassait
au-dessus du texte. Désormais ancré sur la **ligne de base** : hauteur =
ascent, bas = baseline, le texte repose au bas du caret. `RenderTextCaret`
(`MainWindow.cpp`).

## Réalisé : Défauts couleur/alpha + lissage du trait (`feature/text-options-and-defaults`)

Trois ajustements demandés à l'usage, sur la même branche que la future
sélection de police.

### Rouge au lancement
La couleur de dessin repart **toujours sur rouge** (index 1) à chaque
ouverture de l'app, au lieu de restaurer la dernière couleur via l'INI.
`colorindex` n'est plus persisté (`WM_CREATE` + sauvegarde dans
`MainWindow.cpp`). Changement libre pendant la session.

### Alpha du texte suit le thème
Le texte héritait toujours d'`alpha = 255` (opaque). Il suit désormais
`m_currentAlpha` comme les traits (`Commands.cpp`, `ID_CMD_TEXTMODE`) :
semi-transparent sur fond Transparent/Light, plein sur fond Dark. Un texte
déjà posé est réaligné au changement de thème (boucle existante des
handlers `Q`/`Z`).

### Lissage du trait à main levée
`RenderAnnotations` reliait les points capturés par des segments droits
(`DrawLines`) → crantelage visible sur les mouvements rapides (Windows
espace les `WM_MOUSEMOVE`). Remplacé par une **spline cardinale**
`DrawCurve` (tension `0.5f`) : courbes fluides sans trop arrondir les
angles serrés. S'applique au rendu live **et** aux screenshots (chemin de
rendu commun).

## Réalisé : Options à onglets + réglages produit (`feature/options-tabs`)

Cap assumé : DemoInk passe d'outil perso à **produit publiable**. Le dialog
Options unique devient un **PropertySheet à onglets** (Win32 natif), pour que
les réglages grandissent par catégorie. Fait par paliers (un build + un
commit chacun).

### Palier 0 — Squelette à onglets
`IDD_OPTIONS` (dialog unique) → PropertySheet avec une *property page* par
onglet (`ShowOptionsSheet` + une proc par page dans `OptionsDlg.cpp`). Les 3
réglages existants répartis sur **General** (moniteur, lien crédit upstream)
et **Draw** (hotkey, fade seconds). Chaque page persiste sur `PSN_APPLY` ;
sauvegarde INI une fois à la fermeture. `ICC_TAB_CLASSES` ajouté à l'init
comctl32. Comportement inchangé.

### Palier 1 — Onglet Text
Combobox police (liste figée Segoe Print / Segoe UI / Ink Free / Consolas)
+ taille par défaut. Chaque `DrawLine` stocke son `fontName` (comme
`fontSize`/`colorIndex`), lu depuis l'INI à l'entrée en mode texte ; les 2
points de rendu (texte + caret) l'utilisent au lieu du `"Segoe Print"` codé
en dur. `ResolveTextFont()` retombe sur Segoe Print si la police stockée
n'est pas installée. Taille initiale = `Text/defaultsize` (défaut 30) au lieu
de `penWidth*5`.

### Palier 2 — Onglet Draw : défauts au lancement
Couleur (index palette 0-9) et épaisseur de départ configurables
(`Draw/defaultcolor`, `Draw/defaultpenwidth`), lues au `WM_CREATE`. On arrête
de persister la dernière épaisseur utilisée → l'app repart toujours des
défauts configurés (même modèle que le rouge par défaut). Valeurs out-of-box
inchangées (couleur 1 = rouge, épaisseur 6).

### Palier 3 — Onglet Colors : palettes éditables
10 pastilles owner-drawn (numérotées 0-9), toggle **Light/Dark**, bouton
**Reset to defaults**. Clic sur une pastille → `ChooseColor` Windows ; édition
en mémoire, écrite dans `[Colors]` de l'INI sur OK. `ApplyTheme` devient
data-driven : lit chaque slot depuis l'INI, fallback sur les constantes
`DEFAULT_COLORS_LIGHT/DARK` (qui servent aussi au reset). Les nouvelles
couleurs s'appliquent à la prochaine entrée en mode draw.

### Palier 4 — Onglet Screenshot
Auto-capture désactivable (`Screenshot/enabled`), dossier racine configurable
(`Screenshot/folder`, vide = `Pictures\DemoInk`, bouton Browse via
`SHBrowseForFolder`), détection Meet optionnelle (`Screenshot/meetdetect` ;
off → captures rangées par date seulement). `SaveScreenshot` honore les trois
réglages. Sous-dossiers renommés en **anglais** : `By client` / `By date`
(les anciennes captures FR de Kevin ne sont pas migrées).

## Réalisé : Molette = taille + réglages sur toile vide (`feature/wheel-resizes`)

- **Molette = taille partout** : en mode draw la molette (nue ou Ctrl) change
  l'épaisseur du pinceau au lieu de cycler les couleurs ; en mode texte elle
  ajuste toujours la police. Le cycle couleur reste sur `←/→` et `0-9`
  (`WM_MOUSEWHEEL`).
- **Réglable sur toile vide** : retrait du garde `!m_drawLines.empty()` sur
  `ID_CMD_INCREASE/DECREASE` et `NEXT/PREVCOLOR` → l'épaisseur et la couleur
  se règlent avant le premier trait.

## Réalisé : Onglet Shortcuts + défaut opaque (`feature/shortcuts-tab`)

Rebind des 5 lettres d'action du mode draw, défaut alpha `T`→`X`, et option
d'opacité au lancement. Disposition clavier (AZERTY/QWERTY) **hors scope** :
on réassigne juste action↔lettre.

### Table d'accélérateurs dynamique
La table passe de **statique** (`LoadAccelerators` sur la ressource
`ACCELERATORS` du `.rc`) à **construite au runtime** depuis l'INI. La
ressource `IDR_DEMOHELPER ACCELERATORS` est retirée du `.rc`.
`BuildAcceleratorTable()` (`MainWindow.cpp`) assemble un `std::vector<ACCEL>`
puis `CreateAcceleratorTable`. La `HACCEL` est détenue par la fenêtre
(`m_hAccel`), créée au `WM_CREATE` (`RebuildAcceleratorTable`), détruite au
`WM_DESTROY`, et **reconstruite à la fermeture des Options** (`Commands.cpp`)
pour prendre en compte un rebind sans redémarrer. La boucle de messages
(`DemoHelper.cpp`) lit `trayWindow.AcceleratorTable()`.

### Touches rebindables vs fixes
Rebindables (table `SHORTCUTS[]` dans `MainWindow.h` : cmd ↔ clé INI ↔ lettre
défaut ↔ contrôle edit) :

| Action | Défaut | Clé INI `[Shortcuts]` |
|--------|--------|-----------------------|
| Text mode | `A` | `key_textmode` |
| Erase all | `W` | `key_clearlines` |
| Solid background | `Q` | `key_toggletheme` |
| Board frame | `Z` | `key_cycleboard` |
| Opaque / see-through | `X` (était `T`) | `key_togglerop` |

Fixes (non rebindables, en dur dans `BuildAcceleratorTable`) : `0-9`, `↑↓`,
`←→`, `Esc`, `Backspace`, `Delete`, `F1`, `Alt+/`. Lettres stockées en
majuscule ; `'A'-'Z' == VK_A-VK_Z` donc utilisées telles quelles comme
`ACCEL.key` avec `FVIRTKEY`.

### Onglet Shortcuts (UI)
6e property page (`IDD_OPT_SHORTCUTS`, `ShortcutsPageProc`). 5 edits d'1 car
(`ES_UPPERCASE`, `EM_SETLIMITTEXT 1`) + bouton **Reset to defaults**.
Validation sur `PSN_APPLY` : refuse vide / non-lettre / doublon
(`MessageBox` + `PSNRET_INVALID_NOCHANGEPAGE`, reste sur la page). Écrit dans
`[Shortcuts]` sur OK. `ResolveShortcutLetter()` retombe sur le défaut si la
valeur INI est absente/invalide.

### Option « Start opaque » (onglet Draw)
Case **« Start with opaque strokes (not see-through) »**
(`IDC_DEFAULTOPAQUE`, `[Draw] startopaque`). Dans `ApplyTheme`, l'alpha de
départ devient `(dark || startopaque) ? 255 : LINE_ALPHA` — le Dark reste
toujours opaque, le Light/Transparent devient opaque si l'option est cochée.

## Réalisé : Onglet Background — fond uni configurable + image de board (`feature/background-custom`)

Nouvel onglet **Background** (7e page du PropertySheet, `IDD_OPT_BACKGROUND`,
`BackgroundPageProc`).

### Fond uni (touche Q) configurable
Les couleurs des thèmes clair et sombre (blanc / noir en dur jusqu'ici)
deviennent éditables : 2 pastilles owner-drawn + `ChooseColor` + bouton Reset.
Stockées dans `[Background] light`/`dark`, défauts `DEFAULT_BG_LIGHT` (blanc) /
`DEFAULT_BG_DARK` (noir). `PaintThemeBackground` lit `BackgroundColor(bool dark)`
au lieu des `RGB(255,255,255)`/`RGB(0,0,0)` codés en dur.

### Image de board (touche Z) — état intermédiaire (remplacé, voir pivot ci-dessous)
Première version : `BoardStyle::Image` en **3e cran** du cycle Z, image unique
`[Background] image`. Abandonné au profit du pivot ci-dessous.

## Réalisé : Images de board A/B (pivot) (`feature/board-images-ab`)

Pivot demandé : l'image ne doit **pas** être un 3e cran mais **remplacer** le
rendu vectoriel d'un des deux cadres. Le cycle Z reste à **2 crans** (A clair ↔
B sombre) ; chaque cran dessine soit son cadre vectoriel par défaut, soit
l'image fournie pour ce style si elle existe.

- `BoardStyle::Image` retiré ; cycle Z revenu à `None → A → B → A`.
- 2 chemins INI : `[Background] imagelight` (remplace FrameA) et `imagedark`
  (remplace FrameB). Helper `BoardImagePath(bool dark)`.
- `PaintBoardFrame` : lambda locale `tryDrawImage(bool dark)` — si un chemin est
  configuré et l'image se charge (`Gdiplus::Bitmap`), elle est dessinée centrée
  + mise à l'échelle uniforme (letterbox sur la couleur de fond du thème déjà
  peinte dessous) puis `return` ; sinon on tombe sur le dessin vectoriel. Testée
  en tête des cas `FrameA` (dark=false) et `FrameB` (dark=true).
- Onglet Background : 2 lignes image (Light board / Dark board), chacune avec
  Browse (`GetOpenFileName`) + bouton X (clear). Champ vide = cadre vectoriel.

## Idées futures
- **Auto-screenshot à l'Esc** : quand on sort du mode draw, capturer
  l'écran annoté et le sauver dans un dossier dédié. Implémentation
  basique sans MCP : dossier fixe + nom horodaté.
- **Intégration Google Meet** : récupérer le titre de la fenêtre
  Chrome/Meet active pour préfixer les screenshots avec le nom du
  meeting / du client. Probablement via un MCP dédié.
