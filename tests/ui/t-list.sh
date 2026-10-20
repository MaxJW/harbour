# List view (native menu) for Downloads, and the Trash and stack menus.
D=$(pos Downloads); TR=$(pos Trash)
Y=$((H - 42))
kwriteconfig6 --file harbourrc --group Items --key StackViews list   # the CI copy only
sleep 1
reveal "$D"
probe click "$D" $Y left; sleep 0.8; snap list
probe key 1; sleep 0.4
probe click "$TR" $Y right; sleep 0.7; snap trash-menu
probe key 1; sleep 0.4
probe click "$D" $Y right; sleep 0.7; snap stack-menu
probe key 1
