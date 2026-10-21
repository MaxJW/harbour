# Menus open above their item, whatever their size: a two-entry menu (an app that
# isn't running) fits below the anchor and used to open downwards over the icon.
SP=$(pos 'Spotify \(Launcher\)'); ST=$(pos Steam)
Y=$((H - 42))
reveal "$SP"
probe click "$SP" $Y right; sleep 0.7; snap menu-small "1000x400+460+680"
probe key 1; sleep 0.4
probe click "$ST" $Y right; sleep 0.7; snap menu-large "1000x700+460+380"
probe key 1
