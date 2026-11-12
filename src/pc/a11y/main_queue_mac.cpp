/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The voices' turn on a Mac: the AVSpeech row's turn in
 * screen_reader_bridge.cpp, built by a11y.cmake's Mac branch alone, so the
 * bridge stays free of platform headers. CoreFoundation's C API, no
 * Objective-C. */
#include <CoreFoundation/CoreFoundation.h>

namespace a11y {

/* Prism's AVSpeech backend sends each call's work to the main dispatch queue
 * with dispatch_sync and waits (v0.18.2, sync_on_main in avspeech.cpp); the
 * main queue runs whenever the main thread's run loop does, which SDL's event
 * pump does once a frame. At quit the game thread waits for the voices'
 * worker in turns instead, and each turn runs the run loop for one pass
 * without waiting (a time of 0), so the blocks already queued run and the
 * game thread goes back to its own short wait at once. Default mode is one of
 * the common modes the main queue is served in. Only the main thread's run
 * loop serves the main queue, so on any other thread there is nothing to do. */
void run_main_queue() {
    if (CFRunLoopGetCurrent() != CFRunLoopGetMain()) {
        return;
    }
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);
}

}  // namespace a11y
