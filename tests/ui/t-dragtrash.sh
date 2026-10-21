# Dragging a file over the dock magnifies under it and labels the target; dropped on
# the Trash, it goes to the session's own Trash (never the user's).
kwin_script() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$1" "ci-$2")
    qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
    sleep 0.3
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ci-$2" > /dev/null
}
mkdir -p "$CI/demo"; : > "$CI/demo/file.txt"
python3 "$HERE/dragsource.py" "file://$CI/demo/file.txt" > /dev/null 2>&1 &
SRC=$!
sleep 2
printf 'for (const w of workspace.windowList()) if (w.normalWindow) { w.skipTaskbar = true; w.frameGeometry = {x: 100, y: 100, width: 300, height: 200}; }\n' > "$CI/place.js"
kwin_script "$CI/place.js" place
sleep 1
TR=$(pos Trash)
# Drag over to the Trash and linger there (long move), grabbing while it hovers.
"$PROBE" drag 250 200 "$TR" $((H - 42)) 900 &
sleep 1.6
grab dragzoom 460 820 1000 260 4 60
wait $!
kill $SRC
sleep 1
echo "test Trash: $(ls "$CI/data-home/Trash/files" 2>/dev/null | tr "\n" " ")"
