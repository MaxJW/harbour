#!/bin/bash
# Runs a Harbour UI test inside an invisible, isolated KWin (virtual backend, own D-Bus
# session, own config). Nothing touches the real session, cursor or running dock.
#
#   tests/ui/harness.sh <test-script> [outputs]
#
# Needs a build with -DHARBOUR_BUILD_UI_TESTS=ON (for edgeprobe), plus spectacle and
# ImageMagick. HARBOUR_BUILD_DIR picks the build tree (default: build/). Screenshots
# and logs go to <build>/ui-tests/.
#
# The test script is sourced inside the session with these available:
#   $CI (work dir), $W/$H (output size), probe <edgeprobe args>, snap <name> [crop],
#   pos <label> (dock item centre x from the geometry log), reveal <x>
HERE=$(dirname "$(readlink -f "$0")")
if [ -z "$INSIDE_CI" ]; then
    export INSIDE_CI=1
    exec timeout 180 dbus-run-session --config-file="$HERE/bus.conf" -- "$0" "$@" 2> /dev/null < /dev/null
fi

BUILD=$(readlink -f "${HARBOUR_BUILD_DIR:-$HERE/../../build}")
HARBOUR_BIN=$BUILD/bin/harbour
PROBE=$BUILD/bin/edgeprobe
for bin in "$HARBOUR_BIN" "$PROBE"; do
    [ -x "$bin" ] || { echo "missing $bin (build with -DHARBOUR_BUILD_UI_TESTS=ON)"; exit 1; }
done

CI=$BUILD/ui-tests
TEST=$(readlink -f "$1")
OUTPUTS=${2:-1}
W=1920; H=1080
SHOTS=$CI/shots
mkdir -p "$CI/config" "$SHOTS"
rm -f "$SHOTS"/*.png

# Isolated config: the user's look and dock settings, nothing else.
cp ~/.config/kdeglobals "$CI/config/" 2> /dev/null
cp ~/.config/harbourrc "$CI/config/" 2> /dev/null
printf '[Plugins]\nscreenedgeEnabled=false\n' > "$CI/config/kwinrc"
export XDG_CONFIG_HOME=$CI/config

SOCKET=wl-harbour-ci-$$
kwin_wayland --virtual --width $W --height $H --output-count "$OUTPUTS" --socket "$SOCKET" --no-lockscreen \
    > "$CI/kwin.log" 2>&1 &
KWIN=$!
for i in $(seq 1 50); do [ -S "$XDG_RUNTIME_DIR/$SOCKET" ] && break; sleep 0.1; done
export WAYLAND_DISPLAY=$SOCKET
export QT_QPA_PLATFORM=wayland

QT_FORCE_STDERR_LOGGING=1 QT_MESSAGE_PATTERN='%{time boot} %{category}: %{message}' \
    QT_LOGGING_RULES='qt.qpa.services.warning=false;harbour.geometry.info=true' \
    "$HARBOUR_BIN" > "$CI/harbour.log" 2>&1 &
HARBOUR=$!
sleep 3

probe() { "$PROBE" "$@" > /dev/null; }
snap() {
    spectacle -b -n -f -o "$SHOTS/$1.png" 2> /dev/null
    [ -n "$2" ] && magick "$SHOTS/$1.png" -crop "$2" "$SHOTS/$1.png"
}
pos() { grep geometry "$CI/harbour.log" | tail -1 | grep -oE "$1@[0-9]+" | cut -d@ -f2; }
reveal() { probe move "$1" $((H - 4)); for i in 1 2 3 4 5 6; do probe move "$1" $((H - 1)); done; sleep 0.4; }

source "$TEST"

if kill -0 $HARBOUR 2> /dev/null; then echo "harbour: alive"; else echo "harbour: DIED"; fi
grep -iE 'warn|error|TypeError|qml' "$CI/harbour.log" | sort | uniq -c | head -15
kill $HARBOUR 2> /dev/null
sleep 0.3
kill $KWIN 2> /dev/null
