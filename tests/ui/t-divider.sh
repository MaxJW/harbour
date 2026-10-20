# The divider between apps and stacks: clicks, right-clicks and drags on and across
# it, mixed with menus, over and over. Harbour must survive (dropping an app on the
# divider moves it to the end of the apps; check the "drags" shot).
geometry() { grep geometry "$CI/harbour.log" | tail -1 | sed 's/.*geometry: //'; }
LAST=$(geometry | grep -oE '[^|]+@[0-9]+,' | grep -vE 'Downloads|Trash' | tail -1 | grep -oE '@[0-9]+' | tr -d @)
D=$(pos Downloads); TR=$(pos Trash); GH=$(pos Ghostty)
SEP=$(( (LAST + D) / 2 ))
Y=$((H - 42))
echo "divider at $SEP"
reveal "$SEP"
for i in $(seq 1 15); do
    probe click "$SEP" $Y left; sleep 0.1
    probe click "$SEP" $Y right; sleep 0.15
    probe click "$GH" $Y right; sleep 0.15
    probe click "$SEP" $Y right; sleep 0.15
    probe key 1; sleep 0.1
    probe click "$D" $Y right; sleep 0.15
    probe click "$SEP" $Y left; sleep 0.15
    probe key 1; sleep 0.1
done
snap clicks "1000x300+460+780"
# Drags: from the divider, an app across it and back, the last app onto it, a stack onto it.
probe drag "$SEP" $Y "$GH" $Y 300; sleep 0.6
probe drag "$GH" $Y $((SEP + 20)) $Y 400; sleep 0.6
probe drag $((SEP + 20)) $Y "$GH" $Y 400; sleep 0.6
probe drag "$LAST" $Y "$SEP" $Y 300; sleep 0.6
probe drag "$D" $Y "$SEP" $Y 300; sleep 0.6
probe drag "$TR" $Y "$SEP" $Y 300; sleep 0.6
probe drag "$SEP" $Y "$SEP" $((Y - 300)) 300; sleep 0.8
probe move $((W / 2)) $((H / 3)); sleep 1
snap drags
