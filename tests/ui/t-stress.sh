# Menus and the stack fan, opened, switched and closed quickly, many times; plus a
# look at the fan mid-flight and settled.
GH=$(pos Ghostty); ST=$(pos Steam); D=$(pos Downloads); TR=$(pos Trash)
Y=$((H - 42))
reveal "$D"
probe click "$D" $Y left; sleep 0.15; snap fan-mid
sleep 0.6; snap fan-open
probe click "$D" $Y left; sleep 0.5; snap fan-closed
for i in $(seq 1 25); do
    probe click "$GH" $Y right; sleep 0.12
    probe click "$ST" $Y right; sleep 0.12
    probe click "$TR" $Y right; sleep 0.12
    probe key 1; sleep 0.08
    probe click "$D" $Y left; sleep 0.2
    probe click "$D" $Y left; sleep 0.1
    probe click "$D" $Y right; sleep 0.12
    probe key 1; sleep 0.08
done
sleep 1.5
snap after
