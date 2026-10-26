# Over a fullscreen window the dock stays hidden (even with auto-hide off). Resting the
# pointer on the bottom row brings it up over the window only with RevealOverFullscreen
# on (off by default). KWin turns client screen edges off under fullscreen windows;
# Harbour's KWin script reports the bottom row.
kwin_script() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$1" "ci-$2")
    qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
    sleep 0.3
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ci-$2" > /dev/null
}
mark() { echo "$(awk '{print $1}' /proc/uptime) === $1" >> "$CI/marks.log"; }
dock() {  # anything bright along the bottom (the game window is a dull blue-grey)
    snap "$1"
    [ "$(magick "$SHOTS/$1.png" -crop "1920x70+0+$((H - 70))" +repage -format '%[fx:maxima.r>0.5?1:0]' info:)" = 1 ] \
        && echo "$1: dock up" || echo "$1: dock hidden"
}
rm -f "$CI/marks.log"
printf 'import QtQuick\nWindow { width: 600; height: 400; visible: true; color: "#3a4a5a"; title: "Game" }\n' > "$CI/game.qml"
qml6 "$CI/game.qml" > /dev/null 2>&1 &
GAME=$!
sleep 2
printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.fullScreen = true;\n' > "$CI/fs.js"
kwin_script "$CI/fs.js" fs
probe move $((W / 2)) $((H / 3)); sleep 1.5
mark "fullscreen, pointer away"
dock away

mark "rest on the bottom row, RevealOverFullscreen off"
kwriteconfig6 --file harbourrc --group Behaviour --key RevealOverFullscreen false
sleep 0.5
probe move $((W / 2)) $((H - 1)); sleep 0.8
dock pushed-setting-off
probe move $((W / 2)) $((H / 3)); sleep 1.5

mark "rest on the bottom row, RevealOverFullscreen on"
kwriteconfig6 --file harbourrc --group Behaviour --key RevealOverFullscreen true
sleep 0.5
probe move $((W / 2)) $((H - 1)); sleep 0.8
dock pushed
probe move $((W / 2)) $((H / 3)); sleep 1.5
mark "away again"
dock away-again

mark "auto-hide off, still fullscreen"
kwriteconfig6 --file harbourrc --group Behaviour --key AutoHide false
sleep 1.5
dock autohide-off
probe move $((W / 2)) $((H - 1)); sleep 0.8
dock autohide-off-pushed
probe move $((W / 2)) $((H / 3)); sleep 1.5
dock autohide-off-away

mark "leave fullscreen (auto-hide off: dock stays up)"
kill $GAME; sleep 1.5
dock windowed
sort -n "$CI/harbour.log" "$CI/marks.log" | grep -E '===|-> |fullscreen|pushed' | sed 's/AutoHideController:://g'
