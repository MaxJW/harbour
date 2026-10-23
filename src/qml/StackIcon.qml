import QtQuick
import Harbour

// A folder after the separator (Downloads by default). Clicking opens its newest
// files as a fan, grid or list; files dropped on it are moved into the folder.
DockSlot {
    id: stack

    required property string folderPath
    required property int stackIndex
    property string view: "fan"

    readonly property url folderUrl: DockTools.folderUrl(folderPath)
    readonly property bool dropTarget: dock.dropTarget === stack

    iconSource: DockTools.iconUrl(DockTools.iconNameForUrl(folderUrl), false, DockTools.iconThemeRevision)
    label: folderPath.replace(/\/+$/, "").split("/").pop() || folderPath
    iconOpacity: dropTarget ? 0.65 : 1

    function acceptDrop(urls: list<url>) {
        DockTools.moveToFolder(urls, folderUrl);
    }

    function setView(newView: string) {
        const views = HarbourSettings.stackViews.slice();
        while (views.length <= stackIndex) {
            views.push("fan");
        }
        views[stackIndex] = newView;
        HarbourSettings.stackViews = views;
        HarbourSettings.save();
    }

    function remove() {
        const stacks = HarbourSettings.stacks.slice();
        const views = HarbourSettings.stackViews.slice();
        stacks.splice(stackIndex, 1);
        views.splice(stackIndex, 1);
        HarbourSettings.stacks = stacks;
        HarbourSettings.stackViews = views;
        HarbourSettings.save();
    }

    Connections {
        target: stack.mouseArea

        function onPressed(mouse) {
            if (mouse.button === Qt.RightButton) {
                // A click that just closed another menu opens ours once released (see Dock).
                if (stack.dock.menuJustClosed()) {
                    stack.dock.openMenuAfterRelease(stack);
                } else {
                    stack.openMenu();
                }
            }
        }

        function onClicked(mouse) {
            if (mouse.button === Qt.LeftButton && stack.dock.shouldOpenMenu(stack)) {
                const popup = popupComponent.createObject(stack.dock, { dock: stack.dock, stack: stack });
                popup.showAbove();
            }
        }
    }

    function contextMenu() {
        openMenu();
    }

    function openMenu() {
        if (!stack.dock.shouldOpenMenu(stack)) {
            return;
        }
        const menu = menuComponent.createObject(stack.dock, { dock: stack.dock });
        menu.showAbove(stack);
    }

    Component {
        id: popupComponent

        StackPopup {}
    }

    Component {
        id: menuComponent

        DockMenu {
            function build() {
                addEntry(qsTr("Open in Dolphin"), "system-file-manager", () => DockTools.openUrl(stack.folderUrl));
                addSeparator();
                const view = stack.view === "grid" || stack.view === "list" ? stack.view : "fan";
                addEntry(qsTr("View as Fan"), "", () => stack.setView("fan"), { checkable: true, checked: view === "fan" });
                addEntry(qsTr("View as Grid"), "view-grid", () => stack.setView("grid"), { checkable: true, checked: view === "grid" });
                addEntry(qsTr("View as List"), "view-list-details", () => stack.setView("list"), { checkable: true, checked: view === "list" });
                addSeparator();
                addEntry(qsTr("Remove from Dock"), "list-remove", () => stack.remove());
            }
        }
    }
}
