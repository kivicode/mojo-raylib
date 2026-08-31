# Callback registration for the raylib safe API.
#
# raylib has several "set callback" entry points. The codegen skips them,
# because handing a Mojo function to C needs a bridge: C can only call a
# function that carries Mojo's `abi("C")` effect, and such a function has to
# take raw C types (`Pointer[c_char, ...]`, `c_int`, ...).
#
# None of that belongs in user code. Each helper below declares an *idiomatic*
# Mojo handler type (`Int`, `String`, ...) and builds the `abi("C")` bridge
# internally, so a handler is written as ordinary Mojo:
#
#     def my_log(level: Int, text: String):
#         print(level, text)
#
#     set_trace_log_callback[my_log]()
#
# The handler is still supplied in `[...]` rather than `(...)`: the bridge is
# generated around it at compile time, so it must be known at compile time.
# Handlers therefore cannot capture — pass state through globals or through the
# raylib object the callback is attached to.

from std.ffi import c_char, c_int, c_uchar, c_uint, external_call, CStringSlice
import mojo_raylib.types as public_types
from mojo_raylib.types import AudioStream

comptime TraceLogHandler = def(level: Int, text: String) thin -> None
"""Handler for `set_trace_log_callback`: one formatted raylib log message."""

comptime AudioHandler = def(
    buffer: Pointer[Float32, MutUntrackedOrigin], frames: Int
) thin -> None
"""Handler for the audio callbacks: `frames` x channels 32-bit float samples."""

comptime LoadFileDataHandler = def(
    file_name: String, data_size: Pointer[c_int, MutUntrackedOrigin]
) thin -> Pointer[c_uchar, MutUntrackedOrigin]
"""Handler for `set_load_file_data_callback`.

The returned buffer must be allocated so that raylib can `free()` it, and
`data_size` must be written with its length.
"""

comptime SaveFileDataHandler = def(
    file_name: String, data: Pointer[NoneType, MutUntrackedOrigin], data_size: Int
) thin -> Bool
"""Handler for `set_save_file_data_callback`. Returns success."""

comptime LoadFileTextHandler = def(
    file_name: String
) thin -> Pointer[c_char, MutUntrackedOrigin]
"""Handler for `set_load_file_text_callback`.

The returned string must be allocated so that raylib can `free()` it.
"""

comptime SaveFileTextHandler = def(file_name: String, text: String) thin -> Bool
"""Handler for `set_save_file_text_callback`. Returns success."""


@always_inline
def _c_string(ptr: Pointer[c_char, MutUntrackedOrigin]) -> String:
    """Copy a NUL-terminated C string into an owned Mojo `String`."""
    return String(CStringSlice(unsafe_from_ptr=ptr))


def set_trace_log_callback[handler: TraceLogHandler]():
    """Set a custom trace-log handler.

    Routed through the native shim, which formats raylib's `va_list` message
    into a plain C string before `handler` sees it.
    """

    def bridge(level: c_int, text: Pointer[c_char, MutUntrackedOrigin]) abi("C"):
        handler(Int(level), _c_string(text))

    external_call["mojo_raylib_SetTraceLogCallback", NoneType](bridge)


def set_load_file_data_callback[handler: LoadFileDataHandler]():
    """Set a custom binary-file loader."""

    def bridge(
        file_name: Pointer[c_char, MutUntrackedOrigin],
        data_size: Pointer[c_int, MutUntrackedOrigin],
    ) abi("C") -> Pointer[c_uchar, MutUntrackedOrigin]:
        return handler(_c_string(file_name), data_size)

    external_call["SetLoadFileDataCallback", NoneType](bridge)


def set_save_file_data_callback[handler: SaveFileDataHandler]():
    """Set a custom binary-file saver."""

    def bridge(
        file_name: Pointer[c_char, MutUntrackedOrigin],
        data: Pointer[NoneType, MutUntrackedOrigin],
        data_size: c_int,
    ) abi("C") -> Bool:
        return handler(_c_string(file_name), data, Int(data_size))

    external_call["SetSaveFileDataCallback", NoneType](bridge)


def set_load_file_text_callback[handler: LoadFileTextHandler]():
    """Set a custom text-file loader."""

    def bridge(
        file_name: Pointer[c_char, MutUntrackedOrigin]
    ) abi("C") -> Pointer[c_char, MutUntrackedOrigin]:
        return handler(_c_string(file_name))

    external_call["SetLoadFileTextCallback", NoneType](bridge)


def set_save_file_text_callback[handler: SaveFileTextHandler]():
    """Set a custom text-file saver."""

    def bridge(
        file_name: Pointer[c_char, MutUntrackedOrigin],
        text: Pointer[c_char, MutUntrackedOrigin],
    ) abi("C") -> Bool:
        return handler(_c_string(file_name), _c_string(text))

    external_call["SetSaveFileTextCallback", NoneType](bridge)


@always_inline
def _audio_bridge[
    handler: AudioHandler
]() -> def(Pointer[NoneType, MutUntrackedOrigin], c_uint) abi("C") thin -> None:
    """Wrap an `AudioHandler` in raylib's `void *`-based `AudioCallback`."""

    def bridge(
        buffer_data: Pointer[NoneType, MutUntrackedOrigin], frames: c_uint
    ) abi("C"):
        handler(buffer_data.unsafe_bitcast[Float32](), Int(frames))

    return bridge


def attach_audio_mixed_processor[handler: AudioHandler]():
    """Attach an audio processor to the entire audio pipeline (stereo float)."""
    external_call["AttachAudioMixedProcessor", NoneType](_audio_bridge[handler]())


def detach_audio_mixed_processor[handler: AudioHandler]():
    """Detach a pipeline audio processor previously attached with the same `handler`."""
    external_call["DetachAudioMixedProcessor", NoneType](_audio_bridge[handler]())


def set_audio_stream_callback[handler: AudioHandler](stream: AudioStream):
    """Set the audio-thread callback that requests new data for `stream`."""
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_SetAudioStreamCallback", NoneType](
        Pointer(to=raw_stream), _audio_bridge[handler]()
    )


def attach_audio_stream_processor[handler: AudioHandler](stream: AudioStream):
    """Attach an audio processor to `stream` (32-bit float samples)."""
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_AttachAudioStreamProcessor", NoneType](
        Pointer(to=raw_stream), _audio_bridge[handler]()
    )


def detach_audio_stream_processor[handler: AudioHandler](stream: AudioStream):
    """Detach a processor previously attached to `stream` with the same `handler`."""
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_DetachAudioStreamProcessor", NoneType](
        Pointer(to=raw_stream), _audio_bridge[handler]()
    )
