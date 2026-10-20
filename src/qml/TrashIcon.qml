import QtQuick
import Harbour

// The Trash, always last: opens it, empties it, and takes files dropped on it.
DockSlot {
    id: trash

    readonly property bool dropTarget: dock.dropTarget === trash

    iconSource: DockTools.iconUrl(Trash.empty ? "user-trash" : "user-trash-full")
    label: qsTr("Trash")
    iconOpacity: dropTarget ? 0.65 : 1

    function acceptDrop(urls: list<url>) {
        Trash.moveToTrash(urls);
    }

    Connections {
        target: trash.mouseArea

        function onPressed(mouse) {
            if (mouse.button === Qt.RightButton) {
                // A click that just closed another menu opens ours once released (see Dock).
                if (trash.dock.menuJustClosed()) {
                    trash.dock.openMenuAfterRelease(trash);
                } else {
                    trash.openMenu();
                }
            }
        }

        function onPressAndHold(mouse) {
            if (mouse.button === Qt.LeftButton) {
                trash.openMenu();
            }
        }

        function onClicked(mouse) {
            if (mouse.button === Qt.LeftButton) {
                Trash.open();
            }
        }
    }

    function contextMenu() {
        openMenu();
    }

    function openMenu() {
        if (!trash.dock.shouldOpenMenu(trash)) {
            return;
        }
        const menu = menuComponent.createObject(trash.dock, { dock: trash.dock });
        menu.showAbove(trash);
    }

    Component {
        id: menuComponent

        DockMenu {
            function build() {
                addEntry(qsTr("Open"), "document-open", () => Trash.open());
                addSeparator();
                addEntry(qsTr("Empty Trash…"), "trash-empty", () => Trash.emptyTrash(), { enabled: !Trash.empty });
            }
        }
    }
}
