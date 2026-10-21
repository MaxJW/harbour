import QtQuick
import org.kde.taskmanager as TaskManager
import Harbour

// An app's menu, styled and ordered like a KDE context menu: its windows (when it
// has several), its own actions, then New Window / Pin, then Show All Windows,
// Minimise and Quit.
DockMenu {
    id: menu

    required property DockIcon dockIcon
    required property TaskManager.TasksModel tasksModel

    // The icon can go before the menu does (it's destroyed a moment after closing).
    readonly property bool running: dockIcon?.running ?? false
    readonly property url launcherUrl: dockIcon?.model.LauncherUrlWithoutIcon ?? ""

    function build() {
        const windows = dock.windowIndexes(dockIcon);
        const appIcon = DockTools.iconNameForUrl(launcherUrl);

        if (windows.length > 1) {
            for (const w of windows) {
                const title = dock.role(w, Qt.DisplayRole) || dockIcon.model.AppName;
                addEntry(title, appIcon, () => tasksModel.requestActivate(w),
                         { bold: dock.role(w, TaskManager.AbstractTasksModel.IsActive) === true });
            }
            addSeparator();
        }

        for (const action of DockTools.desktopActions(launcherUrl)) {
            if (action.separator) {
                addSeparator();
            } else {
                addEntry(action.text, action.icon, () => DockTools.runDesktopAction(launcherUrl, action.index));
            }
        }
        addSeparator();

        if (!running) {
            addEntry(qsTr("Open"), "document-open", () => dock.activate(dockIcon));
        } else if (dockIcon.model.CanLaunchNewInstance) {
            addEntry(qsTr("New Window"), "window-new", () => dock.newInstance(dockIcon));
        }
        if (!dock.isPermanent(dockIcon)) {
            if (dockIcon.model.HasLauncher) {
                addEntry(qsTr("Unpin from Dock"), "window-unpin", () => {
                    dockIcon.leaving = true;
                    tasksModel.requestRemoveLauncher(launcherUrl);
                });
            } else {
                addEntry(qsTr("Pin to Dock"), "window-pin", () => tasksModel.requestAddLauncher(launcherUrl));
            }
        }

        if (running) {
            addSeparator();
            addEntry(qsTr("Show All Windows"), "window", () => {
                const ids = [];
                for (const w of windows) {
                    ids.push(...(dock.role(w, TaskManager.AbstractTasksModel.WinIdList) || []));
                }
                DockTools.showWindows(ids);
            });
            addEntry(qsTr("Minimise"), "window-minimize", () => {
                for (const w of windows) {
                    if (!dock.role(w, TaskManager.AbstractTasksModel.IsMinimized)) {
                        tasksModel.requestToggleMinimized(w);
                    }
                }
            });
            addEntry(qsTr("Quit"), "application-exit", () => {
                for (const w of windows) {
                    tasksModel.requestClose(w);
                }
            });
        }
    }
}
