# Game controllers

Include `<Tempest/Application>` and `<Tempest/GameController>`.
`Application::controllers()` returns value
handles for the connected extended gamepads. Copies and repeated enumeration of
one connection share its immutable `device()` metadata, current state and FIFO.
There is one event consumer per connection, not one subscription per copy.
Connection IDs are never reused; reconnecting creates a new handle identity.

```cpp
auto controllers = Tempest::Application::controllers();
if(!controllers.empty()) {
  auto controller = controllers.front();
  controller.pull([](const Tempest::GameController::Event& event) {
    // Inspect button transitions and cancel actions on Reset/Disconnected.
    });
  auto movement = controller.axis(Tempest::GameController::Stick::Left);
  auto snapshot = controller.state(); // Also includes both analog triggers.
  }
```

`device().supportedButtons` and `supportedAxes` distinguish missing optional
controls from supported controls at zero. Face buttons use position names
South/East/West/North. Stick components are in `[-1, 1]`, with positive Y upward;
triggers are in `[0, 1]`. Nonfinite native samples are ignored and out-of-range
values are clamped. These are normalized native values, including framework
filtering. Tempest adds no game dead zone or neutral rearming.

## Threads and callbacks

On macOS, call `Application::controllers()` on the main thread after `NSApplication` has
finished launching, and keep its main run loop running. The backend neither
creates an application nor requires a Tempest `Window`. A non-Tempest host owns
its AppKit startup and run loop. A Tempest window still performs its normal
guarded `finishLaunching` when Tempest owns the application.

Native discovery, observer lifetime and GameController input handlers use the
main thread. The separate `ControllerApi` registry owns native resources and
clears each connection's native pointer during disconnection or registry cleanup.
Ordinary window creation does not initialize this registry. `GameController` is
a facade over shared synchronized connection state; `Detail::MacosController`
extends that implementation for native input without a second event queue.
Handles may outlive native disconnection or be destroyed on a worker thread.
GameController is weak-linked and enumeration returns no connections when the
required native API is unavailable.

`state()` and `axis()` may run on any thread and return the latest synchronized
sample. They never consume queued events. Samples are not tied to a particular
queued button event or game frame; native input can advance between calls.

`pull(callback)` invokes callbacks synchronously on the calling thread, without
holding internal locks. The application chooses that thread. Pulls for the same
connection must be serialized across every copy, and a callback must not reenter
`pull` for that connection. The application enforces this requirement; there is
no runtime rejection of recursive or overlapping pulls. Calls for distinct
connections are independent. A callback may inspect state or destroy its
calling handle. If it throws, the exception propagates; the current event has
been consumed, while untouched events remain queued and a later pull can resume.
One call delivers at most the queue length observed when that call started, so
continuous native input cannot keep a game tick inside `pull` indefinitely.

The backend never forwards controller input through a window event dispatcher
or subscribes to `Application::onEventLoop()`. An application can choose its own
polling and UI delivery points. A synchronous modal loop entered by application
code needs its own polling strategy; it must not recursively call `pull` on the
same connection before the outer call returns.

## Cancellation and bounded history

The per-connection FIFO preserves up to 1024 events observed by the backend.
This retains a press and release that arrive between game frames, within that
bound. It cannot promise unlimited history or recover input the native API did
not report.

Overflow discards pending history and publishes `Reset` with reason `Overflow`
and this connection's ID. The triggering edge is represented by the current
snapshot, rather than a possibly incomplete event sequence. Other controllers'
queues and states are unaffected.

App focus loss replaces this connection's pending events with a `Reset` whose
reason is `FocusLost`. Button edges are suppressed while the app is inactive;
native callbacks still update the current snapshot. macOS may supply synthetic
neutral samples when an app loses foreground status. Focus recovery refreshes
the native snapshot while still inactive, then publishes `Reset(FocusGained)`
before accepting new edges. This recovery notification is necessary because an
application's window-focus callback can run before the native snapshot refresh.
The application must resynchronize on the reset even if it already handled
window focus. Recovery does not invent button edges.

`Reset` means cancellation, not a physical button release. The application must
cancel held actions and decide how to resume from current input, including
whether to require neutral controls. Inactive input, overflow and asynchronous
snapshot changes must not be interpreted as a new user gesture automatically.
Bindings, device selection, focus policy, dead zones and UI navigation belong
to the application.

Disconnect clears the snapshot and replaces pending input with
`Reset(Disconnected)` followed by terminal `Disconnected`. The terminal event
cannot be displaced by queue overflow or late native callbacks. Stale handles
remain safe; their metadata persists and `state()`/`axis()` return zero. Copies
share consumption of the terminal event just as they share button events.
An empty or moved-from handle instead has device ID zero, an empty name and
capability masks, zero state/axes, and a no-op `pull`. Its accessors are safe.

## Regression tests

The controller regressions are part of the existing `TempestTests` project and
use its bundled GoogleTest:

```sh
cmake -S Tests/tests -B build-tests -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tests --target TempestTests -j 4
./build-tests/testsuite/TempestTests --gtest_filter='Controller*'
```

These tests cover shared handles/queues, short taps, normalized snapshots,
focus cancellation, per-device overflow, terminal disconnect, callback threads,
concurrent snapshots, exceptions, finite draining, empty/moved-from access and
handle destruction from callbacks. Cancellation during a callback must replace
pending edges immediately; a callback exception preserves untouched events.
The normal test project also builds the renderer and test shaders and requires
its usual dependencies, including `glslangValidator`.

Local native validation invokes the installed native handlers on synthetic
profiles, including retained callbacks, focus recovery, reconnect identity,
registry cleanup and final handle release on a worker. It initializes AppKit
without a Tempest window and must run in a desktop session. It does not establish
physical USB/Bluetooth coverage. Native macOS tests in Codex must run outside
the command sandbox with its normal permission elevation.

Compiler-supported address/undefined-behavior and thread sanitizer runs use
separate builds. Match sanitizer flags for compiled sources and linking; use
`-fsanitize=address,undefined -fno-omit-frame-pointer` or, separately,
`-fsanitize=thread -fno-omit-frame-pointer`.

Only macOS is implemented. Windows, Linux, iOS and Android backends are separate
work; the current fallback returns no controllers. Future Windows support must
choose its native API and runtime loading independently, without adding a hard
controller DLL dependency to ordinary windowing.
