// Loaded into KWin by Harbour for a moment, to ask where the pointer is: after a menu
// or stack closes with the pointer resting where it was, nothing else says so (see
// AutoHideController::release).
callDBus("dev.harbour.dock", "/Dock", "dev.harbour.Dock", "cursorAt", workspace.cursorPos.x, workspace.cursorPos.y);
