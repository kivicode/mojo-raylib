"""raylib [textures] example - sprite animation (Mojo port).

Loads a 6-frame spritesheet (scarfy.png), advances the sub-rectangle each
tick, and draws the current frame. Use Left/Right to change playback speed.
"""

from mojo_raylib import (
    Color, Rectangle, Vector2,
    KEY_LEFT, KEY_RIGHT,
    begin_drawing,
    clear_background, close_window,
    draw_rectangle, draw_rectangle_lines, draw_text, draw_texture, draw_texture_rec,
    end_drawing,
    init_window, is_key_pressed,
    load_texture,
    set_target_fps,
    unload_texture,
    window_should_close,
)


comptime SCREEN_W = 800
comptime SCREEN_H = 450
comptime MAX_FRAME_SPEED = 15
comptime MIN_FRAME_SPEED = 1

comptime BG = Color(245, 245, 245, 255)
comptime WHITE = Color(255, 255, 255, 255)
comptime DARKGRAY = Color(80, 80, 80, 255)
comptime GRAY = Color(130, 130, 130, 255)
comptime RED = Color(230, 41, 55, 255)
comptime LIME = Color(0, 158, 47, 255)
comptime MAROON = Color(190, 33, 55, 255)


def main():
    init_window(SCREEN_W, SCREEN_H, "raylib [textures] example - sprite animation (Mojo port)")
    set_target_fps(60)

    # Texture must be loaded after InitWindow (needs an OpenGL context).
    var scarfy = load_texture(String("resources/scarfy.png"))

    var position = Vector2(Float32(350.0), Float32(280.0))
    var frame_w = Int(scarfy.width) // 6
    var frame_h = Int(scarfy.height)
    var frame_rec = Rectangle(Float32(0.0), Float32(0.0), Float32(frame_w), Float32(frame_h))

    var current_frame: Int = 0
    var frames_counter: Int = 0
    var frames_speed: Int = 8

    while not window_should_close():
        frames_counter += 1
        if frames_counter >= (60 // frames_speed):
            frames_counter = 0
            current_frame += 1
            if current_frame > 5:
                current_frame = 0
            frame_rec.x = Float32(current_frame * frame_w)

        if is_key_pressed(KEY_RIGHT):
            frames_speed += 1
        elif is_key_pressed(KEY_LEFT):
            frames_speed -= 1
        frames_speed = min(MAX_FRAME_SPEED, max(MIN_FRAME_SPEED, frames_speed))

        begin_drawing()
        clear_background(BG)

        # Full spritesheet on the left + current-frame outline.
        draw_texture(scarfy, 15, 40, WHITE)
        draw_rectangle_lines(15, 40, Int(scarfy.width), Int(scarfy.height), LIME)
        draw_rectangle_lines(
            15 + Int(frame_rec.x), 40 + Int(frame_rec.y),
            Int(frame_rec.width), Int(frame_rec.height), RED,
        )

        draw_text("FRAME SPEED: ", 165, 210, 10, DARKGRAY)
        draw_text(String(frames_speed) + String(" FPS"), 575, 210, 10, DARKGRAY)
        draw_text("PRESS LEFT/RIGHT TO CHANGE SPEED!", 290, 240, 10, DARKGRAY)

        for i in range(MAX_FRAME_SPEED):
            if i < frames_speed:
                draw_rectangle(250 + 21 * i, 205, 20, 20, RED)
            draw_rectangle_lines(250 + 21 * i, 205, 20, 20, MAROON)

        # The actual animated sprite — just the current sub-rectangle.
        draw_texture_rec(scarfy, frame_rec, position, WHITE)

        draw_text("(c) Scarfy sprite by Eiden Marsal",
                  SCREEN_W - 200, SCREEN_H - 20, 10, GRAY)

        end_drawing()

    unload_texture(scarfy)
    close_window()
