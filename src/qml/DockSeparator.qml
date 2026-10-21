import QtQuick

// The thin line between apps and stacks/Trash. Moves with magnification like
// everything else in the row. Dragging it up or down resizes the dock, as on macOS.
Item {
    id: separator

    required property Item dock

    readonly property real restLeft: parent ? parent.x + x : 0
    readonly property real shift: dock.magnify(restLeft + width / 2) - (restLeft + width / 2)

    width: Math.round(dock.iconSize * 0.3)
    height: dock.iconSize

    MouseArea {
        id: resizeArea

        // Where the drag started, on screen, and the size then (as set, before any
        // per-screen scaling).
        property real startY
        property int startSize

        // A little wider than the line, and down to the screen edge.
        x: separator.shift - 4
        width: parent.width + 8
        height: parent.height + separator.dock.padding + separator.dock.bottomGap
        hoverEnabled: true
        cursorShape: Qt.SizeVerCursor

        function screenY(mouse): real {
            return mapToItem(null, mouse.x, mouse.y).y + separator.dock.dockWindow.globalOrigin.y;
        }

        onPressed: mouse => {
            startY = screenY(mouse);
            startSize = HarbourSettings.iconSize;
            separator.dock.resizingByHand = true;
            separator.dock.autoHide.hold();
        }
        onPositionChanged: mouse => {
            if (!pressed) {
                return;
            }
            // Up makes it bigger; in the screen's own pixels, whatever its scale.
            const delta = (startY - screenY(mouse)) / separator.dock.dockWindow.sizeScale;
            HarbourSettings.iconSize = Math.max(32, Math.min(separator.dock.maxIconSize, Math.round(startSize + delta)));
        }
        onReleased: finish()
        onCanceled: finish()

        Binding {
            target: separator.dock
            property: "resizeReady"
            value: resizeArea.containsMouse || resizeArea.pressed
        }

        function finish() {
            if (!separator.dock.resizingByHand) {
                return;
            }
            separator.dock.resizingByHand = false;
            separator.dock.autoHide.release();
            HarbourSettings.save();
        }
    }

    Rectangle {
        width: 1
        height: Math.round(parent.height * 0.82)
        x: Math.round((parent.width - width) / 2 + separator.shift)
        y: (parent.height - height) / 2 + parent.height * 0.06
        color: separator.dock.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.18)
    }
}
