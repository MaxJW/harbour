# The dock stays magnified while a menu is open (the menu sitting above the enlarged
# icon), follows a switch to another item's menu, and resumes after closing.
ST=$(pos Steam); GH=$(pos Ghostty); TR=$(pos Trash); Y=$((H - 42))
reveal "$ST"
probe move "$ST" $Y; sleep 0.5
probe click "$ST" $Y right; sleep 0.7; snap menu-steam "1100x560+410+520"
# Where Discord is drawn while Steam is enlarged (two icons along from Steam).
DI=$(pos Equibop)
probe click "$DI" $Y right; sleep 0.8; snap menu-switch "1100x560+410+520"
probe key 1; sleep 0.3
probe move "$TR" $Y; sleep 0.5; snap after "1100x300+410+780"
