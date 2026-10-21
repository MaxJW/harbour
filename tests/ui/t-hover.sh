# After a right-click menu closes, the dock must follow the pointer again straight
# away: magnification and the name label move to the Trash when the pointer does.
# (The press that opens a menu used to leave its icon holding the pointer.)
ST=$(pos Steam); TR=$(pos Trash); Y=$((H - 42))
above() {
    # Anything drawn above the plate over x (a magnified icon, the name label)?
    magick "$SHOTS/$1.png" -crop "40x60+$(($2 - 20))+$((H - 160))" +repage -format '%[fx:maxima.r>0.5?1:0]' info:
}
check() {
    probe move "$TR" $Y; sleep 0.5; snap "$1"
    [ "$(above "$1" "$TR")" = 1 ] && echo "$1: follows the pointer" || echo "$1: STUCK"
}
reveal "$ST"
probe move "$ST" $Y; sleep 0.4
check before-menu
probe click "$ST" $Y right; sleep 0.6; probe key 1; sleep 0.4
check after-escape
probe click "$ST" $Y right; sleep 0.6; probe click "$ST" $((Y - 200)) left; sleep 0.4
check after-click-outside
