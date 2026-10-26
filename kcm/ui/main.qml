import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

// The Dock page: how it looks, how it behaves, what it holds; timing folded away.
KCM.SimpleKCM {
    id: root

    readonly property var settings: kcm.settings

    Kirigami.FormLayout {
        // ── Look ──────────────────────────────────────────────────────────────

        RowLayout {
            Kirigami.FormData.label: i18nc("@label:slider", "Size:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label { text: i18nc("@item:inrange", "Small") }
            QQC2.Slider {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                from: 32
                to: 96
                // Rounded here rather than with stepSize, which draws a crowd of ticks.
                value: root.settings.iconSize
                onMoved: {
                    root.settings.iconSize = Math.round(value / 2) * 2;
                    kcm.preview("iconSize", root.settings.iconSize);
                }

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "iconSize"
                }
            }
            QQC2.Label { text: i18nc("@item:inrange", "Large") }
            QQC2.Label {
                text: i18nc("icon size in pixels", "%1 px", root.settings.iconSize)
                opacity: 0.7
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18nc("@label:slider", "Magnification:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.CheckBox {
                id: magnification
                checked: root.settings.magnification
                onToggled: {
                    root.settings.magnification = checked;
                    kcm.preview("magnification", checked);
                }

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "magnification"
                }
            }
            QQC2.Label { text: i18nc("@item:inrange", "Small") }
            QQC2.Slider {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                enabled: magnification.checked
                from: Math.max(48, root.settings.iconSize + 8)
                to: 192
                // Rounded here rather than with stepSize, which draws a crowd of ticks.
                value: root.settings.magnifiedIconSize
                onMoved: {
                    root.settings.magnifiedIconSize = Math.round(value / 4) * 4;
                    kcm.preview("magnifiedIconSize", root.settings.magnifiedIconSize);
                }

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "magnifiedIconSize"
                }
            }
            QQC2.Label { text: i18nc("@item:inrange", "Large") }
            QQC2.Label {
                text: i18nc("icon size in pixels", "%1 px", root.settings.magnifiedIconSize)
                opacity: magnification.checked ? 0.7 : 0.35
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18nc("@label:slider", "Glass:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label { text: i18nc("@item:inrange glass appearance", "Clear") }
            QQC2.Slider {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                from: 0
                to: 100
                // Rounded here rather than with stepSize, which draws a crowd of ticks.
                value: root.settings.glassOpacity
                onMoved: {
                    root.settings.glassOpacity = Math.round(value / 5) * 5;
                    kcm.preview("glassOpacity", root.settings.glassOpacity);
                }

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "glassOpacity"
                }
            }
            QQC2.Label { text: i18nc("@item:inrange glass appearance", "Tinted") }
        }

        RowLayout {
            spacing: 0

            QQC2.CheckBox {
                text: i18nc("@option:check", "Same physical size on every screen")
                checked: root.settings.matchPhysicalSize
                onToggled: root.settings.matchPhysicalSize = checked


                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "matchPhysicalSize"
                }
            }
            Kirigami.ContextualHelpButton {
                toolTipText: i18nc("@info:tooltip", "Screens with fewer pixels per inch get a proportionally smaller dock, so it looks the same size as on the main screen. The sizes above are for the main screen.")
            }
        }

        // ── Behaviour ─────────────────────────────────────────────────────────

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Behaviour")
        }

        RowLayout {
            Kirigami.FormData.label: i18nc("@title:group", "Hiding:")
            spacing: 0

            QQC2.CheckBox {
                text: i18nc("@option:check", "Automatically hide and show the Dock")
                checked: root.settings.autoHide
                onToggled: root.settings.autoHide = checked

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "autoHide"
                }
            }
            Kirigami.ContextualHelpButton {
                toolTipText: i18nc("@info:tooltip", "Meta+Alt+D turns this on and off from anywhere.")
            }
        }

        QQC2.CheckBox {
            text: i18nc("@option:check", "Keep it shown while no window is in the way")
            enabled: root.settings.autoHide
            checked: root.settings.showWhenClear
            onToggled: root.settings.showWhenClear = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "showWhenClear"
                extraEnabledConditions: root.settings.autoHide
            }
        }

        RowLayout {
            spacing: 0

            QQC2.CheckBox {
                text: i18nc("@option:check", "Show the Dock over fullscreen windows")
                checked: root.settings.revealOverFullscreen
                onToggled: root.settings.revealOverFullscreen = checked

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "revealOverFullscreen"
                }
            }
            Kirigami.ContextualHelpButton {
                toolTipText: i18nc("@info:tooltip", "Rest the pointer on the bottom edge of the screen to bring the Dock up over a fullscreen game or video. Off, it stays out of the way until the window leaves fullscreen.")
            }
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Show the Dock on:")
            textRole: "text"
            valueRole: "value"
            model: [
                { text: i18nc("@item:inlistbox", "Every screen"), value: "follow" },
                { text: i18nc("@item:inlistbox", "Only the screen with the pointer"), value: "cursor" },
                { text: i18nc("@item:inlistbox", "Only the main screen"), value: "primary" },
            ]
            currentIndex: Math.max(0, model.findIndex(entry => entry.value === root.settings.screenMode))
            onActivated: root.settings.screenMode = currentValue

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "screenMode"
            }
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Minimise windows using:")
            enabled: kcm.genieInstalled
            textRole: "text"
            valueRole: "value"
            model: [
                { text: i18nc("@item:inlistbox minimise animation", "Genie effect"), value: "Genie" },
                { text: i18nc("@item:inlistbox minimise animation", "Scale effect"), value: "Scale" },
                { text: i18nc("@item:inlistbox minimise animation", "Suck effect"), value: "Suck" },
                { text: i18nc("@item:inlistbox minimise animation", "KDE's own animation"), value: "Default" },
            ]
            currentIndex: Math.max(0, model.findIndex(entry => entry.value === kcm.minimizeEffect))
            onActivated: kcm.minimizeEffect = currentValue
        }

        QQC2.Label {
            visible: !kcm.genieInstalled
            text: i18nc("@info", "Install the Harbour Genie effect for KWin to choose a minimise animation here.")
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            font: Kirigami.Theme.smallFont
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("@title:group", "Icons:")
            text: i18nc("@option:check", "Show indicators for open applications")
            checked: root.settings.showIndicators
            onToggled: root.settings.showIndicators = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "showIndicators"
            }
        }

        QQC2.CheckBox {
            text: i18nc("@option:check", "Animate opening applications")
            checked: root.settings.animateOpening
            onToggled: root.settings.animateOpening = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "animateOpening"
            }
        }

        RowLayout {
            spacing: 0

            QQC2.CheckBox {
                text: i18nc("@option:check", "Even out square app icons")
                checked: root.settings.evenTiles
                onToggled: root.settings.evenTiles = checked

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "evenTiles"
                }
            }
            Kirigami.ContextualHelpButton {
                toolTipText: i18nc("@info:tooltip", "App icons drawn as rounded squares come in many sizes and corner shapes. This draws them all at one size with the same corners. Round and other shaped icons are left as they are.")
            }
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("@title:group", "Clicking:")
            text: i18nc("@option:check", "Clicking the active app's icon minimises its windows")
            checked: root.settings.clickToMinimize
            onToggled: root.settings.clickToMinimize = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "clickToMinimize"
            }
        }

        // ── Items ─────────────────────────────────────────────────────────────

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Folders and Trash")
        }

        ColumnLayout {
            id: folders

            Kirigami.FormData.label: i18nc("@label", "Folders:")
            Kirigami.FormData.labelAlignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.gridUnit * 24
            spacing: Kirigami.Units.smallSpacing

            readonly property var stacks: root.settings.stacks
            readonly property var views: {
                const views = root.settings.stackViews.slice(0, stacks.length);
                while (views.length < stacks.length) {
                    views.push("fan");
                }
                return views;
            }

            function update(stacks, views) {
                root.settings.stacks = stacks;
                root.settings.stackViews = views;
            }

            function add(path: string) {
                if (path !== "" && !stacks.includes(path)) {
                    update(stacks.concat([path]), views.concat(["fan"]));
                }
            }

            function remove(index: int) {
                const stacks = folders.stacks.slice();
                const views = folders.views.slice();
                stacks.splice(index, 1);
                views.splice(index, 1);
                update(stacks, views);
            }

            function move(from: int, to: int) {
                const stacks = folders.stacks.slice();
                const views = folders.views.slice();
                stacks.splice(to, 0, stacks.splice(from, 1)[0]);
                views.splice(to, 0, views.splice(from, 1)[0]);
                update(stacks, views);
            }

            function setView(index: int, view: string) {
                const views = folders.views.slice();
                views[index] = view;
                update(stacks.slice(), views);
            }

            Repeater {
                model: folders.stacks

                delegate: RowLayout {
                    id: folderRow

                    required property string modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: kcm.stackIcon(folderRow.modelData)
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: folderRow.modelData
                        elide: Text.ElideMiddle
                    }
                    QQC2.ComboBox {
                        textRole: "text"
                        valueRole: "value"
                        model: [
                            { text: i18nc("@item:inlistbox stack view", "Fan"), value: "fan" },
                            { text: i18nc("@item:inlistbox stack view", "Grid"), value: "grid" },
                            { text: i18nc("@item:inlistbox stack view", "List"), value: "list" },
                        ]
                        currentIndex: Math.max(0, model.findIndex(entry => entry.value === folders.views[folderRow.index]))
                        onActivated: folders.setView(folderRow.index, currentValue)
                    }
                    QQC2.ToolButton {
                        icon.name: "go-up"
                        text: i18nc("@action:button", "Move left in the Dock")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: folderRow.index > 0
                        onClicked: folders.move(folderRow.index, folderRow.index - 1)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "go-down"
                        text: i18nc("@action:button", "Move right in the Dock")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: folderRow.index < folders.stacks.length - 1
                        onClicked: folders.move(folderRow.index, folderRow.index + 1)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        text: i18nc("@action:button", "Remove from the Dock")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: folders.remove(folderRow.index)
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }

            QQC2.Label {
                visible: folders.stacks.length === 0
                text: i18nc("@info", "No folders in the Dock.")
                opacity: 0.7
            }

            QQC2.Button {
                text: i18nc("@action:button", "Add Folder…")
                icon.name: "list-add"
                onClicked: folderDialog.open()
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: i18nc("@info", "You can also drop a folder between the Dock's icons, or remove one from its right-click menu.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                opacity: 0.7
            }

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "stacks"
            }
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18nc("@label", "Trash:")
            text: i18nc("@option:check", "Show the Trash")
            checked: root.settings.showTrash
            onToggled: root.settings.showTrash = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "showTrash"
            }
        }

        // ── Advanced: timing, folded away ─────────────────────────────────────

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: ""
        }

        QQC2.Button {
            id: advanced

            Kirigami.FormData.label: i18nc("@label", "Advanced:")
            text: checked ? i18nc("@action:button", "Hide Timing Options") : i18nc("@action:button", "Show Timing Options")
            icon.name: checked ? "arrow-up" : "arrow-down"
            checkable: true
        }

        QQC2.SpinBox {
            visible: advanced.checked
            Kirigami.FormData.label: i18nc("@label:spinbox", "Delay before showing:")
            from: 0
            to: 2000
            stepSize: 50
            value: root.settings.revealDelay
            onValueModified: root.settings.revealDelay = value
            textFromValue: (v, locale) => i18nc("milliseconds", "%1 ms", v)
            valueFromText: (text, locale) => parseInt(text)

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "revealDelay"
            }
        }

        QQC2.SpinBox {
            visible: advanced.checked
            Kirigami.FormData.label: i18nc("@label:spinbox", "Delay before hiding:")
            from: 0
            to: 5000
            stepSize: 50
            value: root.settings.hideDelay
            onValueModified: root.settings.hideDelay = value
            textFromValue: (v, locale) => i18nc("milliseconds", "%1 ms", v)
            valueFromText: (text, locale) => parseInt(text)

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "hideDelay"
            }
        }

        QQC2.SpinBox {
            visible: advanced.checked
            Kirigami.FormData.label: i18nc("@label:spinbox", "Show animation:")
            from: 0
            to: 2000
            stepSize: 25
            value: root.settings.revealDuration
            onValueModified: root.settings.revealDuration = value
            textFromValue: (v, locale) => i18nc("milliseconds", "%1 ms", v)
            valueFromText: (text, locale) => parseInt(text)

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "revealDuration"
            }
        }

        QQC2.SpinBox {
            visible: advanced.checked
            Kirigami.FormData.label: i18nc("@label:spinbox", "Hide animation:")
            from: 0
            to: 2000
            stepSize: 25
            value: root.settings.hideDuration
            onValueModified: root.settings.hideDuration = value
            textFromValue: (v, locale) => i18nc("milliseconds", "%1 ms", v)
            valueFromText: (text, locale) => parseInt(text)

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "hideDuration"
            }
        }
    }

    FolderDialog {
        id: folderDialog

        title: i18nc("@title:window", "Add a Folder to the Dock")
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        onAccepted: folders.add(kcm.stackPath(selectedFolder))
    }
}
