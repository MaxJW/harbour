pragma Singleton

import QtQuick
import Harbour

// Fills a stack's list menu (a NativeMenu) like a normal KDE context menu: the newest
// few entries, folders as submenus filled when first opened, then "Open in Dolphin".
QtObject {
    // Long names are shortened in the middle, keeping the start and the extension.
    function shortName(name: string): string {
        return name.length <= 48 ? name : name.slice(0, 30) + "…" + name.slice(-15);
    }

    function fill(menu: var, folderUrl: url, limit: int) {
        const entries = DockTools.folderEntries(folderUrl, limit);
        for (const entry of entries) {
            const url = entry.url;
            const icon = DockTools.iconNameForUrl(url);
            if (entry.isDir) {
                const submenu = menu.addMenu(shortName(entry.name), icon);
                let filled = false;
                submenu.aboutToShow.connect(() => {
                    if (!filled) {
                        filled = true;
                        fill(submenu, url, limit);
                    }
                });
            } else {
                menu.addAction(shortName(entry.name), icon, () => DockTools.openUrl(url), {});
            }
        }
        if (entries.length === 0) {
            menu.addAction(qsTr("Empty"), "", null, { enabled: false });
        }
        menu.addSeparator();
        menu.addAction(qsTr("Open in Dolphin"), "system-file-manager", () => DockTools.openUrl(folderUrl), {});
    }
}
