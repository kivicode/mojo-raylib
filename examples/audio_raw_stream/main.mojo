"""raylib [audio] example - raw stream (Mojo port).

Synthesises a sine wave on the fly and pushes it into a raylib audio stream
each time the device asks for more samples. Up/Down change frequency,
Left/Right change pan. The waveform being fed to the device is plotted
underneath.
"""

from std.math import sin
from std.memory.unsafe_pointer import UnsafePointer
from std.memory import stack_allocation

from mojo_raylib import (
    Color, Vector2,
    KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_UP,
    PI,
    begin_drawing,
    clear_background, close_audio_device, close_window,
    draw_line_v, draw_text,
    end_drawing,
    get_time,
    init_audio_device, init_window,
    is_audio_stream_processed, is_key_down,
    load_audio_stream, play_audio_stream,
    set_audio_stream_buffer_size_default, set_audio_stream_pan,
    set_target_fps,
    unload_audio_stream, update_audio_stream,
    void_ref_array,
    window_should_close,
)


comptime SCREEN_W = 800
comptime SCREEN_H = 450
comptime SAMPLE_RATE = 44100
comptime BUFFER_SIZE = 4096
comptime DEAD = Color(245, 245, 245, 255)
comptime RED = Color(230, 41, 55, 255)
comptime DARKGRAY = Color(80, 80, 80, 255)


def main():
    init_window(SCREEN_W, SCREEN_H, "raylib [audio] example - raw stream (Mojo port)")
    init_audio_device()

    set_audio_stream_buffer_size_default(BUFFER_SIZE)

    var stream = load_audio_stream(SAMPLE_RATE, 32, 1)
    var pan: Float32 = 0.0
    set_audio_stream_pan(stream, pan)
    play_audio_stream(stream)

    var sine_freq: Int = 440
    var new_sine_freq: Int = 440
    var sine_index: Int = 0
    var sine_start_time: Float64 = 0.0

    var buffer = stack_allocation[BUFFER_SIZE, Float32]()
    for i in range(BUFFER_SIZE):
        buffer[i] = 0.0

    set_target_fps(30)

    while not window_should_close():
        if is_key_down(KEY_UP):
            new_sine_freq += 10
            if new_sine_freq > 12500:
                new_sine_freq = 12500
        if is_key_down(KEY_DOWN):
            new_sine_freq -= 10
            if new_sine_freq < 20:
                new_sine_freq = 20
        if is_key_down(KEY_LEFT):
            pan -= 0.01
            if pan < -1.0:
                pan = -1.0
            set_audio_stream_pan(stream, pan)
        if is_key_down(KEY_RIGHT):
            pan += 0.01
            if pan > 1.0:
                pan = 1.0
            set_audio_stream_pan(stream, pan)

        if is_audio_stream_processed(stream):
            for i in range(BUFFER_SIZE):
                var wavelength = SAMPLE_RATE // sine_freq
                buffer[i] = Float32(sin(2.0 * Float64(PI) * Float64(sine_index) / Float64(wavelength)))
                sine_index += 1
                if sine_index >= wavelength:
                    sine_freq = new_sine_freq
                    sine_index = 0
                    sine_start_time = get_time()
            update_audio_stream(stream, void_ref_array(buffer), BUFFER_SIZE)

        begin_drawing()
        clear_background(DEAD)

        draw_text(String("sine frequency: ") + String(sine_freq), SCREEN_W - 220, 10, 20, RED)
        # `pan` truncated to 2 chars after the dot via integer math
        var pan_x100 = Int(pan * 100.0)
        draw_text(String("pan x100: ") + String(pan_x100), SCREEN_W - 220, 30, 20, RED)
        draw_text("Up/Down to change frequency", 10, 10, 20, DARKGRAY)
        draw_text("Left/Right to pan", 10, 30, 20, DARKGRAY)

        # Plot the sine wave currently driving the stream.
        var window_start = Int((get_time() - sine_start_time) * Float64(SAMPLE_RATE))
        var window_size = Int(0.1 * Float64(SAMPLE_RATE))
        var wavelength = SAMPLE_RATE // sine_freq
        for i in range(SCREEN_W):
            var t0 = window_start + i * window_size // SCREEN_W
            var t1 = window_start + (i + 1) * window_size // SCREEN_W
            var y0 = 250.0 + 50.0 * sin(2.0 * Float64(PI) * Float64(t0) / Float64(wavelength))
            var y1 = 250.0 + 50.0 * sin(2.0 * Float64(PI) * Float64(t1) / Float64(wavelength))
            draw_line_v(
                Vector2(Float32(i), Float32(y0)),
                Vector2(Float32(i + 1), Float32(y1)),
                RED,
            )

        end_drawing()

    unload_audio_stream(stream)
    close_audio_device()
    close_window()
