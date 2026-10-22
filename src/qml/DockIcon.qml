import QtQuick
import QtQuick.Window
import org.kde.taskmanager as TaskManager
import Harbour

// One app in the dock: a DockSlot driven by the tasks model, plus dragging,
// bouncing, badges and the geometry KWin's minimise animation flies to.
DockSlot {
    id: icon

    required property var model
    required property int index
    required property TaskManager.TasksModel tasksModel

    readonly property bool dragged: dock.dragIcon === icon
    readonly property bool dropTarget: dock.dropTarget === icon
    readonly property int childCount: model.ChildCount ?? 0
    readonly property string appKey: dock.appKey(model)
    readonly property url launcherUrl: model.LauncherUrlWithoutIcon ?? ""

    readonly property int badge: LauncherEntries.count(launcherUrl, LauncherEntries.revision)
    readonly property real progress: LauncherEntries.progress(launcherUrl, LauncherEntries.revision)

    readonly property bool demandsAttention: model.IsDemandingAttention === true
                                             || LauncherEntries.urgent(launcherUrl, LauncherEntries.revision)
    onDemandsAttentionChanged: demandsAttention ? dock.startBounce(appKey, "attention") : dock.stopBounce(appKey)

    // A pinned app that isn't installed (a default this system lacks, or one mid-
    // reinstall) stays in the settings, so it comes back, but isn't shown.
    readonly property bool missing: model.IsLauncher === true && !DockTools.isApplication(launcherUrl)
    visible: !missing

    // Being unpinned from its menu: shrinks away rather than vanishing (see Dock).
    property bool leaving: false

    // Captured at creation: required properties can already be gone during destruction.
    property string createdKey
    property bool createdAsStartup

    // Pointer offset from the icon centre at press time, kept while dragging.
    property point pressOffset
    property point pressPoint

    iconSource: DockTools.iconUrl(model.decoration, HarbourSettings.evenTiles)
    // Games run through Steam have no desktop file; Steam knows their names.
    readonly property string steamName: DockTools.steamAppName(model.AppId ?? "")
    // Bumped when one of the group's window titles changes (see label).
    property int titleRevision: 0

    label: {
        if (steamName && childCount > 1) {
            // A launcher and the game it starts share Steam's app id (Battle.net and
            // Hearthstone): the newest window, the game once it's up, names the icon.
            // Games often title their window only after it has appeared.
            titleRevision;
            return dock.role(dock.modelIndex(icon, childCount - 1), Qt.DisplayRole) || steamName;
        }
        return steamName || model.AppName || model.display || "";
    }
    running: model.IsWindow === true || model.IsGroupParent === true
    bounceOffset: dock.bounceOffset(appKey, dock.now)
    iconOpacity: (dragged && dock.dragRemoving) ? 0.5 : dropTarget ? 0.65 : 1
    lifted: dragged
    liftPosition: Qt.point(dock.dragPoint.x - (x + width / 2) - pressOffset.x,
                           dock.dragPoint.y - height / 2 - pressOffset.y)

    // Unread count, top right of the icon, macOS red.
    Rectangle {
        visible: icon.badge > 0
        height: Math.round(icon.dock.iconSize * 0.36)
        width: Math.max(height, badgeText.implicitWidth + height * 0.5)
        radius: height / 2
        x: icon.icon.x + icon.icon.width - width * 0.72
        y: -height * 0.22
        color: "#ff3b30"
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.12)

        Text {
            id: badgeText

            anchors.centerIn: parent
            text: icon.badge > 99 ? "99+" : icon.badge
            color: "white"
            font.pixelSize: parent.height * 0.62
            font.weight: Font.DemiBold
        }
    }

    // Progress bar across the bottom of the icon.
    Rectangle {
        visible: icon.progress >= 0
        x: icon.icon.x + icon.icon.width * 0.12
        y: icon.icon.height - height - icon.icon.height * 0.1
        width: icon.icon.width * 0.76
        height: Math.max(5, Math.round(icon.dock.iconSize * 0.1))
        radius: height / 2
        color: Qt.rgba(0, 0, 0, 0.55)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.25)

        Rectangle {
            x: 1
            y: 1
            height: parent.height - 2
            width: Math.max(height, (parent.width - 2) * Math.max(0, icon.progress))
            radius: height / 2
            color: "#0a84ff"
        }
    }

    // Peeking up out of the screen edge while the dock is hidden and this app wants
    // attention: the same bounce, from below the edge to almost the whole icon.
    Image {
        parent: icon.dock.peekLayer
        visible: icon.dock.autoHide.peeking && icon.demandsAttention && icon.bounceOffset > 0
        x: icon.restLeft + icon.dock.gap / 2
        y: parent ? parent.height - icon.bounceOffset / icon.dock.bounceHeight * (icon.dock.iconSize * 0.85 + icon.dock.bottomGap) : 0
        width: icon.dock.iconSize
        height: icon.dock.iconSize
        sourceSize.width: icon.dock.magnifiedIconSize * Screen.devicePixelRatio
        sourceSize.height: icon.dock.magnifiedIconSize * Screen.devicePixelRatio
        source: visible ? icon.iconSource : ""
        mipmap: true
        smooth: true
    }

    Connections {
        target: icon.mouseArea

        function onPressed(mouse) {
            icon.pressPoint = icon.mouseArea.mapToItem(icon, mouse.x, mouse.y);
            if (mouse.button === Qt.RightButton) {
                if (icon.dock.menuJustClosed()) {
                    icon.dock.openMenuAfterRelease(icon);
                } else {
                    icon.contextMenu();
                }
            }
        }

        function onPressAndHold(mouse) {
            if (mouse.button === Qt.LeftButton && !icon.dragged && icon.dock.shouldOpenMenu(icon)) {
                icon.dock.openMenu(icon);
            }
        }

        function onPositionChanged(mouse) {
            if (!(mouse.buttons & Qt.LeftButton)) {
                return;
            }
            const local = icon.mouseArea.mapToItem(icon, mouse.x, mouse.y);
            const rowPoint = icon.mouseArea.mapToItem(icon.parent, mouse.x, mouse.y);
            if (icon.dragged) {
                icon.dock.moveDrag(rowPoint);
            } else if (Math.hypot(local.x - icon.pressPoint.x, local.y - icon.pressPoint.y) > 6) {
                icon.pressOffset = Qt.point(icon.pressPoint.x - icon.width / 2, icon.pressPoint.y - icon.height / 2);
                icon.dock.beginDrag(icon, rowPoint);
            }
        }

        function onReleased() {
            icon.dock.endDrag();
        }

        function onCanceled() {
            icon.dock.endDrag();
        }

        function onClicked(mouse) {
            if (icon.dragged) {
                return;
            }
            if (mouse.button === Qt.LeftButton) {
                icon.dock.activate(icon);
            } else if (mouse.button === Qt.MiddleButton) {
                icon.dock.newInstance(icon);
            }
        }
    }

    function contextMenu() {
        if (icon.dock.shouldOpenMenu(icon)) {
            icon.dock.openMenu(icon);
        }
    }

    // Tell KWin where this icon sits so minimise animations aim at it. Published
    // for the resting, shown position even while the dock is hidden or magnified.
    function publishGeometry() {
        if (!icon.model.IsWindow && !icon.model.IsGroupParent) {
            return;
        }
        const p = icon.mapToItem(icon.dock.slideContainer, icon.dock.gap / 2, 0);
        const origin = icon.dock.dockWindow.globalOrigin;
        tasksModel.requestPublishDelegateGeometry(tasksModel.makeModelIndex(index),
                                                  Qt.rect(origin.x + p.x, origin.y + p.y, icon.icon.width, icon.icon.height),
                                                  icon);
    }

    Timer {
        id: publishTimer

        interval: 250
        onTriggered: icon.publishGeometry()
    }

    onXChanged: publishTimer.restart()
    onRunningChanged: publishTimer.restart()
    onChildCountChanged: publishTimer.restart()

    Component.onCompleted: {
        createdKey = appKey;
        createdAsStartup = model.IsStartup === true;
        publishTimer.restart();
        if (dock.isNewApp(appKey)) {
            popIn();
        }
        dock.iconCreated(icon);
        if (demandsAttention) {
            dock.startBounce(appKey, "attention");
        }
    }
    Component.onDestruction: dock.iconDestroyed(icon, createdKey, createdAsStartup)

    Connections {
        target: icon.tasksModel
        enabled: icon.steamName !== "" && icon.childCount > 1

        function onDataChanged(topLeft, bottomRight, roles) {
            if (topLeft.parent.row === icon.index && (roles.length === 0 || roles.includes(Qt.DisplayRole))) {
                ++icon.titleRevision;
            }
        }
    }

    Connections {
        target: icon.dock.dockWindow

        function onGlobalOriginChanged() {
            publishTimer.restart();
        }
    }
}
