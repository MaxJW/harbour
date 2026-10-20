import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

// The Dock page, in the order of macOS's Desktop & Dock settings.
KCM.SimpleKCM {
    id: root

    readonly property var settings: kcm.settings

    Kirigami.FormLayout {
        // ── Size and look ─────────────────────────────────────────────────────
        RowLayout {
            Kirigami.FormData.label: i18nc("@label:slider", "Size:")
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label { text: i18nc("@item:inrange", "Small") }
            QQC2.Slider {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                from: 32
                to: 96
                stepSize: 2
                value: root.settings.iconSize
                onMoved: {
                    root.settings.iconSize = value;
                    kcm.preview("iconSize", value);
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
                stepSize: 4
                value: root.settings.magnifiedIconSize
                onMoved: {
                    root.settings.magnifiedIconSize = value;
                    kcm.preview("magnifiedIconSize", value);
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
                stepSize: 5
                value: root.settings.glassOpacity
                onMoved: {
                    root.settings.glassOpacity = value;
                    kcm.preview("glassOpacity", value);
                }

                KCM.SettingStateBinding {
                    configObject: root.settings
                    settingName: "glassOpacity"
                }
            }
            QQC2.Label { text: i18nc("@item:inrange glass appearance", "Tinted") }
        }

        Item {
            Kirigami.FormData.isSection: true
        }

        // ── Behaviour ─────────────────────────────────────────────────────────
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
            Kirigami.FormData.label: i18nc("@title:group", "Dock:")
            text: i18nc("@option:check", "Automatically hide and show the Dock")
            checked: root.settings.autoHide
            onToggled: root.settings.autoHide = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "autoHide"
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

        QQC2.Label {
            text: i18nc("@info", "Meta+Alt+D turns this on and off.")
            font: Kirigami.Theme.smallFont
            opacity: 0.7
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

        QQC2.CheckBox {
            text: i18nc("@option:check", "Show indicators for open applications")
            checked: root.settings.showIndicators
            onToggled: root.settings.showIndicators = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "showIndicators"
            }
        }

        QQC2.CheckBox {
            text: i18nc("@option:check", "Show the Trash")
            checked: root.settings.showTrash
            onToggled: root.settings.showTrash = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "showTrash"
            }
        }

        QQC2.CheckBox {
            text: i18nc("@option:check", "Clicking the active app's icon minimises its windows")
            checked: root.settings.clickToMinimize
            onToggled: root.settings.clickToMinimize = checked

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "clickToMinimize"
            }
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: i18nc("@label:listbox", "Show the Dock on:")
            textRole: "text"
            valueRole: "value"
            model: [
                { text: i18nc("@item:inlistbox", "The screen you move the pointer to"), value: "follow" },
                { text: i18nc("@item:inlistbox", "The main screen only"), value: "primary" },
            ]
            currentIndex: Math.max(0, model.findIndex(entry => entry.value === root.settings.screenMode))
            onActivated: root.settings.screenMode = currentValue

            KCM.SettingStateBinding {
                configObject: root.settings
                settingName: "screenMode"
            }
        }

        Item {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18nc("@title:group", "Timing")
        }

        QQC2.SpinBox {
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
}
