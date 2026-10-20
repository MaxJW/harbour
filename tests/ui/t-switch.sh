# One-click menu switching, and close-on-second-click.
GH=$(pos Ghostty); ST=$(pos Steam)
echo "Ghostty=$GH Steam=$ST"
reveal "$GH"
snap shown
probe click "$GH" $((H - 42)) right; sleep 0.6; snap menu-ghostty
probe click "$ST" $((H - 42)) right; sleep 0.7; snap menu-steam
probe click "$ST" $((H - 42)) right; sleep 0.7; snap menu-closed
probe key 1
