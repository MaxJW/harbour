import QtQuick
import Harbour

// A dock menu: a native KDE context menu (NativeMenu, the same kind Plasma's desktop
// uses), centred above the item it belongs to, holding the dock open while shown.
// Subclasses fill it in build(); call showAbove() to open it.
NativeMenu {
    id: menu

    required property Item dock
    // The dock item the menu belongs to (set by showAbove()).
    property Item owner: null

    function build() {
    }

    // options: enabled, bold, checkable, checked. addSeparator() and addMenu() come
    // from NativeMenu.
    function addEntry(text: string, iconName: string, onTriggered: var, options = ({})) {
        addAction(text, iconName, onTriggered, options);
    }

    function showAbove(item: Item) {
        owner = item;
        build();
        popupAbove(item, 8);
    }

    onAboutToShow: {
        dock.currentMenu = menu;
        dock.menuOpen = true;
        dock.autoHide.hold();
    }
    onAboutToHide: {
        if (dock.currentMenu === menu) {
            dock.currentMenu = null;
            dock.menuOpen = false;
        }
        dock.autoHide.release();
        dock.noteMenuClosed(owner);
        // A triggered action runs after the menu hides; let it, and Qt, finish first.
        destroy(1000);
    }
}
