# Dragging the divider up grows the dock, down shrinks it, saved on release; the dock
# stays up throughout and a size change never hides it.
size() { kreadconfig6 --file harbourrc --group Appearance --key IconSize; }
LAST=$(grep geometry "$CI/harbour.log" | tail -1 | grep -oE '[^|]+@[0-9]+,' | grep -vE 'Downloads|Trash' | tail -1 | grep -oE '@[0-9]+' | tr -d @)
D=$(pos Downloads); SEP=$(( (LAST + D) / 2 )); Y=$((H - 42))
echo "size before: $(size)"
reveal "$SEP"
probe drag "$SEP" $Y "$SEP" $((Y - 30)) 500; sleep 0.8
snap bigger "1200x300+360+780"
echo "after dragging up 30 px: $(size)"
sleep 1
LAST=$(grep geometry "$CI/harbour.log" | tail -1 | grep -oE '[^|]+@[0-9]+,' | grep -vE 'Downloads|Trash' | tail -1 | grep -oE '@[0-9]+' | tr -d @)
D=$(pos Downloads); SEP=$(( (LAST + D) / 2 ))
probe drag "$SEP" $Y "$SEP" $((Y + 40)) 500; sleep 0.8
snap smaller "1200x300+360+780"
echo "after dragging down 40 px: $(size)"
