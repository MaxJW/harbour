// Loaded into KWin by Harbour. Wayland clients only see the pointer over their own
// windows, so this tells the dock what it needs to know about it:
//  - which screen it's on (for ScreenMode "cursor"), and
//  - when it's on a screen's bottom row, so a push can still bring the dock up over a
//    fullscreen window (KWin turns client screen edges off under those).
// Only changes are sent, so each movement costs a couple of comparisons. But following
// the pointer at all runs this inside KWin for every movement, so it only does while
// it's needed: in ScreenMode "cursor", or while a window is fullscreen.

// Set by Harbour when it writes this file out: whether ScreenMode is "cursor".
const followPointer = false;

let lastScreen = null;
let lastAtEdge = false;
let tracking = false;
const fullscreen = new Set();

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

function retrack() {
    const want = followPointer || fullscreen.size > 0;
    if (want === tracking) {
        return;
    }
    tracking = want;
    if (want) {
        workspace.cursorPosChanged.connect(update);
        update();
    } else {
        workspace.cursorPosChanged.disconnect(update);
        if (lastAtEdge) {
            lastAtEdge = false;
            callDBus("dev.harbour.dock", "/Dock", "dev.harbour.Dock", "pointerAtEdge", lastScreen || "", false);
        }
    }
}

function watch(window) {
    const changed = () => {
        if (window.fullScreen) {
            fullscreen.add(window);
        } else {
            fullscreen.delete(window);
        }
        retrack();
    };
    window.fullScreenChanged.connect(changed);
    changed();
}

workspace.windowAdded.connect(watch);
workspace.windowRemoved.connect(window => {
    fullscreen.delete(window);
    retrack();
});
workspace.windowList().forEach(watch);
retrack();
