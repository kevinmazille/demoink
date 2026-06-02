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

## Idées futures

- **Re-bind des raccourcis autour de Ctrl+Shift** : regrouper les touches
  les plus utilisées près de la main gauche (zone Ctrl+Shift) pour tout
  faire d'une main, et afficher un repère visuel de l'état/mode courant
  quand on commence à taper (savoir où on en est). À concevoir plus tard.
- **Background custom au clear** : remplacer le blanc par défaut
  (`COLOR_WINDOW`) par une couleur configurable. Extension : permettre
  une image de fond.
- **Auto-screenshot à l'Esc** : quand on sort du mode draw, capturer
  l'écran annoté et le sauver dans un dossier dédié. Implémentation
  basique sans MCP : dossier fixe + nom horodaté.
- **Intégration Google Meet** : récupérer le titre de la fenêtre
  Chrome/Meet active pour préfixer les screenshots avec le nom du
  meeting / du client. Probablement via un MCP dédié.
