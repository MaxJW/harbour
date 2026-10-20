# The plate over a busy window: the tint should be even, with no dark band.
dolphin --new-window /usr/share/icons > /dev/null 2>&1 &
DOLPHIN=$!
sleep 2.5
printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.setMaximize(true, true);\n' > "$CI/max.js"
id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$CI/max.js" ci-max)
qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
sleep 1
probe move $((W / 2)) $((H / 3))
qdbus6 dev.harbour.dock /Dock dev.harbour.Dock.reveal "" > /dev/null
sleep 0.8
snap glass "1100x160+410+920"
kill $DOLPHIN 2> /dev/null
