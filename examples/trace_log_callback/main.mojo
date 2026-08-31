# Custom trace-log callback — routing raylib's log output into Mojo.
#
# Demonstrates `set_trace_log_callback`, one of the callback-registration
# helpers in `mojo_raylib.callbacks`. The handler is plain Mojo — `Int` and
# `String`, no `abi("C")` and no C pointers; `mojo_raylib` builds the C bridge
# around it. It is supplied as a compile-time parameter
# (`set_trace_log_callback[on_trace_log]()`) because that bridge is generated
# at compile time, so handlers cannot capture.
#
# raylib's real `SetTraceLogCallback` expects a `va_list`-based variadic
# callback; the native shim bridges that to the simple `(level, text)` form
# this callback uses.

from mojo_raylib import set_trace_log_callback, init_audio_device, close_audio_device

# raylib TraceLogLevel values.
comptime LOG_WARNING = 4
comptime LOG_ERROR = 5


# raylib (via the shim) calls this for every log message.
def on_trace_log(level: Int, text: String):
    var label: String
    if level == LOG_WARNING:
        label = "WARN"
    elif level == LOG_ERROR:
        label = "ERROR"
    else:
        label = "INFO"
    print("[mojo] (" + label + ") " + text)


def main():
    # Register our Mojo function as raylib's log handler.
    set_trace_log_callback[on_trace_log]()

    # Drive real raylib logging without opening a window: initialising the audio
    # device emits several INFO messages, which now arrive in `on_trace_log`.
    init_audio_device()
    close_audio_device()
