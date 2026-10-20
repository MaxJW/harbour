import QtQuick
import org.kde.taskmanager as TaskManager
import Harbour

// State shared by the docks on every screen: one tasks model (with the pinned
// launchers kept in step with the settings), plus desktop and activity info.
QtObject {
    id: shared

    // Dolphin plays Finder's part: always first, can't be removed.
    readonly property string permanentLauncher: "applications:org.kde.dolphin.desktop"

    // Views attach only once the initial launcher churn is over.
    property bool ready: false

    readonly property TaskManager.VirtualDesktopInfo virtualDesktopInfo: TaskManager.VirtualDesktopInfo {}
    readonly property TaskManager.ActivityInfo activityInfo: TaskManager.ActivityInfo {}

    readonly property TaskManager.TasksModel tasksModel: TaskManager.TasksModel {
        groupMode: TaskManager.TasksModel.GroupApplications
        groupInline: false
        sortMode: TaskManager.TasksModel.SortManual
        // With separate launchers on, launchInPlace sorts each app's windows into its
        // launcher's slot (libtaskmanager only applies it in that mode).
        separateLaunchers: true
        launchInPlace: true
        hideActivatedLaunchers: true
        taskReorderingEnabled: true
        // Like macOS: every running app is in the dock, whichever desktop or screen it's on.
        filterByVirtualDesktop: false
        filterByScreen: false
        filterByActivity: true
        activity: shared.activityInfo.currentActivity

        onLauncherListChanged: {
            if (JSON.stringify(launcherList) !== JSON.stringify(HarbourSettings.launchers)) {
                HarbourSettings.launchers = launcherList;
                HarbourSettings.save();
            }
        }

        Component.onCompleted: {
            launcherList = shared.withPermanentLauncher(HarbourSettings.launchers);
            shared.ready = true;
        }
    }

    readonly property Connections settingsConnection: Connections {
        target: HarbourSettings

        function onLaunchersChanged() {
            const wanted = shared.withPermanentLauncher(HarbourSettings.launchers);
            if (JSON.stringify(wanted) !== JSON.stringify(shared.tasksModel.launcherList)) {
                shared.tasksModel.launcherList = wanted;
            }
        }
    }

    function withPermanentLauncher(launchers: list<string>): list<string> {
        const rest = launchers.filter(url => url !== permanentLauncher);
        return [permanentLauncher].concat(rest);
    }
}
