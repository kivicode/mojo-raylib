from std.math import exp, log

from mojo_raylib import (
    Camera2D, Color, Rectangle, Vector2,
    FLAG_WINDOW_RESIZABLE,
    KEY_A, KEY_LEFT, KEY_R, KEY_RIGHT, KEY_S,
    begin_drawing, begin_mode_2d,
    clear_background, close_window,
    draw_line, draw_rectangle, draw_rectangle_lines, draw_rectangle_rec, draw_text,
    end_drawing, end_mode_2d,
    fade,
    get_mouse_wheel_move, get_random_value,
    init_window, is_key_down, is_key_pressed,
    set_config_flags, set_target_fps, window_should_close,
)


comptime MAX_BUILDINGS = 100


def main():
    var screen_width = 800
    var screen_height = 450

    set_config_flags(FLAG_WINDOW_RESIZABLE)
    init_window(screen_width, screen_height, "raylib [core] example - 2d camera (Mojo port)")

    var player = Rectangle(400.0, 280.0, 40.0, 40.0)
    var buildings = List[Rectangle](capacity=MAX_BUILDINGS)
    var building_colors = List[Color](capacity=MAX_BUILDINGS)

    var spacing: Int = 0
    for i in range(MAX_BUILDINGS):
        var w = get_random_value(50, 200)
        var h = get_random_value(100, 800)
        buildings.append(
            Rectangle(
                Float32(-6000 + spacing),
                Float32(screen_height - 130 - h),
                Float32(w),
                Float32(h),
            )
        )
        building_colors.append(
            Color(
                UInt8(get_random_value(200, 240)),
                UInt8(get_random_value(200, 240)),
                UInt8(get_random_value(200, 250)),
                255,
            )
        )
        spacing += w
        _ = i

    var camera = Camera2D(
        Vector2(Float32(screen_width) / 2.0, Float32(screen_height) / 2.0),
        Vector2(player.x + 20.0, player.y + 20.0),
        Float32(0.0),
        Float32(1.0),
    )

    set_target_fps(60)

    while not window_should_close():
        # --- Update -------------------------------------------------------
        if is_key_down(KEY_RIGHT):
            player.x += 2.0
        elif is_key_down(KEY_LEFT):
            player.x -= 2.0

        camera.target = Vector2(player.x + 20.0, player.y + 20.0)

        if is_key_down(KEY_A):
            camera.rotation -= 1.0
        elif is_key_down(KEY_S):
            camera.rotation += 1.0
        camera.rotation = max(Float32(-40.0), min(Float32(40.0), camera.rotation))

        # Log-scaled zoom for consistent feel across the range.
        camera.zoom = Float32(exp(log(Float64(camera.zoom)) + Float64(get_mouse_wheel_move()) * 0.1))
        camera.zoom = max(Float32(0.1), min(Float32(3.0), camera.zoom))

        if is_key_pressed(KEY_R):
            camera.zoom = 1.0
            camera.rotation = 0.0

        # --- Draw ---------------------------------------------------------
        begin_drawing()
        clear_background(Color(245, 245, 245, 255))

        begin_mode_2d(camera)

        draw_rectangle(-6000, 320, 13000, 8000, Color(80, 80, 80, 255))
        for i in range(MAX_BUILDINGS):
            draw_rectangle_rec(buildings[i], building_colors[i])
        draw_rectangle_rec(player, Color(230, 41, 55, 255))

        var cx = Int(camera.target.x)
        var cy = Int(camera.target.y)
        draw_line(cx, -screen_height * 10, cx, screen_height * 10, Color(0, 228, 48, 255))
        draw_line(-screen_width * 10, cy, screen_width * 10, cy, Color(0, 228, 48, 255))

        end_mode_2d()

        # Screen-space HUD ------------------------------------------------
        draw_text("SCREEN AREA", 640, 10, 20, Color(230, 41, 55, 255))

        var red = Color(230, 41, 55, 255)
        draw_rectangle(0, 0, screen_width, 5, red)
        draw_rectangle(0, 5, 5, screen_height - 10, red)
        draw_rectangle(screen_width - 5, 5, 5, screen_height - 10, red)
        draw_rectangle(0, screen_height - 5, screen_width, 5, red)

        draw_rectangle(10, 10, 250, 113, fade(Color(102, 191, 255, 255), 0.5))
        draw_rectangle_lines(10, 10, 250, 113, Color(0, 121, 241, 255))

        var black = Color(0, 0, 0, 255)
        var dark = Color(80, 80, 80, 255)
        draw_text("Free 2D camera controls:", 20, 20, 10, black)
        draw_text("- Right/Left to move player", 40, 40, 10, dark)
        draw_text("- Mouse Wheel to Zoom in-out", 40, 60, 10, dark)
        draw_text("- A / S to Rotate", 40, 80, 10, dark)
        draw_text("- R to reset Zoom and Rotation", 40, 100, 10, dark)

        end_drawing()

    close_window()
