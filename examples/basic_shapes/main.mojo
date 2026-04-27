from mojo_raylib import Color, Vector2, begin_drawing, clear_background, close_window, draw_circle, draw_circle_gradient, draw_circle_lines, draw_ellipse, draw_ellipse_lines, draw_line, draw_poly, draw_poly_lines, draw_poly_lines_ex, draw_rectangle, draw_rectangle_gradient_h, draw_rectangle_lines, draw_text, draw_triangle, draw_triangle_lines, end_drawing, init_window, set_target_fps, window_should_close


def main():
    var screen_width = 800
    var screen_height = 450

    init_window(screen_width, screen_height, "raylib [shapes] example - basic shapes")

    var rotation: Float32 = 0.0

    set_target_fps(60)

    while not window_should_close():
        rotation += 0.2

        begin_drawing()
        clear_background(Color(245, 245, 245, 255))

        draw_text("some basic shapes available on raylib", 20, 20, 20, Color(80, 80, 80, 255))

        draw_circle(screen_width / 5, 120, 35.0, Color(0, 82, 172, 255))
        draw_circle_gradient(Vector2(Float32(screen_width) / 5.0, 220.0), 60.0, Color(0, 228, 48, 255), Color(102, 191, 255, 255))
        draw_circle_lines(screen_width / 5, 340, 80.0, Color(0, 82, 172, 255))
        draw_ellipse(screen_width / 5, 120, 25.0, 20.0, Color(253, 249, 0, 255))
        draw_ellipse_lines(screen_width / 5, 120, 30.0, 25.0, Color(253, 249, 0, 255))

        draw_rectangle(screen_width / 4 * 2 - 60, 100, 120, 60, Color(230, 41, 55, 255))
        draw_rectangle_gradient_h(screen_width / 4 * 2 - 90, 170, 180, 130, Color(190, 33, 55, 255), Color(255, 203, 0, 255))
        draw_rectangle_lines(screen_width / 4 * 2 - 40, 320, 80, 60, Color(255, 161, 0, 255))

        draw_triangle(
            Vector2(Float32(screen_width) / 4.0 * 3.0, 80.0),
            Vector2(Float32(screen_width) / 4.0 * 3.0 - 60.0, 150.0),
            Vector2(Float32(screen_width) / 4.0 * 3.0 + 60.0, 150.0),
            Color(135, 60, 190, 255),
        )

        draw_triangle_lines(
            Vector2(Float32(screen_width) / 4.0 * 3.0, 160.0),
            Vector2(Float32(screen_width) / 4.0 * 3.0 - 20.0, 230.0),
            Vector2(Float32(screen_width) / 4.0 * 3.0 + 20.0, 230.0),
            Color(0, 82, 172, 255),
        )

        draw_poly(Vector2(Float32(screen_width) / 4.0 * 3.0, 330.0), 6, 80.0, rotation, Color(127, 106, 79, 255))
        draw_poly_lines(Vector2(Float32(screen_width) / 4.0 * 3.0, 330.0), 6, 90.0, rotation, Color(127, 106, 79, 255))
        draw_poly_lines_ex(Vector2(Float32(screen_width) / 4.0 * 3.0, 330.0), 6, 85.0, rotation, 6.0, Color(211, 176, 131, 255))

        draw_line(18, 42, screen_width - 18, 42, Color(0, 0, 0, 255))
        end_drawing()

    close_window()
