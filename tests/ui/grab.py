#!/usr/bin/env python3
# Grabs a burst of frames of a screen area through KWin's ScreenShot2 D-Bus API, much
# faster than spectacle, so animations can be checked frame by frame.
#
#   grab.py <out-prefix> <x> <y> <w> <h> <count> <interval-ms>
#
# Writes <out-prefix>-NN.png and prints each frame's time. KWin only answers callers
# listed in a desktop file (harness.sh provides one for python3).
import os
import sys
import threading
import time

import gi

gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib  # noqa: E402
from PIL import Image  # noqa: E402

prefix = sys.argv[1]
x, y, w, h, count, interval = (int(a) for a in sys.argv[2:8])

def read_all(fd):
    with os.fdopen(fd, "rb") as pipe:
        return pipe.read()


bus = Gio.bus_get_sync(Gio.BusType.SESSION)
frames = []
start = time.monotonic()
for i in range(count):
    due = start + i * interval / 1000
    time.sleep(max(0, due - time.monotonic()))
    taken = time.monotonic() - start
    read_fd, write_fd = os.pipe()
    fds = Gio.UnixFDList.new()
    fds.append(write_fd)
    os.close(write_fd)
    # KWin may fill the pipe before it replies, so read it meanwhile.
    chunks = []
    reader = threading.Thread(target=lambda fd=read_fd: chunks.append(read_all(fd)))
    reader.start()
    reply, _ = bus.call_with_unix_fd_list_sync(
        "org.kde.KWin", "/org/kde/KWin/ScreenShot2", "org.kde.KWin.ScreenShot2", "CaptureArea",
        GLib.Variant("(iiuua{sv}h)", (x, y, w, h, {}, 0)), GLib.VariantType("(a{sv})"),
        Gio.DBusCallFlags.NONE, 5000, fds, None)
    info = reply.unpack()[0]
    del fds  # drops our copy of the write end, so the reader sees EOF
    reader.join(5)
    frames.append((taken, info, chunks[0] if chunks else b""))

for i, (taken, info, data) in enumerate(frames):
    width, height, stride, fmt = info["width"], info["height"], info["stride"], info["format"]
    # QImage formats: ARGB32 / ARGB32_Premultiplied / RGB32 are BGRA in memory here.
    mode = "BGRA" if fmt in (4, 5, 6) else "RGBA"
    Image.frombuffer("RGBA", (width, height), data, "raw", mode, stride, 1).save(f"{prefix}-{i:02d}.png")
    print(f"{prefix}-{i:02d}.png {taken * 1000:.0f} ms")
