# The animations between states, as bursts of frames: the name label sliding between
# icons, a dropped icon settling, an app quitting, a size change, the fan closing, and
# an app peeking out of the hidden dock for attention.
kwin_script() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$1" "ci-$2")
    qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
    sleep 0.3
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ci-$2" > /dev/null
}
mark() { echo "$(awk '{print $1}' /proc/uptime) === $1" >> "$CI/marks.log"; }
rm -f "$CI/marks.log"
GH=$(pos Ghostty); ST=$(pos Steam); OB=$(pos Obsidian); D=$(pos Downloads)
Y=$((H - 42))
area() { echo 460 820 1000 260; } # the dock and the space above it

# Label: onto Ghostty, then across to Steam.
reveal "$GH"
probe move "$GH" $Y; sleep 0.6
probe move "$ST" $Y &
grab label $(area) 8 25
wait $!

# Drop settle: Obsidian dragged two slots left; frames from the release on.
mark "drag"
probe drag "$OB" $Y $((OB - 120)) $((Y - 60)) 400 &
sleep 0.95
grab settle $(area) 26 30
wait $!
probe move $((W / 2)) $((H / 3)); sleep 1

# Quit: an unpinned app opens, then quits.
printf 'import QtQuick\nWindow { width: 300; height: 200; visible: true; title: "Quitter" }\n' > "$CI/app.qml"
qml6 "$CI/app.qml" > /dev/null 2>&1 &
APP=$!
sleep 2.5
kill $APP
mark "quit"
grab quit $(area) 8 30
sleep 1

# Size: larger, live from the config file.
mark "resize"
kwriteconfig6 --file harbourrc --group Appearance --key IconSize 64
grab resize $(area) 10 30
sleep 1
D=$(pos Downloads)

# Fan: open, then close it by clicking the stack again.
reveal "$D"
probe click "$D" $Y left; sleep 1
probe click "$D" $Y left &
grab fanclose 400 280 1100 800 16 30
wait $!
sleep 0.5

# Peek: a maximised window hides the dock, then Dolphin asks for attention.
printf 'import QtQuick\nWindow { width: 600; height: 400; visible: true; color: "#3a4a5a"; title: "Cover" }\n' > "$CI/cover.qml"
qml6 "$CI/cover.qml" > /dev/null 2>&1 &
COVER=$!
sleep 2
printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.setMaximize(true, true);\n' > "$CI/max.js"
kwin_script "$CI/max.js" max
probe move $((W / 2)) $((H / 3))
sleep 2
mark "attention"
gdbus emit --session --object-path /com/canonical/unity/launcherentry/1 \
    --signal com.canonical.Unity.LauncherEntry.Update "application://org.kde.dolphin.desktop" "{'urgent': <true>}"
grab peek 460 960 1000 120 24 100
sleep 1
# Afterwards the edge must work as before; and pushing it mid-peek reveals the dock.
mark "push after the peek"
reveal $((W / 2)); probe move $((W / 2)) $((H / 3)); sleep 1.5
mark "attention again, push mid-peek"
gdbus emit --session --object-path /com/canonical/unity/launcherentry/1 \
    --signal com.canonical.Unity.LauncherEntry.Update "application://org.kde.dolphin.desktop" "{'urgent': <false>}"
sleep 0.3
gdbus emit --session --object-path /com/canonical/unity/launcherentry/1 \
    --signal com.canonical.Unity.LauncherEntry.Update "application://org.kde.dolphin.desktop" "{'urgent': <true>}"
sleep 0.5
probe move $((W / 2)) $((H - 1)); sleep 0.6
probe move $((W / 2)) $((H / 3)); sleep 1.5
# Fullscreen (a game): no peeking at all.
printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.fullScreen = true;\n' > "$CI/fs.js"
kwin_script "$CI/fs.js" fs
sleep 1
mark "attention over a fullscreen window (expect no peek)"
gdbus emit --session --object-path /com/canonical/unity/launcherentry/1 \
    --signal com.canonical.Unity.LauncherEntry.Update "application://org.kde.dolphin.desktop" "{'urgent': <false>}"
sleep 0.3
gdbus emit --session --object-path /com/canonical/unity/launcherentry/1 \
    --signal com.canonical.Unity.LauncherEntry.Update "application://org.kde.dolphin.desktop" "{'urgent': <true>}"
sleep 2.5
mark "end"
kill $COVER

sort -n "$CI/harbour.log" "$CI/marks.log" | grep -E '===|-> |peek|armed|rendered|warn' | sed 's/AutoHideController:://g'
