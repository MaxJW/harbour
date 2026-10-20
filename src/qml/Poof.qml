import QtQuick

// The puff of smoke left behind when an icon is dragged off the dock.
// Positioned by its centre; destroys itself when done.
Item {
    id: poof

    property real size: 56

    width: 0
    height: 0

    Repeater {
        model: 7

        Rectangle {
            required property int index

            readonly property real angle: index * 2 * Math.PI / 7 + 0.4
            readonly property real puff: poof.size * (index === 0 ? 0.55 : 0.38)
            property real progress: 0

            width: puff
            height: puff
            radius: puff / 2
            x: (index === 0 ? 0 : Math.cos(angle) * poof.size * 0.42 * progress) - width / 2
            y: (index === 0 ? 0 : Math.sin(angle) * poof.size * 0.32 * progress) - height / 2
            scale: 0.4 + 0.9 * progress
            opacity: 0.85 * (1 - progress * progress)
            color: Qt.rgba(0.93, 0.93, 0.95, 1)

            NumberAnimation on progress {
                from: 0
                to: 1
                duration: 420
                easing.type: Easing.OutCubic
            }
        }
    }

    Timer {
        interval: 450
        running: true
        onTriggered: poof.destroy()
    }
}
