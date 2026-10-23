# macOS-style Dock for KDE Plasma — Roadmap

Status: **Phases 0–7 built and feedback rounds 1–5 done (2026-10-20).** Public at
https://github.com/MaxJW/harbour. Next: pinned folders in settings, then the animation pass (see the end).

## Target system (verified 2026-10-20)

- CachyOS, kernel 7.2.11, KDE Plasma / KWin **6.8.0**, Qt **6.12.0**, KF 6.31, Wayland session
- NVIDIA RTX 3070, driver 615.78 (open kernel modules)
- DP-1 2560×1440 @ 165 Hz (primary, x=1080) · HDMI-A-1 1080×1920 portrait @ 60 Hz (x=0)
- Existing: macOS-style top panel (Kickoff + global menu + tray + clock) on each screen, no task manager
- KWin effects: Magic Lamp on (300 ms), Better Blur DX (has a refraction pass), rounded corners, Sheet
- Icon theme Papirus-Dark, colour scheme Breeze Dark

## Decisions (agreed with user)

| Topic | Decision |
|---|---|
| Name | **Harbour** — binary `harbour`, app id `dev.harbour.dock` |
| Approach | Build our own: C++20 + Qt Quick/QML, layer-shell-qt, LibTaskManager, KWindowSystem |
| Default look | macOS 27 Liquid Glass at the **medium** setting; opacity slider in settings |
| Icons | The icon theme's own icons, drawn pixel-exact at rest (user rejected squircle tiles, 2026-10-20) |
| Minimise | **Custom C++ KWin genie effect** (Genie / Scale / Suck), separate from the dock |
| v1 extras | Trash + folder stacks (Fan/Grid/List), badges + progress bars |
| Not in v1 | Recent-apps section, Launchpad grid |
| Monitors | One dock that follows the mouse to whichever screen's edge you push against (macOS behaviour) |
| Settings | Plasma System Settings module (KCM) |
| Games | Handle native fullscreen and gamescope; a hidden dock must draw nothing |
| Default visibility | Auto-hide, macOS style (no reserved space) |
| Install | Local PKGBUILD → pacman |
| Left slot | Dolphin pinned permanently, like Finder |

## Architecture notes

- **Window list / activation / launchers:** LibTaskManager (`org.kde.taskmanager` QML module, `TasksModel`).
  KWin 6.8 only withholds restricted globals (`org_kde_plasma_window_management` etc.) from *sandboxed* clients
  (`KWinDisplay::allowInterface` in `wayland_server.cpp`), so the dock runs fine straight from the build dir.
  The .desktop file still lists `X-KDE-Wayland-Interfaces` for older KWins.
- **Genie target:** publish each icon's geometry with `TasksModel::requestPublishDelegateGeometry`; minimise
  effects read it from KWin.
- **Surface:** layer-shell, bottom layer anchored; window tall enough for magnified icons + bounce, with the
  **input region** limited to the visible dock so the transparent area passes clicks through.
- **Auto-hide (verified in Phase 0, from KWin 6.8.0 source + tests):** `kde_screen_edge_v1`, wrapped in `ScreenEdge`.
  - KWin ignores the edge request unless the layer surface is already mapped → attach after the first frame.
  - The edge spans only the surface's width → the surface is full-width with a masked input region.
  - `activate` hides the surface instantly (no effect unless slide data is set); there is no "shown" event.
    Harbour animates its own slide, presents one frame with the dock off-screen, *then* arms the edge, so a reveal
    never shows a stale frame. The reveal is detected from the window being re-exposed (Qt re-renders when KWin
    paints it again; also covers touch swipes) or, as a backup, pointer enter in a 4 px full-width edge strip.
  - KWin blocks client edges under fullscreen windows regardless of `RemainActiveOnFullscreen`
    (`Edge::checkBlocking`), so games can't summon the dock. Edges fire during drag-and-drop but not while a
    mouse button is held otherwise.
  - Reveal latency ≈ 90–100 ms after the push (KWin `ElectricBorderDelay` 75 ms + 1 px push-back, user defaults).
  - ⚠ Protocol docs call it a DE implementation detail that may change without a version bump; without it Harbour
    falls back to a 1 px input strip.
- **Animation clock:** Qt Quick's default animation driver steps one vsync of *Qt's* primary screen per frame (here
  the 60 Hz HDMI screen), which ran every animation 2.75× fast on the 165 Hz screen → `QSG_USE_SIMPLE_ANIMATION_DRIVER=1`
  (wall clock), set in `main()`.
- **Primary screen:** `QGuiApplication::primaryScreen()` doesn't follow Plasma's primary on Wayland → `OutputOrder`
  reads `kde_output_order_v1`. `HARBOUR_SCREEN=<output>` overrides it for development.
- **TasksModel:** `launchInPlace` only works with `separateLaunchers: true` (libtaskmanager only applies it inside that
  branch of `lessThan`), which is what Plasma's icon-only task manager uses.
- **Menus:** QQC2 `Menu` with `popupType: Popup.Window` (real xdg popups, Breeze style via qqc2-desktop-style, so
  the app is a QApplication). On Wayland the popup is anchored to its parent item's rectangle and x/y are ignored,
  so the menu pops up from an invisible anchor item as wide as the menu, centred over the icon.
- **Positioners:** a Row `add` transition interrupted by model churn left icons stuck at scale 0 (blank slots).
  Insertion animations must live in the delegate (Phase 2).
- **Settings:** KConfigXT schema `src/harboursettings.kcfg` → `HarbourSettings` (QML singleton), stored in
  `~/.config/harbourrc`. `SettingsWatcher` re-reads on any write to the file, so edits apply live.
- **Memory (measured):** Harbour ≈ 100 MB private / 131 MB PSS; a trivial EGL demo on the same NVIDIA driver is
  ≈ 38 MB private / 52 MB PSS, so Qt/QML/KF6 cost ≈ 60 MB. User chose to stay on C++/QML. Look for savings in Phase 6.
- **Glass (verified in Phase 3):** KWin 6.7+ dropped `org_kde_kwin_blur` in favour of `ext_background_effect_v1`
  (vendored in `src/protocols/`); `BlurBehind` uses it (falls back to the KDE protocol on older KWin). Better Blur DX
  honours the requested region because scope "dock" makes the surface a Dock window (special window). Its refraction
  is **global** (`RefractionStrength` > 0 in `[Effect-better-blur-dx]`, currently 0) — the user's call, not ours.
  A *moving* blur region can't use BBDX's cache and cost ~1 late frame per 5 slides at 165 Hz, so the region is
  pinned at the resting position while the plate is within 2% of it (the easing makes that the slow ends of each
  slide) and off otherwise, with the tint thickening to cover the switch: 1 late frame in 78 slides, same as no blur.
  ⚠ Hover magnification still moves the region (≈0.4% late frames while sweeping) → Phase 6.
- **Icon sharpness:** at rest each icon is drawn from a texture rendered at exactly its on-screen size (no filtering,
  pixel-aligned); a second, mipmapped texture at the magnified size is shown only while scaled. The user found the
  earlier single mipmapped texture blurry. Squircle tiles were built and then removed at the user's request.
  Light/dark follows the Plasma palette live.
- **Stack popups:** plain QQC2 `Popup` windows ignore x/y on Wayland and land in the wrong place; only `Menu`
  anchors reliably, so stack popups are a `Menu` with a custom `contentItem` (children declared directly on a Menu
  become menu entries and aren't laid out).
- **Performance budget:** zero CPU/GPU at idle (no running animations, no timers); magnification is pure bindings
  on the scene graph; a hidden dock renders nothing (KWin hides the surface).
- **Fullscreen/games:** ⚠ Unverified: KWin's stacking of layer-shell surfaces relative to fullscreen windows, and
  whether a hidden layer surface blocks direct scanout. Test in Phase 6 with a native game and with gamescope.

## Reference: macOS Dock behaviour to match

- macOS 27 "Golden Gate": a single Liquid Glass slider in System Settings > Appearance (clear ↔ medium ↔ opaque,
  default medium); the Dock is its most visible consumer ([9to5Mac, 2026-09-22](https://9to5mac.com/2026/09/22/macos-27-gives-you-more-control-over-liquid-glass/)).
  macOS 27 reduced default transparency and revamped icons with more glass layers ([Wikipedia: Liquid Glass](https://en.wikipedia.org/wiki/Liquid_Glass)).
- Desktop & Dock settings to mirror: size, magnification (on/off + size), position (left/bottom/right),
  minimise effect (Genie/Scale), minimise into app icon, animate opening apps, auto-hide, show indicators for open apps.
  Hidden `defaults` worth exposing: auto-hide delay, auto-hide animation speed, scroll-to-open, translucent icons for
  hidden apps, spacer tiles.
- ⚠ Exact timings (magnification falloff width, bounce height/period, auto-hide delay/duration, genie duration) are
  from memory, not measured. Calibrate against screen recordings of a real Mac before finalising.

## Prior art reviewed

- **latte-dock-ng** (github.com/ruizhi-lab/latte-dock-ng): active Plasma 6 Wayland fork, parabolic zoom; Plasma 6.8
  support still landing (Oct 2026); auto-hide and minimise targeting not documented. Useful reference for the
  parabolic zoom and LibTaskManager usage.
- **Crystal Dock** v2.16 (github.com/dangvd/crystal-dock, GPL-3): Plasma 6 Wayland, zoom, bounce, auto-hide,
  uses kde_screen_edge + plasma_window_management directly; Qt Widgets (CPU painting). Useful reference for the
  auto-hide protocol.
- Plank, Cairo-Dock, nwg-dock: X11 or wlroots-only, not usable on KWin Wayland.

## Feedback round 1 (2026-10-20)

- Magnification no longer slides the dock when the pointer is at the screen edge beyond the dock's ends: it only
  applies with the pointer over the dock, and its centre is clamped to the dock. Verified (far left/right pushes).
- Right-click missed during a bounce: the mouse area bounced with the icon. Now only the picture bounces
  (`DockSlot` `bouncer`). Verified: menu opens on the first right-click mid-bounce.
- Stacks: fan removed; grid (default) or list. The grid uses the dock's glass (`DockBackground`) with compositor
  blur on the popup window (`DockTools.blurBehind`). Verified.
- Settings page shows sizes in px and previews size/magnification/glass live over D-Bus (`DockService`:
  `dev.harbour.dock /Dock preview|endPreview|reveal`); Reset/close reverts. Preview verified over D-Bus; not yet
  driven from the page itself. The D-Bus name also makes Harbour single-instance.
- "Sizes not copying to the other screen": not reproduced — both docks resize (geometry log + screenshots). The
  portrait screen still has a Plasma panel at the bottom, which is the likely cause. Ask the user if it persists.

## Feedback round 2 (2026-10-20)

- List view crashed (`popup` inside a Menu is the menu's own popup() method) — fixed; the list is now a compact KDE
  context menu (12 newest, folders as lazily-filled submenus via `FolderMenu`/`StackMenus`, Open in Dolphin).
  Verified on screen. The grid uses the unstyled `T.Menu` (no desktop-style warnings).
- Clicking a stack (or an icon with its menu open) again closes it instead of reopening (`Dock.shouldOpenMenu`,
  `noteMenuClosed`). Verified for grid and list (user-run test).
- Default pins now match the user's old Plasma docks (+ Spotify), Dolphin first.
- Replacing the Plasma dock: config backed up to `~/.config/harbour-backup-2026-10-20/` (appletsrc, plasmashellrc,
  kwinrc); bottom panels 50 (screen 0) and 77 (screen 1) removed via PlasmaShell scripting; KWin switched to
  harbour_genie (Magic Lamp off). `harbour.service` (systemd user unit, WantedBy=plasma-workspace.target,
  Restart=on-failure) ships in the package; enable with `systemctl --user enable --now harbour.service`.
  To undo the panels: restore the backed-up appletsrc/plasmashellrc and restart plasmashell.

## Feedback round 3 (2026-10-20)

- `harbour.service` enabled (`systemctl --user enable --now`); Harbour starts with Plasma.
- Blue bar at the bottom edge = KWin's "Screen Edge" glow effect (draws on every reserved edge; no per-edge opt-out
  in `screenedgeeffect.cpp`). Disabled with the user's OK: kwinrc `[Plugins] screenedgeEnabled=false`. Edges and
  hot corners still work, without the glow.
- User disliked the dark tint during slides, and chose "glass while sliding": the blur region follows the plate
  again and the tint is constant. Measured via D-Bus reveals (no pointer): 10 of 24 slides had 1–3 late frames
  (12–18 ms vs 6 ms) — the known cost of a moving blur with Better Blur DX at 165 Hz. Alternatives offered and
  declined: clear while sliding, or the old tint trick.

## Feedback round 4 (2026-10-20)

- Crash (2× SIGSEGV in `QQuickWindowPrivate::polishItems`, null vtable → destroyed item still queued for polish):
  menus/stack popups destroyed themselves synchronously on close. Now `destroy(1000)`; dock windows for removed
  screens are hidden and `deleteLater()`ed. Stress test: ~200 menu/fan open/switch/close cycles, no crash.
- Glass "dark band": the RectangularShadow under the translucent plate showed through, plus a sheen gradient
  darker mid-plate. Shadow removed; only a top-edge highlight. Verified over a busy window.
- `ShowWhenClear` (default on): with auto-hide on, the dock stays up while no window overlaps it — per dock, a
  `TasksModel` with `filterByRegion: Intersect` over the dock's area, current desktop/activity, not minimised;
  300 ms settle before showing. Only user-driven reveals (`userRevealed`) change the active dock; a dock that is
  up only because it's clear never reserves space. Verified: empty desktop → shown; maximised window → hides;
  minimised → back.
- Menus: KDE-style (icons; windows listed only when >1, active bold; top-level Pin/Unpin; New Window, Show All
  Windows, Minimise, Quit). Switching menus with one right-click: the press that closes the old menu reaches the
  dock but its release goes to the closing popup, and a menu opened during the press is closed by that release,
  so the new menu opens once `QGuiApplication::mouseButtons()` reports all buttons up (≤600 ms). Verified.
- Stacks: the macOS fan is back and is the default (user meant "can we not…" as a request): 12 newest, arc of
  radius 2600 px curving right with tilt, staggered spring-in, accent-coloured hover, "Open in Dolphin" on top.
  Grid and List remain. User's Downloads set to fan.
- **Testing without the user:** `tests/ui/harness.sh tests/ui/<test>` (build with `-DHARBOUR_BUILD_UI_TESTS=ON`) runs Harbour in an invisible KWin
  (`--virtual`, own private D-Bus with no service activation, own XDG_CONFIG_HOME) and drives it with fake input;
  tests `t-switch.sh`, `t-stress.sh`, `t-clear.sh`, `t-glass.sh`, `t-list.sh`; output in `build/ui-tests/`. Earlier nested runs had leaked
  xdg-desktop-portal-kde processes (activated via the user's systemd); those were killed and the private bus
  config prevents it.

## Feedback round 5 (2026-10-20)

- Dock menus are now native KDE menus (`NativeMenu`, a QMenu wrapper — the same component as Plasma's desktop
  menu), so they match the desktop exactly: app menus, Trash, stack options, stack List view (folders as lazily
  filled submenus, long names shortened in the middle). QQC2 menus and `FolderMenu.qml` are gone.
- Placement: Qt's idea of a layer surface's position is always (0,0) and `setPosition` doesn't stick, so QMenu's
  own on-screen clamping pushed menus down onto the dock. Fixed with QtWayland's private
  `setParentControlGeometry()` (anchor = a rect as wide as the menu, centred over the item) +
  `setExtendedWindowType(Menu)`: the compositor flips it above.
- Switching: a QMenu swallows the outside click entirely, so on close the dock checks `mouseButtons()` and the
  cursor (mapped through the dock, since Qt's "global" coordinates are window-local here) and opens the item
  under it once the button is up. Verified in the invisible test session: switch in one click, same item closes,
  ~200-cycle stress test, Trash/stack/list menus.

## Next (agreed 2026-10-20)

- [x] Git repo, public on GitHub, README with screenshots (taken in the UI harness over Plasma's "Next" wallpaper,
      with a made-up Downloads folder), GPL-3.0 LICENSE.
- [x] UI test harness in the repo (`tests/ui/`), edgeprobe behind `HARBOUR_BUILD_UI_TESTS`.
- [x] pacman hook (`packaging/arch/harbour.hook`): warns to rebuild after kwin / qt6 / layer-shell-qt /
      plasma-workspace upgrades.
- [x] Pinned folders (user request): a folder dropped between the dock's items (or on the divider) becomes a stack
      (`Dock.pinFolders`, `DockTools.stackPath` stores "~/…"); the KCM has a Folders section (add via FolderDialog,
      per-folder view, move left/right, remove). Verified in the harness (`t-folders.sh`): drop pins it, the page
      lists it, move + Apply reorders harbourrc, remove + Apply takes it off the dock live. Drops in the harness use a
      GTK source (`dragsource.py`): a Qt source sends file lists through the FileTransfer portal, which the private
      bus doesn't have. ⚠ Not verified in the real session: a drop from Dolphin there goes through that portal; Qt
      should resolve it back to the real paths for an unsandboxed receiver, but this needs checking.
- [x] Divider stress test (`t-divider.sh`, after the user saw crashes "messing with the divider"): 15 rounds of
      clicks/right-clicks/menus plus drags on and across it, no crash. The user's two crashes (21:51, 21:52) were
      with the 21:47 package and match the deleted-menu crash fixed after it.
- [x] Animation pass (2026-10-21), each checked frame by frame in the harness (`t-anim.sh`, `grab.py` grabs bursts
      through KWin's ScreenShot2 at ~30 ms):
      - Quit / unpin: a ghost shrinks where the icon was (OutCubic, matching the row's move transition, so it fits the
        closing gap). Only unpinned running apps and menu unpins; startups and pinned apps are replaced by a row for
        the same app, and a reappearing app cancels its ghost.
      - Drop settle: the icon glides from where it was let go into its slot. Saving the order resets the tasks model
        and rebuilds every delegate, so the drop point is handed to the new delegate (`Dock.pendingSettle`).
      - Name label: fades in/out (110 ms), slides between icons (140 ms).
      - Size changes animate (240 ms). Rest images render once at the target size; the mipmapped image is used
        meanwhile; the window and exclusive zone go straight to their final size (no per-frame resizes).
      - Fan/grid close: gathers back into the stack (the Popup exit transition). Only closes Harbour starts animate;
        when the compositor dismisses the popup it just goes. Grid blur goes on after opening, off before closing.
      - Attention peek: while Hidden, the active dock disarms the edge, draws only the bouncing icon rising out of the
        screen edge, then re-arms (`AutoHideController::peeking`). Pushing the edge mid-peek reveals. Never over a
        fullscreen window on that screen. Verified: peek → edge re-armed 33 ms later; push after it reveals; push
        mid-peek reveals; fullscreen → no peek; no frames rendered while armed.
- [x] Bug found on the way (predates this pass): changing the icon size made the dock vanish until the next
      reveal: `slide.y` was bound to `hiddenOffset` and the slide animation doesn't remove the binding, so a new size
      snapped a shown dock off-screen. Now set imperatively.
- [x] Harness: the virtual backend has no pointer device, so each probe run plugged one in and out; clients saw
      enter without leave (the dock never auto-hid in tests). `edgeprobe stay` keeps one plugged in for the session.
      Also `HARBOUR_WRAP` (e.g. `env WAYLAND_DEBUG=1`) for the dock process.
- ⚠ Not yet seen on the real session: all of the above was verified in the nested harness only.
- [x] Menus always open above their item (user saw a two-entry menu cover the icon): small menus fit below the
      anchor, so the default positioner opened them downwards. Now Qt's `_q_waylandPopupAnchor/Gravity = TopEdge`
      and slide-only constraint adjustment (`NativeMenu::popupAbove`). Verified with `t-menuplace.sh`.
- [x] Same physical size on every screen (`MatchPhysicalSize`, default on; KCM checkbox). Each dock's `sizeScale` =
      its screen's logical px/mm ÷ the primary's, along the diagonal (Qt doesn't swap a rotated screen's physical
      size), rounded to 0.05, clamped 0.5–2; 1 when a screen reports no size. User's screens (Qt-reported):
      DP-1 4.288 px/mm, HDMI-A-1 3.645 px/mm → 0.85, so 50 px icons become 43 px on the portrait screen.
      ⚠ Only the fallback (scale 1, virtual outputs report no size) runs in the harness; the 0.85 is checked by the
      numbers, not on screen.
- [x] Magnification/label stuck where the pointer entered until a click (user report). Cause: menus open on a
      press; while a QMenu is open Qt hands it the mouse, so the press's release never reaches the dock and the icon's
      MouseArea keeps the pointer grab: hover (magnification, label) stops updating. `NativeMenu` now drops that
      grab when the menu closes (`releaseDockPointer`). `t-hover.sh` fails without the fix ("after-escape: STUCK"),
      passes with it.
- [x] Name label felt laggy: slide between icons 140 ms → 70 ms, fade 110 → 90 ms.
- [x] Fan labels looked pixelated when tilted (user screenshot, NVIDIA). Not reproducible in the harness (smooth
      there either way). Fan text now uses `Text.CurveRendering` (glyph outlines, no cached bitmaps/distance fields)
      and tilted icons `antialiasing: true`. ⚠ Needs the user's eyes on the real session.
- [x] Round 9 (2026-10-21): magnify under files dragged over the dock (DropArea feeds `pointerX`; reaches up to
      magnified icon tops); generic default launchers (Dolphin, Firefox, Konsole, Discover, System Settings), with pinned apps that aren't
      installed hidden but kept in the settings; settings page regrouped (Look / Behaviour / Folders and Trash /
      Advanced timing folded away, shortcut and density notes as help buttons, no slider tick crowds). Harness gets its
      own XDG_DATA_HOME so Trash drops never reach the user's Trash. Tests: t-dragtrash. Tagged v0.1.0.
- [x] Divider drag-to-resize was in v0.1.0, then removed at the user's request (size stays in settings): hovering
      it grew the window, and on the user's NVIDIA setup a window resize shows stretched frames for a moment (the new
      size reaches KWin before a picture to match). Not reproducible in the harness. Settings size changes still
      resize the window once per change.
- [x] ScreenMode "cursor" ("Only the screen with the pointer"): a KWin script (`src/cursorscreen.js`, copied to the
      runtime dir and loaded over D-Bus while the mode is on) calls `dev.harbour.Dock.cursorScreen` when the pointer
      changes screen; that dock becomes active and only the active dock stays up for ShowWhenClear. Bounces (launch
      and attention) only on the active dock; a dock becomes active when the pointer enters it. Verified with two
      virtual outputs (`t-screens.sh`, run with 2 outputs): dock follows the pointer left → right → left; a launch
      clicked on one dock bounces only that dock.
- [x] Reveal over fullscreen windows (user's choice: "Edge always works", like macOS fullscreen apps). KWin turns
      client screen edges off under fullscreen windows, so the KWin script (now `src/pointer.js`, always loaded)
      also reports when the pointer is on a screen's bottom row; resting there 150 ms (+ reveal delay) reveals the
      dock if a fullscreen window covers that screen. The dock switches to the overlay layer while shown over it and
      back to top once hidden. No surface is added over the game (an overlay strip would stop direct scanout). In
      fullscreen the dock always auto-hides (even with auto-hide off) and never peeks. Verified (`t-fullscreen.sh`):
      hidden with the pointer away; up over the window after resting on the bottom row; hides again; same with
      auto-hide off; back up when fullscreen ends. ⚠ Games that confine the pointer and scroll at the screen edge
      (RTS) will bring the dock up when scrolling down; not seen on the real session yet.
- [x] Magnification holds while a menu or stack is open (user request) and resumes after, gliding from the held
      point to the pointer (150 ms). Menus call `Dock.holdMagnification()` before placing themselves, and anchor to
      the icon as drawn (`DockSlot.shownItem`), so they sit above the enlarged icon. Right-clicking straight to another
      item moves the hold to it (the last pointer position Qt saw; hover stops while a menu is open); the hold is only
      let go when no menu is about to open (`noteMenuClosed` now runs before `menuOpen` drops). `t-menuhold.sh`.
- [x] Even out square app icons (`EvenTiles`, default on; KCM checkbox under Behaviour → Icons). User (Papirus-Dark): Helium,
      paintnotnet and Zed are all rounded squares at different sizes and radii. The provider looks at an app icon at
      twice the size; if its opaque part is square, fills ≥ 90% of that square (a circle fills 79%) and has four flat
      sides, the square is redrawn at 82% of the slot with 22.5% corners (macOS's grid is 80%; 82% reads as the same
      size as Papirus's 87.5% circles). Clipping only takes off corners sharper than that. Only DockIcon asks for it
      (`image://harbour/tile/...`); stacks and the Trash don't. Verified in the harness on the user's dock at 64 px:
      Helium 62 → 52 px, paintnotnet 58 → 52, Zed 56 → 52; an off/on pixel diff changes nothing else.
      t-folders' click points moved with the settings page (row 2 up 843 652, row 1 remove 918 614).
- Zed's icon: the user's own override `~/.local/share/applications/dev.zed.Zed.desktop` (`Icon=zed_original`, a
  copy of the package's `hicolor/512x512/apps/zed.png` in `~/.local/share/icons/hicolor/512x512/apps/`). The name
  has no dash: KDE's icon lookup falls back from `zed-original` to Papirus's `zed` before reaching hicolor.
- [x] Optimisation pass (user: "142 MB seems like a LOT of memory for just a dock"). Measured on the user's session
      first: 142 MB PSS (105 MB private) with two docks; idle CPU ~0, no wakeups while idle. In the harness (which
      renders on the same RTX 3070) one dock was 71 MB at start, +40 MB after opening one stack fan, and 125–208 MB
      after repeated use. Causes and fixes:
  - A GPU context per Qt Quick window (each dock, each stack popup) costs ~20 MB of heap + 4 MB of `nvidiactl` on
    NVIDIA, and isn't given back when a popup closes. Now Qt Quick's **software renderer** by default
    (`QQuickWindow::setGraphicsApi(Software)`; `QT_QUICK_BACKEND`/`QSG_RHI_BACKEND` still pick a GPU). No shaders
    are used, so nothing changes visually (compared screenshots: magnified dock, fan with tilted text). Frames:
    0.1 ms mean, 2 ms worst, 16–17 ms apart during animations on the 60 Hz virtual output; dock CPU over a hover
    sweep 170 ms vs 250 ms with GL; reveal slide 19 frames in 301 ms, none late. Harness, two screens like the
    user's: **31 MB at start, 42 MB after heavy use** (stable), against 85 → 132 MB with OpenGL.
    ⚠ 165 Hz smoothness on the real DP-1 not yet seen (harness output is 60 Hz).
  - Stack fan/grid loaded image files themselves, full size, in the dock (decoders, threads, freed memory kept).
    Now previews come from KDE's thumbnailers (`PreviewProvider`, `image://harbourpreview/…`, KIO::PreviewJob with
    the user's preview settings, cached in ~/.cache/thumbnails), so PDFs and videos get previews too; files without
    one show their type's icon.
  - KIO ran its workers as threads in the dock, loading every thumbnailer and codec (HEIF, x265, OpenEXR, PDF) into
    it. `KIO_ENABLE_WORKER_THREADS=0` unless set: workers are separate, short-lived processes again.
  - Leak: every frame's timestamp went into a list only cleared when a slide started, so a dock that stays up
    (auto-hide off, ShowWhenClear) grew without bound. Frames are now timed only during slides, and only with
    `harbour.frames` logging on. `harbour.autohide`/`harbour.frames` now log warnings only by default (they wrote
    ~470 journal lines a day); the harness turns them on.
  - Leak: nameless window icons (games, Wine) were cached forever, one per window: now a 64-entry `QCache`.
  - The KWin script ran JS inside KWin on every pointer movement. It now follows the pointer only in ScreenMode
    "cursor" or while some window is fullscreen (Harbour rewrites the script's `followPointer` flag and reloads it
    when the mode changes). Each variant has its own script name: KWin refuses a load under a name it's still
    unloading. The file is removed before writing: older versions left it read-only, which QSaveFile won't
    replace (caught by t-fullscreen/t-screens). Checked by counting the script's D-Bus calls while the pointer
    wanders over two screens: 0 normally, 6 with a fullscreen window, 0 after it closes, 6 after switching to
    ScreenMode "cursor" at run time, 0 after switching back.
  - Full UI suite re-run on the software renderer: all pass (t-folders' click points updated earlier).
  - The test-only geometry log no longer runs unless `harbour.geometry` is enabled; dead `folderTotal()` removed.
  - Looked at and left: listing a stack is synchronous (77 entries here; ~10–20 ms for 5000); 1.4 MB installed;
    the systemd unit already restarts on failure only.
- [x] Dark glass rim slightly softer (user): rim alpha 0.16 → 0.12, top highlight 0.32 → 0.26.
- [x] Real session after the optimisation pass (user installed it, 3 min up): 49 MB PSS / 37 MB private (was
      142 / 105), no NVIDIA mappings, 6 threads (was 14); idle 0 CPU and ~1 wakeup/s.
- [x] Dock hid after closing a stack with a second click while the pointer rested on it (user, over a fullscreen
      game; also happens over a maximised window). KWin dismisses the popup and sends the dock a leave, and no enter
      follows until the pointer moves; Wayland can't ask where the pointer is. Now when the pointer is taken by a
      menu/stack (a leave while held, or within 300 ms after the hold ends) the dock asks KWin: a one-line script
      (`src/cursor.js`, loaded under a fresh name, run, unloaded 2 s later since KWin reads the file on a thread)
      calls `dev.harbour.Dock.cursorAt(x, y)`; if that's inside the dock's input region it counts as inside. The hide
      timer waits at least 250 ms for the answer (it came in ~1 ms). Verified (scratch test): fullscreen and
      maximised, pointer still → stays up; then moving away → hides; pointer up over the fan when it closes → hides.
- [x] Name label cut off at the top of a launch bounce (user). The window allowed for the larger of "magnified icon"
      and "icon + bounce", but a just-launched app is usually under the pointer: magnified *and* bouncing, with its
      label above. Window height now magnified size + bounce + label space. Verified with a slow-starting test app:
      label clipped at the bounce peak on the old height, whole on the new.
- [x] Name label swung right and back when launching a pinned app (user). Logged in the harness (System Settings
      launch): the model makes a new delegate for the starting app while the launcher's still there; the label
      retargeted to it before the Row had laid it out (shownLeft 582 instead of 1102), so its slide started from the
      wrong place and it jumped to x=1344 before sliding back to 1084. Now a new delegate for the same app (same
      label) is adopted without a slide. And when the old delegate goes before the new one is hovered (~13 ms when
      the starting app gets its window) the label keeps its last position and text instead of vanishing for a frame.
      Verified by logging the label's x: 824 → 1084 within one update (only 1084 drawn), visible throughout.
- [x] Right-click, then right-click again without moving: magnification lost (user). Same root as the fan: the
      menu's dismissal leaves Qt Quick believing the pointer is gone. When KWin's answer (cursorAt) puts the pointer
      over the dock, DockWindow now also sends Qt Quick the enter and move Wayland didn't, so hover, magnification
      and the label come back. Verified (scratch test): still magnified with its label after the second right-click;
      follows the pointer to Zed; at rest once it leaves.
- [x] Memory: the stack grid's "Open in Dolphin" was the dock's only Qt Quick Controls item, and loading it pulled in
      the org.kde.desktop style and Kirigami the first time a stack opened (+4 MB). Now drawn as a plain pill; the
      Controls import, `QQuickStyle` and the QuickControls2 link are gone. Harness, two screens: 40 → 35 MB after
      use (start unchanged, ~30 MB). Looked at and not worth it: malloc_trim (< 0.5 MB), a smaller QPixmapCache
      (no change), per-dock window lists (the second dock costs ~1.4 MB in all). Allocator stats: ~14 MB of heap in
      use at start, 18.7 MB after use; the rest of the ~30 MB is Qt/KF6 libraries' own data. The remaining large
      piece is the QApplication/QStyle stack that native menus need.
- [x] Code tidy before v0.1.1: no unused QML-exposed methods/properties, QML functions/properties or files (checked by
      script); no debug output or TODOs left. The duplicated "load a KWin script, then run it" code is one helper
      (`runScript`, `writeScript` in dockmanager.cpp); an unused include dropped. Tagged **v0.1.1** (0.1.1 in
      CMake and the PKGBUILD).
- [x] AUR: `packaging/aur/harbour-git/` (PKGBUILD + .SRCINFO) builds the latest commit from GitHub; to publish,
      copy both into the AUR repo (ssh://aur@aur.archlinux.org/harbour-git.git) and push (needs the user's AUR
      account). Test-built with makepkg from GitHub in an empty directory: same 8 files as the local package.
      ⚠ Not built in a clean chroot and not checked with namcap (devtools/namcap not installed). Dependencies
      audited from the binaries' NEEDED entries and the QML imports: qqc2-desktop-style dropped (the dock no
      longer uses Controls; the settings page gets its style from System Settings), wayland added (linked
      directly). The rebuild hook's message now covers AUR installs too.
- [x] Fan still pixelated (user screenshot): tilted previews had jagged edges, and in "IMG_3169.PNG" the M sat a
      pixel low. Reproduced in the harness: since the switch to the software renderer, `CurveRendering` and
      `antialiasing` do nothing there; QPainter rasterises each tilted glyph and snaps it to whole pixels on its own.
      Each fan row's pill and icon are now drawn upright into a layer (`layer.smooth`, a clear 4 px margin) that the
      fan tilts as one picture: an even baseline, soft edges. Text is NativeRendering again (upright in the layer).
      The thumbnails themselves were fine (KIO's downscale matched a Lanczos one); fan previews now ask for
      device pixels like the dock's icons. A 2× layer looked no better at four times the memory. Hover checked on
      label and icon; t-folders, t-anim, t-stress pass. Memory (harness, PSS, 13-row fan): +1.2 MB against the
      old fan both while open (35.4 vs 34.2 MB) and after closing (33.7 vs 32.4 MB); flat over 20 open/close
      cycles (33.2 → 33.6 → 33.7 → 33.7 MB at 1/5/10/20), so no leak.
- [x] App icon for the README (user): `docs/images/icon.svg`, a glass dock magnified around an anchor, floating on
      a dusk harbour, at the top of the README. Not installed as the app/KCM icon (would need CMake + PKGBUILD
      changes; offered, not asked for).
- [x] Pressed icons darken (user, 2026-10-22), like macOS: while the mouse is held on an item (not while dragging
      it) and while its menu or stack is open. `MultiEffect` draws nothing under the software renderer, so the
      icon provider serves a darker copy (`image://harbour/pressed/...`, 30% black over the icon's own pixels, by
      eye) and `DockSlot` swaps to it. Verified in the harness: Steam's icon ×0.70 brightness with its menu open,
      back after Escape; the Trash dark for the length of a click; switching menus moves the darkening.
      t-hover, t-menuhold, t-switch, t-anim pass. ⚠ Not seen: a stack while its popup is open (same code path).
- [x] Tagged **v0.1.2** (2026-10-22; 0.1.2 in CMake and the PKGBUILD): pressed icons, smooth fan rows, tighter
      package dependencies, the AUR PKGBUILD.
- [x] Steam names for games without a desktop file (user, 2026-10-22: Hearthstone through Battle.net, a non-Steam
      shortcut under Proton, showed as "steam_app_3964911085"). Plasma gives such windows AppId `steam_app_<id>`
      and no AppName (checked on the user's session). `DockTools::steamAppName` looks the id up in each Steam
      install's `userdata/*/config/shortcuts.vdf` (binary KeyValues) for non-Steam shortcuts, then in
      `appmanifest_<id>.acf` across `libraryfolders.vdf` for installed games; read each time an icon is made, so a
      rename in Steam shows next time. Verified in the harness with stand-in windows carrying the app ids: labels
      "Battle.net" (shortcut) and "Dishonored" (steam_app_205100, appmanifest). The shortcut's name is
      "Battle.net", so Hearthstone (same prefix, same id) is labelled that too; renaming the shortcut in Steam
      changes it. Icon unchanged (the window's own).
      Follow-up: the user still saw "steam_app_…" once Hearthstone joined the group. Cause: `makepkg -si` found
      the existing harbour-0.1.2-1 package and installed that (built 11:07, before this change), so the old label
      (window title, then the group's raw app id) was running. A watcher on the real session confirmed the group
      parent keeps AppId `steam_app_3964911085`, so no change to the lookup was needed (a speculative hardening
      commit was reverted). Seen working on the real session (user: "now it says Battle.net").
- [x] Launcher + game under one Steam id named after the game (user's pick of the options offered, 2026-10-22):
      when a Steam-named icon has several windows, the newest one's title labels it (children of a group are in
      the order the windows appeared, checked in the harness). Battle.net alone → "Battle.net"; press Play →
      "Hearthstone"; game closed → "Battle.net". (First version used the group's last window: see below.) Verified in the harness with XWayland stand-ins (hover-label
      screenshots at each step); t-hover passes. ⚠ If the launcher closes when the game starts, the lone game
      window shows the Steam name ("Battle.net"): with one window the Steam name wins. Icon unchanged.
      Follow-up (user, real session, new package): still "Battle.net" with Hearthstone up, windows in the right
      order. The label was worked out when the game's window joined, before it had its title; a child's title
      change doesn't touch the group's delegate. DockIcon now listens to the model's dataChanged for its own
      children (only for Steam-named icons with several windows) and recomputes. Reproduced and checked in the
      harness with an X11 stand-in that retitles itself 1 s after mapping: previous commit "Unity" (stuck),
      now "Hearthstone"; the newest-window sequence and t-hover unchanged.
      Still "Battle.net" on the real session. A debug build swapped in for the user's dock logged the live launch:
      when Hearthstone's window arrives, Plasma forms the group as [Hearthstone, Battle.net] (new window first),
      while a dock started with both open sees [Battle.net, Hearthstone] — the group's order says nothing about
      age. Now the window whose process started last (`DockTools::processStartTime`, /proc/<pid>/stat) names the
      icon: the game a launcher starts is always the newer process, and a later launcher window ("Battle.net
      Login", same process) no longer takes over. AppPid checked on the real session (Hearthstone.exe 14:49,
      Battle.net 14:43). Seen working on the real session (user: "It worked!"), build swapped in for the dock.
- [x] Sharp icons for Wine programs (user: Hearthstone's icon "a bit low quality"; picked "automatic from the
      .exe" over a hand-installed theme icon). Hearthstone's window offers only a 32 px icon; its .exe holds
      256 px. `DockTools::wineIcon`: when a window's icon is under 128 px and its process is Wine (process named
      after a Windows path, "C:\…\Game.exe"), the .exe is found in /proc/<pid>/maps (through /proc/<pid>/root if
      only the container has it) and read by KDE's `windowsexethumbnail` in the thumbnail worker at 256 px,
      cached by path; used if larger than the window's icon, else the window's own stays. `wineIconRevision`
      re-runs bindings once read. The icon follows the same window as the label (`shownWindow`: the newest
      process's for a Steam launcher + game), and re-reads on child icon changes as well as titles.
      Verified in the harness with a fake Wine process (argv[0] a Windows path, the real Hearthstone.exe
      mmapped, X11 window with a 32 px icon): Hearthstone's 256 px icon, sharp magnified; a non-Wine window with
      a 32 px icon keeps it; launcher+game group: Battle.net icon → Hearthstone icon and name → back. t-hover
      passes; t-menuhold/t-switch screenshots match the previous commit (t-switch's second click lands on
      Discord under magnification, before and after). ⚠ Not yet seen on the real session.
- [x] Tagged **v0.1.3** (2026-10-22; 0.1.3 in CMake and the PKGBUILD): Steam names for games without a desktop
      file, a launcher's icon named and pictured after the game it started, sharp icons for Wine programs.
- [x] Icon theme changes apply live (user, 2026-10-23: a new icon pack only showed after
      `systemctl --user restart harbour`). Qt Quick caches images by URL and `image://harbour/theme/<name>` stays
      the same across themes. `DockTools` listens for `org.kde.KIconLoader.iconChanged` on the session bus (sent
      once per icon group by System Settings / `plasma-changeicons`), debounced 100 ms, and bumps
      `iconThemeRevision`; `iconUrl()` takes it and appends `?<revision>` (ignored by `IconProvider`), so every
      dock icon, the Trash, stacks and stack popups are requested again from the new theme. Menus already built
      their icons fresh. Memory (harness, RssAnon, 24 switches Papirus ↔ breeze-dark): live heap +1.7 MB then flat
      (Qt Quick's ≤2 MB cache of unreferenced images); glibc kept a further ~3 MB freed, so `malloc_trim` runs 1 s
      after each redraw: 20.5 → 23.2 MB after the 24 switches and 40 s idle, levelling off (26.3 MB without the trim).
      No timers or work at idle. Verified in the harness with `plasma-changeicons`: Papirus → Breeze Dark →
      Papirus → Breeze → Papirus-Dark screenshots each in the new theme; pressed (darkened) icon and stack popup
      after a change; t-hover, t-anim, t-menuhold pass. ⚠ Not yet seen on the real session.
      Follow-up (user, real session): Equibop showed its own logo instead of the `Icon=discord` the user set in
      `~/.local/share/applications/equibop.desktop`, after a switch through a theme without "discord". The tasks
      model works an app's icon out once per window; asked again (the dock now re-reads it on a theme change)
      under a theme without it, it falls back to the window's own icon and keeps that through later changes.
      Old builds never asked again, so never showed it. `DockTools::appIcon` uses the desktop file's icon when the
      model gives a nameless (window) icon and the theme has the desktop file's; anything else is unchanged.
      Reproduced and checked in the harness with a stand-in window (app id "equibop", Equibop's PNG as window
      icon): Papirus → Breeze Dark → Papirus showed Discord → grey logo → grey logo before, Discord → Equibop's
      logo (Breeze has no Discord) → Discord now; without a window icon unchanged; Papirus ↔ Papirus-Dark never
      changed it. t-hover, t-menuhold, t-anim pass.
- [x] Tagged **v0.1.4** (2026-10-23; 0.1.4 in CMake and the PKGBUILD): icon theme changes apply without
      restarting the dock.
- [x] README screenshots retaken (2026-10-23) so they match the current dock: evened-out square icons, the
      user's current launchers, the dock staying magnified and the pressed icon darkened while a stack or menu is
      open, and type icons in place of image previews. Same recipe as the originals: the harness over the "Next"
      wallpaper (a frameless QML window, hidden from the dock with a KWin script), a made-up Downloads folder set
      as the only fan stack, then rest → magnify Obsidian → open the fan → right-click Steam, cropped as before.
- HDT autostart (user asked to stop it): it came from `~/Games/HDT/hdt-autostart.sh` in the Battle.net
  shortcut's Steam launch options; the user removed it there themselves. Outside the repo.
- Spotify's label: the user's own override `~/.local/share/applications/spotify-launcher.desktop` (Name=Spotify),
  outside the repo; the system file belongs to the spotify-launcher package.

## Notes for the user

- **Install:** `cd packaging/arch && makepkg -sif` (needs sudo), then `systemctl --user restart harbour`. Keep
  the `-f` while pkgver/pkgrel are unchanged: without it makepkg installs the package already built for that version.
- **Minimise animation:** after installing, pick Genie in System Settings → Workspace → Dock; that switches KWin from
  Magic Lamp to Harbour's effect. Until then Magic Lamp keeps aiming at Harbour's icons.
- **Commit:** nothing is committed yet — say if you want the work committed.

- **Optional:** Better Blur DX refraction (real Liquid Glass edge distortion) is a global KWin setting
  (`RefractionStrength`); enabling it affects every blurred window.

- Both screens already have a Plasma panel at the bottom (icon task manager). It overlaps Harbour; remove it (or move
  it) once Harbour is in use. Harbour never touches Plasma's config.

## Phases

- [x] **0 — Scaffold + auto-hide spike:** CMake project, PKGBUILD (`makepkg` builds), .desktop, full-width layer-shell
      window on Plasma's primary screen, masked input region, auto-hide state machine on the KWin screen edge.
      Verified by hand (user, all five scenarios) and with an automated fake-input probe: reveal 300 ms / 51 frames,
      hide 250 ms / 43 frames at 165 Hz with 0 late frames, mid-hide reversal, far-left edge, rapid repeats, zero
      frames rendered while hidden. Timings are still placeholders (see the reference section).
- [x] **1 — Core dock:** TasksModel (pinned + running, grouped by app, windows sorted into their launcher's slot),
      icons via `IconProvider` (rasterised at magnified size, mipmapped), indicator dots, name label, macOS-style
      menu (windows, desktop actions, New Window/Open, Options ▸ Keep in Dock, Show All Windows, Hide, Quit), drag to
      reorder (saved), drag up off the dock to remove, `.desktop` drop to pin, file drop onto an app, permanent
      Dolphin slot, minimise-target geometry published, auto-hide off mode reserves space.
      Verified with fake-input + screenshots: icon order/dots, label, menu (opens centred above the icon), reorder
      saved and restored, removal, live settings reload, hide/show unchanged.
      ⚠ Not yet verified by hand: click to launch/activate (bring all windows forward), middle-click, dropping files,
      running desktop actions, Show All Windows/Hide/Quit, genie aiming at icons. Poof animation → Phase 2.
- [x] **2 — Motion:** magnification (raised-cosine falloff integrated into a layout that keeps the point under the
      pointer fixed and grows the plate both ways; purely visual transforms over the resting Row), launch bounce
      (time-based per app, survives Plasma's launcher→startup→window delegate swaps, lands the current hop when
      the window appears, 15 s timeout), attention bounce (3 hops), pop-in for apps new to the dock, row glides when
      icons come and go, poof on drag-off, Meta+Alt+D toggles auto-hide (KGlobalAccel).
      Verified: magnification numbers + screenshots, launch bounce lifecycle (Dolphin: start on click, window at
      171 ms, lands at 651 ms), menu Quit, reorder/removal with transforms, shortcut toggles, hide/show unchanged,
      0 CPU ticks over 10 s idle.
      ⚠ Not verified: attention bounce (needs an app demanding attention), poof and pop-in visuals (too fast for
      screenshots). Bounce height/period and magnification range are first guesses.
- [x] **3 — Visuals:** compositor blur behind the plate (`ext_background_effect_v1`), Liquid-Glass-style plate (tint
      following `GlassOpacity`, specular rim, sheen, shadow), light/dark from the colour scheme. Verified: blur on
      screen, both themes, hide/show pacing. ⚠ Glass tint is by eye.
- [x] **4 — Genie KWin effect (C++):** `kwin-effect/` → `harbour_genie.so` (KWin `OffscreenEffect`, exclusive group
      "minimize" so it and Magic Lamp are mutually exclusive in System Settings). Genie = two overlapping stages
      (S-curve funnel from the window's sides to the icon + stretch of the lower window towards the dock, then slide
      and exponential squeeze into the icon, fade over the last 15%); Scale; Suck (narrow neck, stages concurrent).
      Works for docks on any edge (canonical-frame transform), aims past the screen edge when the dock is hidden,
      Shift = 5× slow motion. Config: kwinrc `[Effect-harbour_genie]` `Style`, `Duration`.
      Verified in a nested KWin (own D-Bus session + config, plugin loaded from the build dir; script
      `scratchpad/nested/run.sh`): frame sequences for all three styles into Harbour's Dolphin icon.
      ⚠ Not yet installed/enabled on the real session (needs `pacman -U` + enabling it instead of Magic Lamp — user's
      call). Durations (Genie 480 ms, Scale 280 ms) are guesses. Must be rebuilt after every KWin update.
- [x] **5 — Extras:** QML refactored around `DockSlot` (shared magnification/bounce/pop-in/dot/mouse) with `DockIcon`,
      `StackIcon`, `TrashIcon`, and a `DockMenu` base (centred popup-window menus) used by `TaskMenu`, stacks, Trash.
      `DockSeparator` before stacks/Trash. `Trash` singleton (watches ~/.local/share/Trash/files; Open; Empty Trash…
      via KIO `DeleteOrTrashJob` with KDE's confirmation; drop → trash). Stacks: `Stacks`/`StackViews` settings
      (default ~/Downloads, auto), `StackPopup` with Fan / Grid / List / Automatic (fan ≤ 12 items), newest first
      (synchronous listing), images shown as themselves, drop → `KIO::move` into the folder. `LauncherEntries`
      singleton (Unity `LauncherEntry.Update`): red count badge, progress bar, `urgent` → attention bounce.
      Verified: separator/stack/Trash render, grid and fan open centred above the stack, Trash menu, badge 7 +
      40% progress from a test D-Bus signal. ⚠ Not verified: drag-and-drop onto Trash/stacks, Empty Trash (destructive).
- [x] **6 — Multi-monitor & games:** `DockManager` keeps a dock per screen (hot-plug aware) sharing one `DockModel`
      (tasks model, launchers, desktop/activity info). The dock last revealed is active: with auto-hide off it is the
      one that stays up (others wait hidden at their edges) and only it publishes minimise targets. `ScreenMode`
      setting: follow (default) / primary. Verified with fake input: both docks created, pushing each screen's edge
      reveals that screen's dock and makes it active; auto-hide off moves the dock to the pushed screen and hides the
      other; a fullscreen window on DP-1 blocks the edge (5 s of pushing, no reveal) and closing it restores it;
      hide/show pacing unchanged (0 late frames); 0 CPU ticks idle; 64 MB private / 93 MB PSS with two docks.
      ⚠ Not tested: gamescope specifically (should behave as any fullscreen window), direct scanout. Hover
      magnification still moves the blur region (rare late frame while sweeping).
- [x] **7 — KCM settings module:** `kcm/` → `kcm_harbour.so` (System Settings → Workspace → Dock), built from the same
      kcfg so saves apply to the running dock live: size, magnification + size, glass, minimise animation (Genie /
      Scale / Suck / KDE's own — writes kwinrc `[Plugins]`/`[Effect-harbour_genie]` and loads/unloads the effects over
      D-Bus; disabled if the effect isn't installed), auto-hide (+ Meta+Alt+D note), animate opening, indicators,
      Trash, click-to-minimise, screens (follow/primary), timing (delays, durations).
      Verified: opens in `kcmshell6` (loaded from the build dir) and shows the live values.
      ⚠ Not re-checked after a fix: "Show the Dock on" combo was blank (index computed before its model); fixed by
      deriving the index from the model array, not yet seen on screen. Apply/Defaults not exercised (Apply would
      have changed the real kwinrc). Package (`makepkg`) builds and ships harbour, harbour_genie.so, kcm_harbour.so.
