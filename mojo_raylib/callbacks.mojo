# Callback registration for the raylib safe API.
#
# raylib has several "set callback" entry points. The codegen skips them in the
# generated layers because passing a Mojo function to C needs care:
#
#   * The callback must carry Mojo's `abi("C")` effect so C can call it.
#   * An `abi("C")` function value can't be forwarded through an ordinary
#     parameter in Mojo 1.0.0b1 — the effect is dropped on a parameter's type
#     and the value doesn't conform to `AnyType` for `external_call`. The only
#     form that lowers to a real C function pointer is an *inferred compile-time*
#     parameter `materialize`d at the call site. Hence the callback is supplied
#     in `[...]`, not `(...)`:
#
#         def my_log(level: Int32, text: UnsafePointer[c_char, MutAnyOrigin]) abi("C"):
#             ...
#         set_trace_log_callback[my_log]()
#
# The callback's parameter type can't be checked (an `abi("C")` function-pointer
# type can't be carried here), so each function documents the exact C signature
# its callback must have — getting it wrong is undefined behaviour.

from std.ffi import c_char, c_int, c_uint, external_call
from std.memory.unsafe_pointer import UnsafePointer
import mojo_raylib.types as public_types
from mojo_raylib.types import AudioStream


def set_trace_log_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Set a custom trace-log handler.

    Callback: `def(level: Int32, text: UnsafePointer[c_char, MutAnyOrigin]) abi("C")`.
    Routed through the native shim, which formats raylib's `va_list` message
    into a plain C string before invoking `cb`.
    """
    external_call["mojo_raylib_SetTraceLogCallback", NoneType](materialize[cb]())


def set_load_file_data_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Set a custom binary-file loader.

    Callback: `def(fileName: UnsafePointer[c_char, MutAnyOrigin], dataSize: UnsafePointer[c_int, MutAnyOrigin]) -> UnsafePointer[c_uchar, MutAnyOrigin] abi("C")`.
    """
    external_call["SetLoadFileDataCallback", NoneType](materialize[cb]())


def set_save_file_data_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Set a custom binary-file saver.

    Callback: `def(fileName: UnsafePointer[c_char, MutAnyOrigin], data: UnsafePointer[NoneType, MutAnyOrigin], dataSize: Int32) -> Bool abi("C")`.
    """
    external_call["SetSaveFileDataCallback", NoneType](materialize[cb]())


def set_load_file_text_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Set a custom text-file loader.

    Callback: `def(fileName: UnsafePointer[c_char, MutAnyOrigin]) -> UnsafePointer[c_char, MutAnyOrigin] abi("C")`.
    """
    external_call["SetLoadFileTextCallback", NoneType](materialize[cb]())


def set_save_file_text_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Set a custom text-file saver.

    Callback: `def(fileName: UnsafePointer[c_char, MutAnyOrigin], text: UnsafePointer[c_char, MutAnyOrigin]) -> Bool abi("C")`.
    """
    external_call["SetSaveFileTextCallback", NoneType](materialize[cb]())


def attach_audio_mixed_processor[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Attach an audio processor to the entire audio pipeline.

    Callback: `def(bufferData: UnsafePointer[NoneType, MutAnyOrigin], frames: UInt32) abi("C")`,
    receiving `frames` x 2 samples as `float` (stereo).
    """
    external_call["AttachAudioMixedProcessor", NoneType](materialize[cb]())


def detach_audio_mixed_processor[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
]():
    """Detach a pipeline audio processor previously attached with the same `cb`."""
    external_call["DetachAudioMixedProcessor", NoneType](materialize[cb]())


def set_audio_stream_callback[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
](stream: AudioStream):
    """Set the audio-thread callback that requests new data for `stream`.

    Callback: `def(bufferData: UnsafePointer[NoneType, MutAnyOrigin], frames: UInt32) abi("C")`.
    """
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_SetAudioStreamCallback", NoneType](
        UnsafePointer(to=raw_stream), materialize[cb]()
    )


def attach_audio_stream_processor[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
](stream: AudioStream):
    """Attach an audio processor to `stream` (`frames` x 2 `float` samples, stereo).

    Callback: `def(bufferData: UnsafePointer[NoneType, MutAnyOrigin], frames: UInt32) abi("C")`.
    """
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_AttachAudioStreamProcessor", NoneType](
        UnsafePointer(to=raw_stream), materialize[cb]()
    )


def detach_audio_stream_processor[
    CbT: ImplicitlyCopyable & ImplicitlyDestructible, //, cb: CbT
](stream: AudioStream):
    """Detach a processor previously attached to `stream` with the same `cb`."""
    var raw_stream = public_types._to_raw_audio_stream(stream)
    external_call["mojo_raylib_DetachAudioStreamProcessor", NoneType](
        UnsafePointer(to=raw_stream), materialize[cb]()
    )
