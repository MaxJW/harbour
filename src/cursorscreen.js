// Loaded into KWin by Harbour (ScreenMode "cursor"): tells the dock which screen the
// pointer is on, since Wayland clients only see the pointer over their own windows.
// Only changes are sent, so moving within a screen costs a comparison.
let lastScreen = "";

function update() {
    const output = workspace.screenAt(workspace.cursorPos);
    const name = output ? output.name : "";
    if (name !== lastScreen) {
        lastScreen = name;
        callDBus("dev.harbour.dock", "/Dock", "dev.harbour.Dock", "cursorScreen", name);
    }
}

workspace.cursorPosChanged.connect(update);
update();
