#!/usr/bin/env python3
# A drag source for UI tests: drag the window's button to drop the URL given as the
# argument. GTK rather than Qt: without portals (as in the harness) a Qt source still
# routes file lists through the FileTransfer portal, and the drop arrives empty.
import sys

import gi

gi.require_version("Gdk", "3.0")
gi.require_version("Gtk", "3.0")
from gi.repository import Gdk, Gtk  # noqa: E402

url = sys.argv[1]

window = Gtk.Window(title="dragsource")
window.set_default_size(300, 200)
button = Gtk.Button(label="Drag me")
button.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [], Gdk.DragAction.COPY | Gdk.DragAction.MOVE)
button.drag_source_add_uri_targets()
button.connect("drag-data-get", lambda _w, _ctx, data, _info, _time: data.set_uris([url]))
window.add(button)
window.connect("destroy", Gtk.main_quit)
window.show_all()
Gtk.main()
