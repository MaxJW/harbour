# Two screens. (1) ScreenMode "cursor": with nothing in the way, only the dock on the
# pointer's screen is up, and it follows the pointer across. (2) Launching an app
# bounces only the dock it was clicked on.  Run with: harness.sh t-screens.sh 2
docks() {  # is each screen's dock up? (bright pixels in its bottom strip)
    snap "$1"
    local a b
    a=$(magick "$SHOTS/$1.png" -crop "1920x70+0+$((H - 70))" +repage -format '%[fx:maxima.r>0.5?1:0]' info:)
    b=$(magick "$SHOTS/$1.png" -crop "1920x70+1920+$((H - 70))" +repage -format '%[fx:maxima.r>0.5?1:0]' info:)
    echo "$1: left dock $([ "$a" = 1 ] && echo up || echo down), right dock $([ "$b" = 1 ] && echo up || echo down)"
}
kwriteconfig6 --file harbourrc --group Behaviour --key ScreenMode cursor
probe move 960 400; sleep 2
docks cursor-left
probe move $((1920 + 960)) 400; sleep 1.5
docks cursor-right
probe move 960 400; sleep 1.5
docks cursor-back

# (2) Every screen, both docks up; a pinned test app launched from the left dock.
printf 'import QtQuick\nWindow { width: 300; height: 200; visible: true; title: "Bounce test" }\n' > "$CI/app.qml"
printf '[Desktop Entry]\nType=Application\nName=Bounce Test\nIcon=applications-games\nExec=qml6 %s\nStartupNotify=true\n' "$CI/app.qml" \
    > "$CI/data/applications/harbour-bounce-test.desktop"
kbuildsycoca6 > /dev/null 2>&1
kwriteconfig6 --file harbourrc --group Behaviour --key ScreenMode follow
kwriteconfig6 --file harbourrc --group Items --key Launchers "applications:org.kde.dolphin.desktop,applications:harbour-bounce-test.desktop"
sleep 2
docks follow
BT=$(grep geometry "$CI/harbour.log" | tail -1 | grep -oE 'Bounce Test@[0-9]+' | cut -d@ -f2)
echo "Bounce Test at $BT"
probe move "$BT" $((H - 42)); sleep 0.4
probe click "$BT" $((H - 42)) left; sleep 0.25
# The tops of both docks' icons: a bouncing icon pokes well above its dock.
grab bounce 0 $((H - 160)) 3840 60 6 60
for f in "$SHOTS"/bounce-0*.png; do
    printf '%s left %s right %s\n' "$(basename "$f")" \
        "$(magick "$f" -crop 1920x60+0+0 +repage -format '%[fx:maxima.r>0.3?1:0]' info:)" \
        "$(magick "$f" -crop 1920x60+1920+0 +repage -format '%[fx:maxima.r>0.3?1:0]' info:)"
done
pkill -f "qml6 $CI/app.qml"
grep -E 'active dock' "$CI/harbour.log" | tail -4
