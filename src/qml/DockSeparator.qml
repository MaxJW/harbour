import QtQuick

// The thin line between apps and stacks/Trash. Moves with magnification like
// everything else in the row.
Item {
    id: separator

    required property Item dock

    readonly property real restLeft: parent ? parent.x + x : 0
    readonly property real shift: dock.magnify(restLeft + width / 2) - (restLeft + width / 2)

    width: Math.round(dock.iconSize * 0.3)
    height: dock.iconSize

    Rectangle {
        width: 1
        height: Math.round(parent.height * 0.82)
        x: Math.round((parent.width - width) / 2 + separator.shift)
        y: (parent.height - height) / 2 + parent.height * 0.06
        color: separator.dock.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.18)
    }
}
