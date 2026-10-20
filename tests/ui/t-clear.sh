# ShowWhenClear: up with an empty desktop, away under a maximised window, back when it goes.
kwin_script() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$1" "ci-$2")
    qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
    sleep 0.3
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ci-$2" > /dev/null
}
mark() { echo "$(awk '{print $1}' /proc/uptime) === $1" >> "$CI/marks.log"; }
rm -f "$CI/marks.log"

probe move $((W / 2)) $((H / 3))
sleep 1.5
mark "empty desktop"
snap clear-empty

dolphin --new-window > /dev/null 2>&1 &
DOLPHIN=$!
sleep 2.5
printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.setMaximize(true, true);\n' > "$CI/max.js"
kwin_script "$CI/max.js" max
mark "maximised window"
sleep 1.5
snap clear-covered

printf 'for (const w of workspace.windowList()) if (w.normalWindow) w.minimized = true;\n' > "$CI/min.js"
kwin_script "$CI/min.js" min
mark "window minimised"
sleep 1.5
snap clear-again
kill $DOLPHIN 2> /dev/null

sort -n "$CI/harbour.log" "$CI/marks.log" | grep -E '===|-> ' | sed 's/AutoHideController:://g'
