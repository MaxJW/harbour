// Drives the pointer and keyboard through KWin fake input, for the UI tests (tests/ui).
// Usage: edgeprobe <screenX> <screenY> <screenW> <screenH> [dockTopY]
#include "qwayland-fake-input.h"
#include <QGuiApplication>
#include <QTimer>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <cstdio>
#include <functional>
#include <time.h>

class FakeInput : public QWaylandClientExtensionTemplate<FakeInput>, public QtWayland::org_kde_kwin_fake_input {
public:
    FakeInput() : QWaylandClientExtensionTemplate<FakeInput>(4) { initialize(); }
};

static void mark(const char *what) {
    timespec ts; clock_gettime(CLOCK_BOOTTIME, &ts);
    std::printf("%ld.%03ld probe: %s\n", ts.tv_sec, ts.tv_nsec / 1000000, what);
    std::fflush(stdout);
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    // edgeprobe cmd ...: move X Y | click X Y [left|right|middle] | key CODE
    if (argc >= 2 && !isdigit(argv[1][0])) {
        FakeInput fi;
        if (!fi.isActive()) return 1;
        fi.authenticate(QStringLiteral("edgeprobe"), QStringLiteral("dock tests"));
        const QString cmd = QString::fromLatin1(argv[1]);
        if (cmd == u"stay") {
            // Keeps a pointer device plugged in: the virtual backend has none of its
            // own, so without this every probe run would add and remove one, and
            // clients would see the pointer appear (enter) and vanish (no leave).
            mark(argv[1]);
            return app.exec();
        }
        if (cmd == u"move" || cmd == u"click") {
            fi.pointer_motion_absolute(wl_fixed_from_double(atof(argv[2])), wl_fixed_from_double(atof(argv[3])));
            if (cmd == u"click") {
                const QString b = argc > 4 ? QString::fromLatin1(argv[4]) : QStringLiteral("left");
                const uint btn = b == u"right" ? 0x111 : b == u"middle" ? 0x112 : 0x110;
                QTimer::singleShot(60, Qt::PreciseTimer, [&fi, btn] { fi.button(btn, 1); });
                QTimer::singleShot(120, Qt::PreciseTimer, [&fi, btn] { fi.button(btn, 0); });
            }
        } else if (cmd == u"drag") {
            // drag X1 Y1 X2 Y2 [MS]: press, nudge past the drag threshold, wait for a
            // drag-and-drop to start, move over MS ms (default 600), linger, release
            const double x1 = atof(argv[2]), y1 = atof(argv[3]), x2 = atof(argv[4]), y2 = atof(argv[5]);
            const int ms = argc > 6 ? atoi(argv[6]) : 600;
            const int steps = qMax(1, ms / 16);
            fi.pointer_motion_absolute(wl_fixed_from_double(x1), wl_fixed_from_double(y1));
            QTimer::singleShot(60, Qt::PreciseTimer, [&fi] { fi.button(0x110, 1); });
            for (int i = 1; i <= 6; ++i) {
                QTimer::singleShot(100 + i * 16, Qt::PreciseTimer, [&fi, i, x1, y1] {
                    fi.pointer_motion_absolute(wl_fixed_from_double(x1 + i * 4), wl_fixed_from_double(y1));
                });
            }
            for (int i = 1; i <= steps; ++i) {
                QTimer::singleShot(500 + i * ms / steps, Qt::PreciseTimer, [&fi, i, steps, x1, y1, x2, y2] {
                    const double t = double(i) / steps;
                    fi.pointer_motion_absolute(wl_fixed_from_double(x1 + 24 + (x2 - x1 - 24) * t), wl_fixed_from_double(y1 + (y2 - y1) * t));
                });
            }
            QTimer::singleShot(500 + ms + 400, Qt::PreciseTimer, [&fi] { fi.button(0x110, 0); });
            mark(argv[1]);
            QTimer::singleShot(500 + ms + 700, &app, &QGuiApplication::quit);
            return app.exec();
        } else if (cmd == u"sweep") {
            // sweep X1 X2 Y MS: move smoothly at ~1 kHz, there and back
            const double x1 = atof(argv[2]), x2 = atof(argv[3]), y = atof(argv[4]);
            const int ms = atoi(argv[5]);
            for (int i = 0; i <= ms; i += 4) {
                const double t = double(i) / ms;
                const double x = t < 0.5 ? x1 + (x2 - x1) * t * 2 : x2 - (x2 - x1) * (t - 0.5) * 2;
                QTimer::singleShot(i, Qt::PreciseTimer, [&fi, x, y] { fi.pointer_motion_absolute(wl_fixed_from_double(x), wl_fixed_from_double(y)); });
            }
            mark(argv[1]);
            QTimer::singleShot(ms + 100, &app, &QGuiApplication::quit);
            return app.exec();
        } else if (cmd == u"chord") {
            // chord CODE...: press in order, release in reverse
            QList<uint> codes;
            for (int i = 2; i < argc; ++i) codes << uint(atoi(argv[i]));
            for (uint c : codes) fi.keyboard_key(c, 1);
            for (auto it = codes.rbegin(); it != codes.rend(); ++it) fi.keyboard_key(*it, 0);
        } else if (cmd == u"key") {
            const uint code = atoi(argv[2]);
            fi.keyboard_key(code, 1);
            fi.keyboard_key(code, 0);
        }
        mark(argv[1]);
        QTimer::singleShot(250, &app, &QGuiApplication::quit);
        return app.exec();
    }
    if (argc < 5) { std::fprintf(stderr, "usage\n"); return 2; }
    const double sx = atof(argv[1]), sy = atof(argv[2]), sw = atof(argv[3]), sh = atof(argv[4]);
    const double dockTop = argc > 5 ? atof(argv[5]) : sy + sh - 60;
    FakeInput fi;
    if (!fi.isActive()) { std::fprintf(stderr, "fake input unavailable\n"); return 1; }
    fi.authenticate(QStringLiteral("edgeprobe"), QStringLiteral("dock edge tests"));

    const double cx = sx + sw / 2, bottom = sy + sh - 1, away = sy + sh / 2;
    struct Step { int delay; std::function<void()> fn; };
    QList<Step> steps;
    auto at = [&](int delay, double x, double y, const char *m) {
        steps.append({delay, [&fi, x, y, m] { mark(m); fi.pointer_motion_absolute(wl_fixed_from_double(x), wl_fixed_from_double(y)); }});
    };
    // Push into the edge the way a hand does: repeated small downward motions.
    auto push = [&](int delay, double x, const char *m) {
        steps.append({delay, [&fi, x, bottom, m] { mark(m); fi.pointer_motion_absolute(wl_fixed_from_double(x), wl_fixed_from_double(bottom - 3)); }});
        for (int i = 0; i < 40; ++i)
            steps.append({8, [&fi] { fi.pointer_motion(wl_fixed_from_double(0), wl_fixed_from_double(4)); }});
    };

    at(0, cx, away, "start: centre, away from edge");
    push(500, cx, "T1 push centre");
    at(800, cx, away, "T1 move away (expect hide)");
    push(1000, sx + 120, "T2 push far left");
    at(800, sx + 120, away, "T2 move away");
    push(1000, cx, "T3 push centre");
    at(700, cx, away, "T3 move away");
    at(480, cx, dockTop + 10, "T3 back onto dock mid-hide (expect reversal)");
    at(800, cx, away, "T3 move away");
    for (int i = 0; i < 3; ++i) {
        push(1000, cx, "T4 rapid push");
        at(450, cx, away, "T4 rapid away");
    }
    at(1000, cx, away, "end");
    steps.append({100, [&] { app.quit(); }});

    // Chain the steps with precise timers: coarse timers scheduled from t=0 drift by
    // up to 5% and bunch the push motions together.
    std::function<void(int)> run = [&](int i) {
        if (i >= steps.size()) return;
        QTimer::singleShot(steps[i].delay, Qt::PreciseTimer, &app, [&, i] { steps[i].fn(); run(i + 1); });
    };
    run(0);
    return app.exec();
}
