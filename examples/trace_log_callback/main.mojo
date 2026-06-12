# Custom trace-log callback — routing raylib's log output into Mojo.
#
# Demonstrates `set_trace_log_callback`, one of the callback-registration
# helpers in `mojo_raylib.callbacks`. Passing a Mojo function to raylib as a C
# callback relies on Mojo's `abi("C")` effect (it gives the function the C
# calling convention so C can call back into it), and the callback is supplied
# as a compile-time parameter — `set_trace_log_callback[on_trace_log]()` — which
# is the form that lowers to a real C function pointer in Mojo 1.0.0b1.
#
# raylib's real `SetTraceLogCallback` expects a `va_list`-based variadic
# callback; the native shim bridges that to the simple `(level, text)` form
# this callback uses.

from mojo_raylib import set_trace_log_callback, init_audio_device, close_audio_device
from std.ffi import c_int, c_char, CStringSlice
from std.memory.unsafe_pointer import UnsafePointer

# raylib TraceLogLevel values.
comptime LOG_WARNING = 4
comptime LOG_ERROR = 5


# raylib (via the shim) calls this for every log message. Must be `abi("C")`.
def on_trace_log(level: c_int, text: UnsafePointer[c_char, MutAnyOrigin]) abi("C"):
    var message = String(CStringSlice(unsafe_from_ptr=text))
    var label: String
    if level == LOG_WARNING:
        label = "WARN"
    elif level == LOG_ERROR:
        label = "ERROR"
    else:
        label = "INFO"
    print("[mojo] (" + label + ") " + message)


def main():
    # Register our Mojo function as raylib's log handler. The callback is a
    # compile-time parameter — that is what makes it a usable C callback.
    set_trace_log_callback[on_trace_log]()

    # Drive real raylib logging without opening a window: initialising the audio
    # device emits several INFO messages, which now arrive in `on_trace_log`.
    init_audio_device()
    close_audio_device()
