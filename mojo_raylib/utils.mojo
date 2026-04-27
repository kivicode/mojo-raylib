"""Hand-written helpers for the raylib safe API.

Bridge a few raylib FFI patterns that aren't worth wiring through the rlparser
codegen:

* `void_ref(x)` — opaque mutable byte pointer to a Mojo value, ready for
  `set_shader_value`, `update_texture_rec`, `update_audio_stream`, etc.
* `void_ref_array(ptr)` — same, but for an existing typed pointer (e.g. a
  heap-allocated buffer).

Both produce `UnsafePointer[NoneType, MutAnyOrigin]` — the type the
generated raw bindings expect for `void *` parameters.
"""

from std.memory.unsafe_pointer import UnsafePointer


@always_inline
fn void_ref[T: AnyType](mut value: T) -> UnsafePointer[NoneType, MutAnyOrigin]:
    """Take an opaque mutable byte pointer to `value`."""
    return UnsafePointer(to=value).bitcast[NoneType]().unsafe_mut_cast[True]().as_any_origin()


@always_inline
fn void_ref_array[T: AnyType, //, origin: Origin[mut=True]](
    ptr: UnsafePointer[T, origin],
) -> UnsafePointer[NoneType, MutAnyOrigin]:
    """Reinterpret a typed buffer pointer as `void *` for a raylib FFI call."""
    return ptr.bitcast[NoneType]().unsafe_mut_cast[True]().as_any_origin()
