import QtQuick

// The dock's plate, after macOS 27's Liquid Glass: the compositor blurs what's
// behind it (see DockWindow.blurRect); on top sit an even tint whose strength
// follows the opacity setting, a faint highlight along the top edge, and a rim.
//
// No drop shadow: under a translucent plate a shadow shows through as a dark band
// across the middle.
Item {
    id: plate

    // 0 = clear, 1 = opaque.
    property real glassOpacity: 0.5
    property bool dark: true
    property real radius: Math.min(height * 0.38, 28)

    // Tint, the same everywhere.
    Rectangle {
        anchors.fill: parent
        radius: plate.radius
        // ⚠ Tuned by eye; medium (0.5) should read like macOS 27's default.
        readonly property real alpha: (plate.dark ? 0.06 : 0.10) + 0.58 * plate.glassOpacity

        color: plate.dark ? Qt.rgba(0.13, 0.13, 0.15, alpha) : Qt.rgba(0.98, 0.98, 1.0, alpha)
    }

    // Light catching the top edge of the glass; transparent from a third of the way down.
    Rectangle {
        anchors.fill: parent
        radius: plate.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, plate.dark ? 0.08 : 0.20) }
            GradientStop { position: 0.32; color: Qt.rgba(1, 1, 1, 0) }
        }
    }

    // Rim.
    Rectangle {
        anchors.fill: parent
        radius: plate.radius
        color: "transparent"
        border.width: 1
        border.color: plate.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.55)
    }

    // A hairline outside the rim keeps the plate distinct from busy or light wallpapers.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: plate.radius + 1
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, plate.dark ? 0.28 : 0.10)
    }

    // Specular highlight along the top edge, fading out towards the corners.
    Rectangle {
        x: plate.radius * 0.6
        y: 1
        width: parent.width - plate.radius * 1.2
        height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0) }
            GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, plate.dark ? 0.26 : 0.75) }
            GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0) }
        }
    }
}
