import QtQuick

// The app name shown above the icon under the pointer.
Rectangle {
    id: label

    property alias text: textItem.text
    property bool dark: true

    width: textItem.implicitWidth + 24
    height: textItem.implicitHeight + 10
    radius: height / 2
    color: dark ? Qt.rgba(0.11, 0.11, 0.13, 0.86) : Qt.rgba(0.97, 0.97, 0.98, 0.92)
    border.width: 1
    border.color: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.10)

    Text {
        id: textItem

        anchors.centerIn: parent
        color: label.dark ? Qt.rgba(1, 1, 1, 0.92) : Qt.rgba(0, 0, 0, 0.85)
        font.pixelSize: 13
        font.weight: Font.Medium
        renderType: Text.NativeRendering
    }
}
