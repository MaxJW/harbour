import QtQuick
import QtQuick.Templates as T
import Harbour

// A stack's contents, newest first, as macOS shows them: a fan of files arcing up
// from the stack (the default), a glass grid, or a plain list (a KDE menu).
//
// A Menu with custom content rather than a Popup: on Wayland a window popup's x/y
// are ignored, and only Menu reliably anchors to its parent item (see DockMenu).
// The unstyled template, since the desktop style's Menu expects a list of items.
T.Menu {
    id: stackPopup

    required property Item dock
    required property StackIcon stack

    readonly property string mode: stack.view === "grid" || stack.view === "list" ? stack.view : "fan"

    // Read once, synchronously, when the popup opens: the layout depends on the count.
    readonly property var entries: DockTools.folderEntries(stack.folderUrl, mode === "fan" ? fanLimit : 120)
    readonly property int entryCount: entries.length

    // Fan geometry. Items rise from the stack along a gentle arc of radius fanRadius,
    // curving right and tilting as they go, like macOS.
    readonly property int fanLimit: 12
    readonly property real fanIcon: Math.round(dock.iconSize * 0.86)
    readonly property real fanStep: fanIcon + 8
    readonly property real fanRadius: 2600
    // The window reaches fanLeft to the left of the stack (room for long names) but
    // only as far right as the fan goes, so it stays clear of the screen's right edge:
    // a popup that doesn't fit is slid back on screen, away from the stack.
    readonly property real fanLeft: 320
    readonly property real fanPillMax: fanLeft - fanIcon / 2 - 24
    readonly property string folderLinkText: qsTr("Open in Dolphin  ›")
    readonly property real fanRight: Math.ceil(Math.max(Math.min(folderLinkMetrics.width + 26, fanPillMax) / 2, fanIcon * 0.54)
                                               + arcX((entryCount + 1) * fanStep) + 16)

    readonly property int gridColumns: Math.max(1, Math.min(5, entryCount))
    readonly property int gridRows: Math.max(1, Math.min(3, Math.ceil(entryCount / gridColumns)))
    readonly property real gridCell: 104

    readonly property real contentWidth_: mode === "fan" ? fanLeft + fanRight : gridColumns * gridCell + 32
    readonly property real contentHeight_: mode === "fan" ? (entryCount + 1) * fanStep + 28
                                                         : 48 + gridRows * gridCell + 52

    // Stands in for a menu here; owner is what Dock.shouldOpenMenu() compares against.
    readonly property Item owner: stack

    // 0 → 1 as the fan opens, and back as it closes.
    property real opening: 0

    popupType: T.Popup.Window
    padding: 0
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    width: contentWidth_
    height: contentHeight_
    background: Item {}

    SystemPalette {
        id: palette
    }

    TextMetrics {
        id: folderLinkMetrics

        text: stackPopup.folderLinkText
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }

    // Everything lives inside the content item: children declared on a Menu itself
    // would be treated as menu entries.
    contentItem: Item {
        implicitWidth: stackPopup.contentWidth_
        implicitHeight: stackPopup.contentHeight_

        // ── Fan ──────────────────────────────────────────────────────────────
        Item {
            anchors.fill: parent
            visible: stackPopup.mode === "fan"

            Repeater {
                model: stackPopup.mode === "fan" ? stackPopup.entries.length + 1 : 0

                delegate: Item {
                    id: fanItem

                    required property int index

                    // The last slot, at the top, is "Open in Dolphin".
                    readonly property bool isFolderLink: index === stackPopup.entryCount
                    readonly property var entry: isFolderLink ? null : stackPopup.entries[index]

                    // Each item starts a little after the one below it, overshooting
                    // slightly as it lands.
                    readonly property real progress: Math.max(0, Math.min(1, stackPopup.opening * 1.6 - index * 0.05))
                    readonly property real eased: {
                        const t = progress - 1;
                        return 1 + 2.2 * t * t * t + 1.2 * t * t;
                    }

                    // Height above the stack, and where the arc puts the item.
                    readonly property real rise: (index + 1) * stackPopup.fanStep
                    readonly property real arcX: stackPopup.arcX(rise)
                    readonly property real tilt: Math.asin(rise / stackPopup.fanRadius) * 180 / Math.PI * 0.7

                    readonly property bool hovered: hoverArea.containsMouse

                    width: stackPopup.contentWidth_
                    height: stackPopup.fanStep
                    // From the stack (bottom centre) to its place on the arc.
                    x: arcX * eased
                    y: stackPopup.contentHeight_ - stackPopup.fanStep - 4 - (rise - stackPopup.fanStep) * eased
                    opacity: Math.min(1, progress * 2)

                    transform: Rotation {
                        origin.x: stackPopup.fanLeft
                        origin.y: fanItem.height / 2
                        angle: fanItem.tilt * fanItem.eased
                    }

                    // The pill and icon, drawn upright into a layer which the fan then tilts as
                    // one smoothly sampled picture. Tilted directly, the software renderer
                    // snaps each glyph to whole pixels on its own (a ragged baseline) and leaves
                    // the edges of previews jagged; the layer's clear margin gives soft edges.
                    Item {
                        id: face

                        readonly property real margin: 4

                        width: (fanItem.isFolderLink ? pill.width : pill.width + 10 + stackPopup.fanIcon) + 2 * margin
                        height: parent.height
                        // The icon centred over the stack; "Open in Dolphin" centred.
                        x: fanItem.isFolderLink ? stackPopup.fanLeft - width / 2 : stackPopup.fanLeft + stackPopup.fanIcon / 2 + margin - width
                        layer.enabled: true
                        layer.smooth: true

                        Image {
                            id: fanImage

                            visible: !fanItem.isFolderLink
                            width: stackPopup.fanIcon
                            height: stackPopup.fanIcon
                            x: parent.width - face.margin - width
                            anchors.verticalCenter: parent.verticalCenter
                            scale: fanItem.hovered ? 1.08 : 1
                            source: fanItem.entry ? stackPopup.iconFor(fanItem.entry.url, fanItem.entry.isDir) : ""
                            sourceSize.width: width * Screen.devicePixelRatio
                            sourceSize.height: height * Screen.devicePixelRatio
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true

                            Behavior on scale {
                                NumberAnimation { duration: 90 }
                            }
                        }

                        // Name pill to the left of the icon; the accent colour when hovered.
                        Rectangle {
                            id: pill

                            readonly property string name: fanItem.isFolderLink ? stackPopup.folderLinkText : fanItem.entry.name

                            height: pillText.implicitHeight + 8
                            width: Math.min(pillText.implicitWidth + 22, stackPopup.fanPillMax)
                            radius: height / 2
                            anchors.verticalCenter: parent.verticalCenter
                            x: face.margin
                            color: fanItem.hovered ? palette.highlight
                                                   : (stackPopup.dock.dark ? Qt.rgba(0.11, 0.11, 0.13, 0.88) : Qt.rgba(0.97, 0.97, 0.98, 0.94))
                            border.width: 1
                            border.color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10)

                            Text {
                                id: pillText

                                anchors.centerIn: parent
                                width: Math.min(implicitWidth, parent.width - 22)
                                text: pill.name
                                elide: Text.ElideMiddle
                                color: fanItem.hovered ? palette.highlightedText
                                                       : (stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.92) : Qt.rgba(0, 0, 0, 0.85))
                                // Upright in the layer, so drawn like the dock's name label.
                                renderType: Text.NativeRendering
                                font.pixelSize: 13
                                font.weight: fanItem.isFolderLink ? Font.DemiBold : Font.Medium
                            }
                        }
                    }

                    MouseArea {
                        id: hoverArea

                        // The icon and its label together.
                        x: face.x + face.margin
                        width: face.width - 2 * face.margin
                        height: parent.height
                        hoverEnabled: true
                        onClicked: stackPopup.openItem(fanItem.isFolderLink ? stackPopup.stack.folderUrl : fanItem.entry.url)
                    }
                }
            }
        }

        // ── Grid ─────────────────────────────────────────────────────────────
        Item {
            anchors.fill: parent
            visible: stackPopup.mode === "grid"
            // Grows out of the stack below it, and shrinks back into it.
            opacity: Math.min(1, stackPopup.opening * 1.5)
            scale: 0.92 + 0.08 * stackPopup.opening
            transformOrigin: Item.Bottom

            // The same glass as the dock, with the compositor blurring behind it.
            DockBackground {
                id: glass

                anchors.fill: parent
                glassOpacity: HarbourSettings.glassOpacity / 100
                dark: stackPopup.dock.dark
                radius: 18
            }

            Text {
                x: 20
                y: 14
                text: stackPopup.stack.label
                color: stackPopup.dock.dark ? "white" : "black"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            GridView {
                x: 16
                y: 44
                width: parent.width - 32
                height: parent.height - 44 - 48
                clip: true
                cellWidth: stackPopup.gridCell
                cellHeight: stackPopup.gridCell
                model: stackPopup.mode === "grid" ? stackPopup.entries : []
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: gridItem

                    required property var modelData

                    width: stackPopup.gridCell
                    height: stackPopup.gridCell

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 10
                        color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.07)
                        visible: cellMouse.containsMouse
                    }

                    Image {
                        width: 56
                        height: 56
                        x: (parent.width - width) / 2
                        y: 8
                        source: stackPopup.iconFor(gridItem.modelData.url, gridItem.modelData.isDir)
                        sourceSize.width: 56
                        sourceSize.height: 56
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }

                    Text {
                        x: 4
                        y: 68
                        width: parent.width - 8
                        text: gridItem.modelData.name
                        color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.92) : Qt.rgba(0, 0, 0, 0.85)
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    MouseArea {
                        id: cellMouse

                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: stackPopup.openItem(gridItem.modelData.url)
                    }
                }
            }

            // Drawn here rather than a Controls button: that alone loaded the whole desktop
            // style and Kirigami into the dock (several MB) the first time a stack opened.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 10
                width: openRow.implicitWidth + 24
                height: 30
                radius: height / 2
                color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, openMouse.containsMouse ? 0.16 : 0.09)
                                            : Qt.rgba(0, 0, 0, openMouse.containsMouse ? 0.12 : 0.06)
                border.width: 1
                border.color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10)

                Row {
                    id: openRow

                    anchors.centerIn: parent
                    spacing: 6

                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        height: 16
                        sourceSize.width: 16
                        sourceSize.height: 16
                        source: DockTools.iconUrl("system-file-manager", false, DockTools.iconThemeRevision)
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Open in Dolphin")
                        color: stackPopup.dock.dark ? Qt.rgba(1, 1, 1, 0.92) : Qt.rgba(0, 0, 0, 0.85)
                        font.pixelSize: 12
                    }
                }

                MouseArea {
                    id: openMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: stackPopup.openItem(stackPopup.stack.folderUrl)
                }
            }
        }
    }

    NumberAnimation {
        id: openAnimation

        target: stackPopup
        property: "opening"
        from: 0
        to: 1
        duration: stackPopup.mode === "fan" ? 420 : 160
        easing.type: stackPopup.mode === "fan" ? Easing.Linear : Easing.OutCubic
        // The blur goes on once the grid is at full size, and off as it closes.
        onFinished: {
            if (stackPopup.mode === "grid") {
                DockTools.blurBehind(glass, glass.radius);
            }
        }
    }

    // Closing gathers everything back into the stack. Only closes this process starts
    // (a click on the dock, an item, Escape) can animate: when the compositor dismisses
    // the popup, its surface is already gone.
    exit: Transition {
        NumberAnimation {
            property: "opening"
            to: 0
            duration: stackPopup.mode === "fan" ? 200 : 120
            easing.type: Easing.InCubic
        }
    }

    // How far right of the stack the arc has drifted at a given height.
    function arcX(rise: real): real {
        return fanRadius - Math.sqrt(fanRadius * fanRadius - rise * rise);
    }

    function iconFor(url: url, isDir: bool): url {
        // Files show their preview (photos, PDFs, videos...) or else their type's icon.
        return isDir ? DockTools.iconUrl(DockTools.iconNameForUrl(url), false, DockTools.iconThemeRevision) : DockTools.previewUrl(url);
    }

    function openItem(url: url) {
        DockTools.openUrl(url);
        close();
    }

    function showAbove() {
        if (mode === "list") {
            // The list view is just a menu. It gets everything it needs up front: inside
            // a Menu, "popup" names the menu's own popup() method, not this object.
            const menu = listMenuComponent.createObject(dock, {
                dock: dock,
                folderUrl: stack.folderUrl,
            });
            menu.aboutToHide.connect(() => stackPopup.destroy(1000));
            menu.showAbove(stack);
            return;
        }
        // Same anchoring trick as DockMenu.
        // Above the stack as drawn (it stays magnified while the popup is open).
        dock.holdMagnification();
        const top = stack.shownItem.mapToItem(dock.menuAnchor.parent, stack.width / 2, 0);
        const p = Qt.point(top.x - (mode === "fan" ? fanLeft : contentWidth_ / 2), top.y - 4);
        dock.menuAnchor.x = p.x;
        dock.menuAnchor.y = p.y;
        dock.menuAnchor.width = contentWidth_;
        stackPopup.popup(dock.menuAnchor);
    }

    onAboutToShow: {
        dock.currentMenu = stackPopup;
        dock.menuOpen = true;
        dock.autoHide.hold();
    }
    onOpened: openAnimation.start()
    onAboutToHide: {
        openAnimation.stop();
        if (mode === "grid") {
            DockTools.clearBlurBehind(glass);
        }
        // Now, not once the closing animation is over: the click that closed it must
        // not open it again. And before menuOpen drops (see Dock.onMenuOpenChanged).
        dock.noteMenuClosed(stack);
        if (dock.currentMenu === stackPopup) {
            dock.currentMenu = null;
            dock.menuOpen = false;
        }
        dock.autoHide.release();
    }
    onClosed: destroy(1000) // see DockMenu

    Component {
        id: listMenuComponent

        DockMenu {
            id: listMenu

            required property url folderUrl

            // Like a normal KDE context menu: the newest dozen, folders as submenus.
            function build() {
                StackMenus.fill(listMenu, folderUrl, 12);
            }
        }
    }
}
