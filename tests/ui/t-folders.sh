# Pinning folders: one dropped between the dock's icons becomes a stack, and the
# System Settings page lists it.
kwin_script() {
    local id
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$1" "ci-$2")
    qdbus6 org.kde.KWin /Scripting/Script$id org.kde.kwin.Script.run > /dev/null
    sleep 0.3
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "ci-$2" > /dev/null
}
FOLDER=$CI/demo/Projects
mkdir -p "$FOLDER"

# The drag source, top left and out of the dock's way (and out of its task list).
python3 "$HERE/dragsource.py" "file://$FOLDER" > /dev/null 2>&1 &
SOURCE=$!
sleep 2
printf 'for (const w of workspace.windowList()) if (w.normalWindow) { w.skipTaskbar = true; w.frameGeometry = {x: 100, y: 100, width: 300, height: 200}; }\n' > "$CI/place.js"
kwin_script "$CI/place.js" place
sleep 1.5

# The gap between the last app and the first stack is where the separator sits.
LAST=$(grep geometry "$CI/harbour.log" | tail -1 | grep -oE '[^|]+@[0-9]+,' | grep -vE 'Downloads|Trash' | tail -1 | grep -oE '@[0-9]+' | tr -d @)
D=$(pos Downloads)
SEP=$(( (LAST + D) / 2 ))
echo "separator at $SEP"
probe drag 250 200 "$SEP" $((H - 42)); sleep 1.2
snap dropped "1000x300+460+780"
echo "Stacks=$(kreadconfig6 --file harbourrc --group Items --key Stacks)"
echo "StackViews=$(kreadconfig6 --file harbourrc --group Items --key StackViews)"
kill $SOURCE 2> /dev/null

# The settings page, loaded from the build tree.
QT_PLUGIN_PATH=$BUILD/bin kcmshell6 kcm_harbour > "$CI/kcm.log" 2>&1 &
KCM=$!
sleep 4
printf 'for (const w of workspace.windowList()) if (w.normalWindow) { w.frameGeometry = {x: 200, y: 20, width: 900, height: 860}; }\n' > "$CI/kcm.js"
kwin_script "$CI/kcm.js" kcm
sleep 1
snap kcm

# Move the new folder first, apply; then remove it, apply. (Positions from the page's
# layout at this size: the second row's up arrow, the first row's remove button.)
probe click 853 613; sleep 0.5
probe click 967 855; sleep 1.2   # Apply
echo "after moving: $(kreadconfig6 --file harbourrc --group Items --key Stacks)"
snap kcm-moved
probe click 928 576; sleep 0.5
probe click 967 855; sleep 1.2
echo "after removing: $(kreadconfig6 --file harbourrc --group Items --key Stacks)"
snap removed "1000x300+460+780"
kill $KCM 2> /dev/null
grep -iE 'qml|error|warn' "$CI/kcm.log" | head -5
