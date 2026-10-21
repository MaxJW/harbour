// Loaded into KWin by Harbour. Wayland clients only see the pointer over their own
// windows, so this tells the dock what it needs to know about it:
//  - which screen it's on (for ScreenMode "cursor"), and
//  - when it's on a screen's bottom row, so a push can still bring the dock up over a
//    fullscreen window (KWin turns client screen edges off under those).
// Only changes are sent, so moving the pointer costs a couple of comparisons.
let lastScreen = null;
let lastAtEdge = false;

function update() {
    const pos = workspace.cursorPos;
    const output = workspace.screenAt(pos);
    const name = output ? output.name : "";
    if (name !== lastScreen) {
        lastScreen = name;
        callDBus("dev.harbour.dock", "/Dock", "dev.harbour.Dock", "cursorScreen", name);
    }
    const atEdge = output !== null && pos.y >= output.geometry.y + output.geometry.height - 1;
    if (atEdge !== lastAtEdge) {
        lastAtEdge = atEdge;
        callDBus("dev.harbour.dock", "/Dock", "dev.harbour.Dock", "pointerAtEdge", name, atEdge);
    }
}

workspace.cursorPosChanged.connect(update);
update();
