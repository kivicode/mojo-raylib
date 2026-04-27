from mojo_raylib import Color, begin_drawing, clear_background, close_window, draw_text, end_drawing, init_window, set_target_fps, window_should_close


def main():
    var screen_width = 800
    var screen_height = 450

    init_window(screen_width, screen_height, "raylib [core] example - basic window")
    set_target_fps(60)

    while not window_should_close():
        begin_drawing()
        clear_background(Color(245, 245, 245, 255))
        draw_text("Congrats! You created your first window!", 190, 200, 20, Color(200, 200, 200, 255))
        end_drawing()

    close_window()
