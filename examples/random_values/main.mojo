from mojo_raylib import Color, begin_drawing, clear_background, close_window, draw_text, end_drawing, get_random_value, init_window, set_target_fps, window_should_close


def main():
    var screen_width = 800
    var screen_height = 450

    init_window(screen_width, screen_height, "raylib [core] example - random values")

    var rand_value = get_random_value(-8, 5)
    var frames_counter: UInt = 0

    set_target_fps(60)

    while not window_should_close():
        frames_counter += 1

        if (((frames_counter / 120) % 2) == 1):
            rand_value = get_random_value(-8, 5)
            frames_counter = 0

        begin_drawing()
        clear_background(Color(245, 245, 245, 255))
        draw_text("Every 2 seconds a new random value is generated:", 130, 100, 20, Color(190, 33, 55, 255))
        draw_text(String(rand_value), 360, 180, 80, Color(200, 200, 200, 255))
        end_drawing()

    close_window()
