import QtQuick
import QtQuick.Window
import org.kde.taskmanager as TaskManager
import Harbour

Item {
    id: root

    required property AutoHideController autoHide
    required property DockWindow dockWindow
    required property DockModel shared

    // Follows the Plasma colour scheme, live.
    readonly property bool dark: systemPalette.window.hslLightness < 0.5

    SystemPalette {
        id: systemPalette
    }

    readonly property string permanentLauncher: shared.permanentLauncher

    // Metrics, all derived from the icon size so the dock scales as a whole.
    readonly property real iconSize: HarbourSettings.iconSize
    readonly property real magnifiedIconSize: Math.max(iconSize, HarbourSettings.magnifiedIconSize)
    readonly property real gap: Math.max(4, Math.round(iconSize * 0.1))
    readonly property real padding: Math.round(iconSize * 0.14)
    readonly property real bottomGap: 6
    readonly property real dockHeight: iconSize + padding * 2
    readonly property real labelSpace: 44
    // ⚠ Bounce height and period are first guesses; calibrate against a Mac.
    readonly property real bounceHeight: Math.round(iconSize * 0.6)
    readonly property int bouncePeriod: 640

    // Magnification. maxZoom is the peak scale under the pointer; the falloff spans
    // magnifyRange either side of it.
    readonly property real maxZoom: HarbourSettings.magnification ? magnifiedIconSize / iconSize : 1
    readonly property real magnifyRange: (iconSize + gap) * 2.5
    property real pointerX: 0 // slide coordinates
    // Only over the dock itself. The input region includes a strip along the whole
    // screen edge, and a pointer beyond the dock's ends must not magnify anything:
    // the falloff would shift every icon by the same amount and slide the dock sideways.
    readonly property bool pointerOverDock: hoverHandler.hovered
                                            && pointerX >= tasksRow.x - gap && pointerX <= tasksRow.x + tasksRow.width + gap
    readonly property bool magnifying: pointerOverDock && dragIcon === null && !menuOpen
                                       && autoHide.state !== AutoHideController.Hiding
    property real magnifyAmount: magnifying ? 1 : 0
    Behavior on magnifyAmount {
        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
    }

    // The plate's shown extent, following magnification.
    readonly property real plateLeft: magnify(tasksRow.x) - (padding - gap / 2)
    readonly property real plateRight: magnify(tasksRow.x + tasksRow.width) + (padding - gap / 2)
    readonly property real plateTop: height - dockHeight - bottomGap

    // Read by DockWindow.
    readonly property real windowHeight: bottomGap + padding
        + Math.max(magnifiedIconSize, iconSize + bounceHeight) + labelSpace + 8
    readonly property real reservedHeight: dockHeight + bottomGap
    // Where the pointer is accepted while the dock is shown: the dock plus the gap
    // below it, so moving down to the screen edge never counts as leaving.
    // While magnified, the enlarged icons poking above the plate count too.
    readonly property real inputTop: Math.min(plateTop, plateTop + padding + iconSize * (1 - (1 + (maxZoom - 1) * magnifyAmount)) - 4)
    readonly property rect inputRect: Qt.rect(plateLeft, inputTop, plateRight - plateLeft, height - inputTop)

    // Far enough down that nothing, shadow included, is left inside the window.
    readonly property real hiddenOffset: dockHeight + bottomGap + 32

    readonly property DockSlot hoveredIcon: hoverHandler.hovered ? iconAtShownX(pointerX) : null
    property DockIcon dragIcon: null
    property point dragPoint // in tasksRow coordinates
    readonly property bool dragRemoving: dragIcon !== null && canRemove(dragIcon) && dragPoint.y < -iconSize
    property bool menuOpen: false

    // Clicking an item whose menu or stack is open closes it, as on macOS: the click
    // that dismisses a popup must not open it again straight away.
    property Item lastClosedOwner: null
    property real lastClosedAt: 0

    function noteMenuClosed(owner: Item) {
        lastClosedOwner = owner;
        lastClosedAt = Date.now();

        // A right-click on another dock item closed this menu. The menu swallows that
        // click, so open the other item's menu from here, once the button is up.
        if (DockTools.mouseButtons() & Qt.RightButton) {
            // Qt's global coordinates for our windows are its own, not the screen's, so map
            // back through the dock itself.
            const p = DockTools.cursorPosition();
            const local = root.mapFromGlobal(p.x, p.y);
            const target = local.y >= plateTop - iconSize ? iconAtShownX(local.x) : null;
            if (target && target !== owner && target.contextMenu) {
                openMenuAfterRelease(target);
            }
        }
    }

    // The open menu or stack popup, if any.
    property var currentMenu: null

    // True right after a click closed a menu. A menu opened by that same click would be
    // closed again by its release, and the release goes to the closing menu rather than
    // the dock, so openMenuAfterRelease() watches the button state instead.
    function menuJustClosed(): bool {
        return Date.now() - lastClosedAt < 100;
    }

    property Item menuAfterRelease: null
    property real menuAfterReleaseSince: 0

    function openMenuAfterRelease(item: Item) {
        menuAfterRelease = item;
        menuAfterReleaseSince = Date.now();
        releaseWatch.start();
    }

    Timer {
        id: releaseWatch

        interval: 16
        repeat: true
        onTriggered: {
            // Buttons up, or give up waiting for a release that never registered.
            if (DockTools.mouseButtons() === 0 || Date.now() - root.menuAfterReleaseSince > 600) {
                stop();
                const item = root.menuAfterRelease;
                root.menuAfterRelease = null;
                if (item) {
                    item.contextMenu();
                }
            }
        }
    }

    function shouldOpenMenu(owner: Item): bool {
        if (currentMenu) {
            // The same item again: this click just closes it.
            if (currentMenu.owner === owner) {
                return false;
            }
            // Another item: switch straight to its menu.
            currentMenu.close();
        }
        return !(owner === lastClosedOwner && Date.now() - lastClosedAt < 400);
    }
    readonly property DockSlot dropTarget: dropArea.target
    readonly property Item slideContainer: slide
    readonly property TaskManager.TasksModel tasks: tasksModel
    readonly property Item menuAnchor: menuAnchorItem

    // The model and desktop info are shared by the docks on all screens (DockModel).
    readonly property TaskManager.TasksModel tasksModel: shared.tasksModel
    readonly property TaskManager.VirtualDesktopInfo virtualDesktopInfo: shared.virtualDesktopInfo

    // Where the dock sits when shown, in global coordinates: from the plate's top down to
    // the screen edge, at rest size.
    readonly property rect dockRegion: Qt.rect(dockWindow.globalOrigin.x + tasksRow.x - (padding - gap / 2),
                                               dockWindow.globalOrigin.y + plateTop,
                                               tasksRow.width + 2 * (padding - gap / 2),
                                               height - plateTop)

    // Windows in the dock's way: on this desktop and activity, not minimised, overlapping
    // the dock. None means the dock can stay up (ShowWhenClear); a fullscreen window
    // always counts.
    TaskManager.TasksModel {
        id: overlapModel

        groupMode: TaskManager.TasksModel.GroupDisabled
        filterByVirtualDesktop: true
        virtualDesktop: root.virtualDesktopInfo.currentDesktop
        filterByActivity: true
        activity: root.shared.activityInfo.currentActivity
        filterMinimized: true
        filterHidden: true
        filterByRegion: TaskManager.RegionFilterMode.Intersect
        regionGeometry: root.dockRegion
    }

    Binding {
        target: root.autoHide
        property: "screenClear"
        value: overlapModel.count === 0
    }

    // Maps a resting x (slide coordinates) to where it is drawn while magnified. The
    // zoom falls off as a raised cosine around the pointer; integrating it gives a
    // layout where the point under the pointer stays put and the dock grows both ways.
    function magnify(x: real): real {
        const amount = magnifyAmount;
        if (amount <= 0 || maxZoom <= 1) {
            return x;
        }
        const extra = (maxZoom - 1) * amount;
        // Clamped to the dock, so the fade-out after leaving past an end doesn't slide it.
        const centre = Math.max(tasksRow.x, Math.min(tasksRow.x + tasksRow.width, pointerX));
        const t = Math.max(-1, Math.min(1, (x - centre) / magnifyRange));
        return x + extra * magnifyRange * (t / 2 + Math.sin(Math.PI * t) / (2 * Math.PI));
    }

    function iconAtShownX(x: real): DockSlot {
        for (const item of tasksRow.children) {
            if (item.isDockSlot && item.visible && x >= item.shownLeft && x < item.shownRight) {
                return item;
            }
        }
        return null;
    }

    // The stack paths for a drop, if everything dropped is a local folder.
    function stackPaths(urls: list<url>): list<string> {
        const paths = [];
        for (const url of urls) {
            const path = DockTools.stackPath(url);
            if (path === "") {
                return [];
            }
            paths.push(path);
        }
        return paths;
    }

    // Adds folders as stacks at the end, skipping ones already there.
    function pinFolders(paths: list<string>) {
        const stacks = HarbourSettings.stacks.slice();
        const views = HarbourSettings.stackViews.slice();
        while (views.length < stacks.length) {
            views.push("fan");
        }
        for (const path of paths) {
            if (!stacks.includes(path)) {
                stacks.push(path);
                views.push("fan");
            }
        }
        HarbourSettings.stacks = stacks;
        HarbourSettings.stackViews = views;
        HarbourSettings.save();
    }

    // Identifies an app across Plasma's launcher → starting → running task swaps.
    function appKey(model: var): string {
        const url = model.LauncherUrlWithoutIcon ? model.LauncherUrlWithoutIcon.toString() : "";
        return url !== "" ? url : (model.AppId || "");
    }

    // ── Bouncing ──────────────────────────────────────────────────────────────
    // Each bounce is a pure function of time keyed by app, so it carries on
    // seamlessly when Plasma replaces the icon's delegate mid-flight, and the frame
    // clock only runs while something is bouncing.
    property real now: 0
    property var bounces: ({})
    property int bounceCount: 0

    FrameAnimation {
        running: root.bounceCount > 0
        onTriggered: root.now = Date.now()
    }

    function startBounce(key: string, kind: string) {
        if (key === "" || (kind === "launch" && !HarbourSettings.animateOpening)) {
            return;
        }
        const existing = bounces[key];
        if (existing) {
            existing.stopAt = null;
            existing.kind = kind;
            return;
        }
        now = Date.now();
        bounces[key] = { start: now, kind: kind, stopAt: null };
        bounceCount = Object.keys(bounces).length;
        // Give up on launches that never produce a window.
        if (kind === "launch") {
            bounceTimeout.createObject(root, { key: key }).start();
        }
    }

    // Lets the current bounce land rather than dropping the icon mid-air.
    function stopBounce(key: string) {
        const b = bounces[key];
        if (b && b.stopAt === null) {
            b.stopAt = Date.now();
        }
    }

    function finishBounce(key: string) {
        delete bounces[key];
        bounceCount = Object.keys(bounces).length;
    }

    function bounceOffset(key: string, time: real): real {
        const b = bounces[key];
        if (!b) {
            return 0;
        }
        const elapsed = time - b.start;
        let cycles = Infinity;
        if (b.stopAt !== null) {
            cycles = Math.max(1, Math.ceil((b.stopAt - b.start) / bouncePeriod));
        }
        if (b.kind === "attention") {
            cycles = Math.min(cycles, 3);
        }
        if (elapsed >= cycles * bouncePeriod) {
            Qt.callLater(finishBounce, key);
            return 0;
        }
        const phase = (elapsed % bouncePeriod) / bouncePeriod;
        return bounceHeight * 4 * phase * (1 - phase); // ballistic arc
    }

    Component {
        id: bounceTimeout

        Timer {
            property string key
            interval: 15000
            onTriggered: {
                root.stopBounce(key);
                destroy();
            }
        }
    }

    // ── Appearing ─────────────────────────────────────────────────────────────
    property bool populated: false
    property var liveKeys: ({})
    property var lastSeen: ({})

    Timer {
        interval: 1000
        running: true
        onTriggered: root.populated = true
    }

    function isNewApp(key: string): bool {
        return populated && !(liveKeys[key] > 0) && !(Date.now() - (lastSeen[key] || 0) < 1500);
    }

    function iconCreated(icon: DockIcon) {
        const key = icon.appKey;
        liveKeys[key] = (liveKeys[key] || 0) + 1;
        if (icon.model.IsStartup) {
            startBounce(key, "launch");
        } else if (icon.running && bounces[key] && bounces[key].kind === "launch") {
            stopBounce(key);
        }
    }

    function iconDestroyed(icon: DockIcon, key: string, wasStartup: bool) {
        liveKeys[key] = Math.max(0, (liveKeys[key] || 0) - 1);
        lastSeen[key] = Date.now();
        if (wasStartup) {
            stopBounce(key);
        }
    }

    function isPermanent(icon: DockIcon): bool {
        return icon.model.LauncherUrlWithoutIcon.toString() === permanentLauncher;
    }

    function canRemove(icon: DockIcon): bool {
        return icon.model.HasLauncher && !isPermanent(icon);
    }

    function modelIndex(icon: DockIcon, childRow = -1): var {
        return tasksModel.makeModelIndex(icon.index, childRow);
    }

    // The windows behind an icon: its children for a group, else itself.
    function windowIndexes(icon: DockIcon): list<var> {
        if (icon.model.IsGroupParent) {
            const result = [];
            for (let i = 0; i < icon.model.ChildCount; ++i) {
                result.push(modelIndex(icon, i));
            }
            return result;
        }
        return icon.model.IsWindow ? [modelIndex(icon)] : [];
    }

    function role(index: var, role: int): var {
        return tasksModel.data(index, role);
    }

    function onCurrentDesktop(index: var): bool {
        if (role(index, TaskManager.AbstractTasksModel.IsOnAllVirtualDesktops)) {
            return true;
        }
        const desktops = role(index, TaskManager.AbstractTasksModel.VirtualDesktops) || [];
        return desktops.indexOf(virtualDesktopInfo.currentDesktop) !== -1;
    }

    // macOS: clicking an app brings all of its windows forward, most recent on top.
    // Minimised windows stay put unless they're all the app has.
    function activate(icon: DockIcon) {
        if (icon.model.IsLauncher) {
            startBounce(icon.appKey, "launch");
            tasksModel.requestActivate(modelIndex(icon));
            return;
        }
        if (icon.model.IsStartup) {
            return;
        }

        const windows = windowIndexes(icon);
        if (HarbourSettings.clickToMinimize && icon.model.IsActive) {
            for (const w of windows) {
                if (!role(w, TaskManager.AbstractTasksModel.IsMinimized)) {
                    tasksModel.requestToggleMinimized(w);
                }
            }
            return;
        }

        const byRecency = (a, b) => role(a, TaskManager.AbstractTasksModel.LastActivated)
                                    - role(b, TaskManager.AbstractTasksModel.LastActivated);
        const visible = windows.filter(w => !role(w, TaskManager.AbstractTasksModel.IsMinimized) && onCurrentDesktop(w));
        if (visible.length === 0) {
            // Nothing to raise here: restore (or switch to) the most recently used window.
            const recent = windows.slice().sort(byRecency).pop();
            if (recent) {
                tasksModel.requestActivate(recent);
            }
            return;
        }
        visible.sort(byRecency);
        for (const w of visible) {
            tasksModel.requestActivate(w);
        }
    }

    function newInstance(icon: DockIcon) {
        tasksModel.requestNewInstance(modelIndex(icon));
    }

    function openMenu(icon: DockIcon) {
        const menu = menuComponent.createObject(root, {
            dockIcon: icon,
            tasksModel: tasksModel,
            dock: root,
        });
        menu.showAbove(icon);
    }

    function beginDrag(icon: DockIcon, point: point) {
        dragPoint = point;
        dragIcon = icon;
        autoHide.hold();
    }

    function moveDrag(point: point) {
        dragPoint = point;
        if (dragRemoving) {
            return;
        }
        // Target slot = how many other icons have their centre left of the pointer.
        let target = 0;
        for (let i = 0; i < taskRepeater.count; ++i) {
            const item = taskRepeater.itemAt(i);
            if (item && item !== dragIcon && item.x + item.width / 2 < point.x) {
                ++target;
            }
        }
        target = Math.max(target, 1); // never in front of Dolphin
        if (isPermanent(dragIcon)) {
            target = 0;
        }
        if (target !== dragIcon.index) {
            tasksModel.move(dragIcon.index, target);
        }
    }

    function endDrag() {
        if (!dragIcon) {
            return;
        }
        if (dragRemoving) {
            const at = tasksRow.mapToItem(slide, dragPoint.x, dragPoint.y);
            poofComponent.createObject(slide, { x: at.x, y: at.y, size: iconSize });
            tasksModel.requestRemoveLauncher(dragIcon.model.LauncherUrlWithoutIcon);
        } else {
            tasksModel.syncLaunchers();
        }
        dragIcon = null;
        autoHide.release();
    }

    Item {
        id: menuAnchorItem

        height: 1
    }

    // QT_LOGGING_RULES=harbour.geometry.info=true logs where each item sits on screen
    // (resting, shown position), for scripted tests that click on dock items.
    LoggingCategory {
        id: lcGeometry

        name: "harbour.geometry"
        defaultLogLevel: LoggingCategory.Warning
    }

    Timer {
        id: geometryLogTimer

        interval: 600
        onTriggered: {
            const origin = root.dockWindow.globalOrigin;
            const parts = [];
            for (const item of tasksRow.children) {
                if (item.isDockSlot && item.visible) {
                    const c = item.mapToItem(slide, item.width / 2, item.height / 2);
                    parts.push(item.label + "@" + Math.round(origin.x + c.x) + "," + Math.round(origin.y + c.y));
                }
            }
            console.info(lcGeometry, parts.join(" | "));
        }
    }

    Connections {
        target: tasksRow

        function onWidthChanged() {
            geometryLogTimer.restart();
        }
    }

    Component.onCompleted: geometryLogTimer.start()

    Component {
        id: poofComponent

        Poof {}
    }

    Component {
        id: menuComponent

        TaskMenu {}
    }

    Item {
        id: slide

        width: root.width
        height: root.height
        // Deliberately never made invisible: off-screen it is clipped away, but the
        // first frame still builds its pipelines, so the first reveal doesn't stutter.
        y: root.hiddenOffset

        HoverHandler {
            id: hoverHandler

            onPointChanged: root.pointerX = point.position.x
        }

        DockBackground {
            id: background

            x: root.plateLeft
            y: root.plateTop
            width: root.plateRight - root.plateLeft
            height: root.dockHeight
            glassOpacity: HarbourSettings.glassOpacity / 100
            dark: root.dark
        }

        // KWin blurs behind the plate wherever it is, including mid-slide. (Pinning the
        // blur during slides avoided a rare late frame but needed a tint change the user
        // disliked; see ROADMAP.)
        Binding {
            target: root.dockWindow
            property: "blurRect"
            value: Qt.rect(slide.x + background.x, slide.y + background.y, background.width, background.height)
        }
        Binding {
            target: root.dockWindow
            property: "blurRadius"
            value: background.radius
        }

        Row {
            id: tasksRow

            x: Math.round((parent.width - width) / 2)
            y: background.y + root.padding

            // Adding or removing an icon re-centres the row; glide rather than jump.
            Behavior on x {
                enabled: root.populated && root.dragIcon === null
                NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
            }

            move: Transition {
                NumberAnimation { properties: "x"; duration: 220; easing.type: Easing.OutCubic }
            }

            Repeater {
                id: taskRepeater

                // Attached only after the shared model's initial launcher churn.
                model: root.shared.ready ? root.tasksModel : null

                delegate: DockIcon {
                    dock: root
                    tasksModel: root.tasks
                }
            }

            DockSeparator {
                dock: root
                visible: stackRepeater.count > 0 || HarbourSettings.showTrash
            }

            Repeater {
                id: stackRepeater

                model: HarbourSettings.stacks

                delegate: StackIcon {
                    required property string modelData
                    required property int index

                    dock: root
                    folderPath: modelData
                    stackIndex: index
                    view: HarbourSettings.stackViews[index] || "fan"
                }
            }

            TrashIcon {
                dock: root
                visible: HarbourSettings.showTrash
            }
        }

        NameLabel {
            id: nameLabel

            dark: root.dark
            readonly property DockSlot target: root.hoveredIcon

            visible: target !== null && root.dragIcon === null && !root.menuOpen
            text: target ? target.label : ""
            x: target ? Math.round((target.shownLeft + target.shownRight) / 2 - width / 2) : 0
            y: target ? Math.round(tasksRow.y + root.iconSize * (1 - target.zoom) - target.bounceOffset - height - 10) : 0
        }

        DropArea {
            id: dropArea

            property DockSlot target: null

            x: background.x
            y: background.y
            width: background.width
            height: background.height + root.bottomGap

            function iconAt(dropX: real): DockSlot {
                return root.iconAtShownX(mapToItem(slide, dropX, 0).x);
            }

            onEntered: drag => {
                drag.accepted = drag.hasUrls;
                target = iconAt(drag.x);
            }
            onPositionChanged: drag => target = iconAt(drag.x)
            onExited: target = null
            onDropped: drop => {
                const urls = drop.urls;
                // Folders dropped between items or on the separator are kept as stacks,
                // as on macOS; dropped on an item, they go to that item as usual.
                const folders = target ? [] : root.stackPaths(urls);
                if (folders.length > 0) {
                    root.pinFolders(folders);
                } else if (target && target.acceptDrop) {
                    target.acceptDrop(urls); // Trash, stacks
                } else if (urls.length === 1 && DockTools.isApplication(urls[0])) {
                    tasksModel.requestAddLauncher(urls[0]);
                } else if (target && target.model && !target.model.IsStartup) {
                    tasksModel.requestOpenUrls(root.modelIndex(target), urls);
                }
                target = null;
            }
        }
    }

    NumberAnimation {
        id: slideAnimation

        target: slide
        property: "y"
        onFinished: root.autoHide.slideFinished()
    }

    Connections {
        target: root.autoHide

        function onRevealedChanged() {
            const to = root.autoHide.revealed ? 0 : root.hiddenOffset;
            const full = root.autoHide.revealed ? HarbourSettings.revealDuration : HarbourSettings.hideDuration;
            // A reversal mid-slide covers less ground, so it gets proportionally less time.
            const remaining = Math.abs(to - slide.y) / root.hiddenOffset;

            slideAnimation.stop();
            slideAnimation.to = to;
            slideAnimation.duration = Math.max(1, Math.round(full * remaining));
            slideAnimation.easing.type = root.autoHide.revealed ? Easing.OutCubic : Easing.InCubic;
            slideAnimation.start();
        }
    }
}
