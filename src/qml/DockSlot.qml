import QtQuick
import QtQuick.Window
import Harbour

// What every item in the dock row has in common: apps, stacks and the Trash.
//
// The Row lays slots out at rest size; magnification, bouncing and dragging are
// purely visual transforms on top, so the resting layout never moves under the
// pointer. Children declared in a subclass (badges, progress) go inside the
// transformed icon, so they scale and bounce with it.
Item {
    id: slot

    required property Item dock

    property url iconSource
    property string label
    property bool running: false // draws the open indicator
    property real bounceOffset: 0
    // Pops in when an item is new to the dock.
    property real appear: 1
    property real iconOpacity: 1

    // While dragged, the subclass positions the icon itself (in slot coordinates).
    property bool lifted: false
    property point liftPosition
    // Once dropped, the icon glides from where it was let go (in row coordinates)
    // into its slot; settleProgress runs from 1 to 0.
    property point settleFrom
    property real settleProgress: 0

    readonly property bool isDockSlot: true

    // Magnification: where this slot's resting edges land once the dock is magnified.
    readonly property real restLeft: parent ? parent.x + x : 0
    readonly property real shownLeft: dock.magnify(restLeft)
    readonly property real shownRight: dock.magnify(restLeft + width)
    readonly property real zoom: lifted ? 1 : (shownRight - shownLeft) / width
    readonly property real shift: lifted ? 0 : (shownLeft + shownRight) / 2 - (restLeft + width / 2)
    // Drawn at exactly its resting size (bouncing only moves it).
    readonly property bool atRest: !lifted && appear === 1 && Math.abs(zoom - 1) < 0.002 && !dock.resizing

    readonly property alias mouseArea: mouseArea
    readonly property alias icon: image
    // The icon as drawn (magnified and shifted): menus open above this, since the dock
    // stays magnified while they're open.
    readonly property Item shownItem: visual
    default property alias decorations: bouncer.data

    width: dock.iconSize + dock.gap
    height: dock.iconSize

    Item {
        id: visual

        width: slot.width
        height: slot.height
        z: slot.lifted || settleAnimation.running ? 1 : 0

        transform: [
            Scale {
                origin.x: visual.width / 2
                origin.y: visual.height
                xScale: slot.zoom * slot.appear
                yScale: slot.zoom * slot.appear
            },
            Translate {
                x: slot.lifted ? slot.liftPosition.x : slot.shift + (slot.settleFrom.x - slot.x) * slot.settleProgress
                y: slot.lifted ? slot.liftPosition.y : slot.settleFrom.y * slot.settleProgress
            }
        ]

        // Only the picture bounces. The mouse area below stays put, or a click aimed at a
        // bouncing icon (say, right after launching it) would miss it mid-hop.
        Item {
            id: bouncer

            width: visual.width
            height: visual.height

            transform: Translate {
                y: slot.lifted ? 0 : -slot.bounceOffset
            }

            // At rest the icon is drawn from a texture rendered at exactly its on-screen
            // size, pixel for pixel, so it is as sharp as anywhere else on the desktop.
            Image {
                id: image

                x: slot.dock.gap / 2
                width: slot.dock.iconSize
                height: slot.dock.iconSize
                // The size being animated to, so a resize renders it once, at the end.
                sourceSize.width: slot.dock.targetIconSize * Screen.devicePixelRatio
                sourceSize.height: slot.dock.targetIconSize * Screen.devicePixelRatio
                source: slot.iconSource
                smooth: false
                visible: slot.atRest
                opacity: slot.iconOpacity

                Behavior on opacity {
                    NumberAnimation { duration: 120 }
                }
            }

            // While scaled (magnified, popping in, dragged, resizing), a texture rendered once at the
            // largest size the icon is ever shown at, mipmapped down, so magnification never
            // re-renders the icon.
            Image {
                x: image.x
                width: image.width
                height: image.height
                sourceSize.width: slot.dock.magnifiedIconSize * Screen.devicePixelRatio
                sourceSize.height: slot.dock.magnifiedIconSize * Screen.devicePixelRatio
                source: slot.iconSource
                mipmap: true
                smooth: true
                visible: !slot.atRest
                opacity: image.opacity
            }
        }

        MouseArea {
            id: mouseArea

            anchors.fill: parent
            // The pointer stays "on" an item right down to the screen edge.
            anchors.bottomMargin: -(slot.dock.padding + slot.dock.bottomGap) / Math.max(slot.zoom, 0.01)
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
            pressAndHoldInterval: 600
        }
    }

    // Open indicator, centred in the padding below the icon. Follows the icon
    // sideways but never scales or bounces, like macOS.
    Rectangle {
        width: 4
        height: 4
        radius: 2
        x: (parent.width - width) / 2 + slot.shift
        y: parent.height + (slot.dock.padding - height) / 2
        color: slot.dock.dark ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(0, 0, 0, 0.7)
        visible: HarbourSettings.showIndicators
        opacity: slot.running && !slot.lifted ? slot.appear : 0

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
    }

    NumberAnimation {
        id: appearAnimation

        target: slot
        property: "appear"
        from: 0
        to: 1
        duration: 280
        easing.type: Easing.OutBack
        easing.overshoot: 1.2
    }

    onLiftedChanged: {
        if (!lifted) {
            settleHome(Qt.point(x + liftPosition.x, liftPosition.y));
        }
    }

    function settleHome(from: point) {
        settleFrom = from;
        settleProgress = 1;
        settleAnimation.restart();
    }

    NumberAnimation {
        id: settleAnimation

        target: slot
        property: "settleProgress"
        to: 0
        duration: 260
        easing.type: Easing.OutCubic
    }

    function popIn() {
        appear = 0;
        appearAnimation.start();
    }
}
