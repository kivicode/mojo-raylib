"""raylib [shaders] example - game of life (Mojo port).

Two render textures ping-pong through a fragment shader that computes one
Conway step per frame. Controls:
  SPACE — pause / resume
  R     — re-randomise the world
  Up    — slower (more frames per step)
  Down  — faster (fewer frames per step)
"""

from std.memory.unsafe_pointer import UnsafePointer

from mojo_raylib import (
    Color, Image, Rectangle, RenderTexture2D, Vector2,
    KEY_DOWN, KEY_R, KEY_SPACE, KEY_UP,
    SHADER_UNIFORM_VEC2,
    begin_drawing, begin_shader_mode, begin_texture_mode,
    clear_background, close_window,
    draw_fps, draw_line, draw_rectangle, draw_text, draw_texture_pro,
    end_drawing, end_shader_mode, end_texture_mode,
    gen_image_color, get_random_value, get_shader_location,
    image_clear_background, image_draw_pixel,
    init_window, is_key_pressed,
    load_render_texture, load_shader,
    set_target_fps, set_shader_value,
    unload_image, unload_render_texture, unload_shader,
    update_texture_rec,
    window_should_close,
)


comptime SCREEN_WIDTH = 800
comptime SCREEN_HEIGHT = 450
comptime MENU_WIDTH = 100
comptime WORLD_WIDTH = 512
comptime WORLD_HEIGHT = 512
comptime DEAD = Color(245, 245, 245, 255)   # RAYWHITE
comptime ALIVE = Color(0, 0, 0, 255)        # BLACK
comptime PANEL = Color(232, 232, 232, 255)
comptime PANEL_LINE = Color(218, 218, 218, 255)
comptime DARKBLUE = Color(0, 82, 172, 255)
comptime GRAY = Color(130, 130, 130, 255)


def randomize(target: RenderTexture2D, density_pct: Int):
    """Fill `target.texture` with random live cells."""
    var pattern = gen_image_color(WORLD_WIDTH, WORLD_HEIGHT, DEAD)
    image_clear_background(pattern, DEAD)
    for x in range(WORLD_WIDTH):
        for y in range(WORLD_HEIGHT):
            if get_random_value(0, 100) < density_pct:
                image_draw_pixel(pattern, x, y, ALIVE)
    update_texture_rec(
        target.texture,
        Rectangle(Float32(0), Float32(0), Float32(WORLD_WIDTH), Float32(WORLD_HEIGHT)),
        pattern.data,
    )
    unload_image(pattern)


def main():
    var view_w = SCREEN_WIDTH - MENU_WIDTH
    var view_h = SCREEN_HEIGHT

    init_window(SCREEN_WIDTH, SCREEN_HEIGHT, "raylib [shaders] example - game of life (Mojo port)")
    set_target_fps(60)

    var shader = load_shader(String(""), String("resources/game_of_life.fs"))
    var resolution_loc = get_shader_location(shader, String("resolution"))
    var resolution = Vector2(Float32(WORLD_WIDTH), Float32(WORLD_HEIGHT))
    set_shader_value(
        shader, resolution_loc,
        UnsafePointer(to=resolution).bitcast[NoneType]().mut_cast[True]().as_any_origin(),
        SHADER_UNIFORM_VEC2,
    )

    var world_a = load_render_texture(WORLD_WIDTH, WORLD_HEIGHT)
    var world_b = load_render_texture(WORLD_WIDTH, WORLD_HEIGHT)
    begin_texture_mode(world_a); clear_background(DEAD); end_texture_mode()
    begin_texture_mode(world_b); clear_background(DEAD); end_texture_mode()
    randomize(world_b, 18)

    # Source rect is flipped vertically (negative height) so the texture maps
    # right-side-up when blitted into the next render target.
    var world_src = Rectangle(Float32(0), Float32(0), Float32(WORLD_WIDTH), Float32(-WORLD_HEIGHT))
    var world_dst = Rectangle(Float32(0), Float32(0), Float32(WORLD_WIDTH), Float32(WORLD_HEIGHT))
    var screen_dst = Rectangle(Float32(0), Float32(0), Float32(view_w), Float32(view_h))
    var origin = Vector2(Float32(0), Float32(0))

    var current = world_b
    var previous = world_a
    var paused = False
    var frames_per_step = 1
    var frame: Int = 0

    while not window_should_close():
        frame += 1
        if is_key_pressed(KEY_SPACE):
            paused = not paused
        if is_key_pressed(KEY_R):
            randomize(current, 18)
        if is_key_pressed(KEY_DOWN) and frames_per_step > 1:
            frames_per_step -= 1
        if is_key_pressed(KEY_UP):
            frames_per_step += 1

        # Step the simulation: render `previous` through the shader into `current`.
        if not paused and (frame % frames_per_step) == 0:
            var tmp = current
            current = previous
            previous = tmp
            begin_texture_mode(current)
            begin_shader_mode(shader)
            draw_texture_pro(previous.texture, world_src, world_dst, origin, Float32(0.0), DEAD)
            end_shader_mode()
            end_texture_mode()

        begin_drawing()
        clear_background(DEAD)
        draw_texture_pro(current.texture, world_dst, screen_dst, origin, Float32(0.0), DEAD)

        # Side panel.
        draw_line(view_w, 0, view_w, SCREEN_HEIGHT, PANEL_LINE)
        draw_rectangle(view_w, 0, SCREEN_WIDTH - view_w, SCREEN_HEIGHT, PANEL)
        draw_text("Conway's", 704, 4, 20, DARKBLUE)
        draw_text(" game of", 704, 19, 20, DARKBLUE)
        draw_text("  life", 708, 34, 20, DARKBLUE)
        draw_text("in raylib", 757, 42, 6, ALIVE)

        draw_text("SPACE: pause", 710, 80, 8, GRAY)
        draw_text("R: random", 710, 95, 8, GRAY)
        draw_text("Up: slower", 710, 110, 8, GRAY)
        draw_text("Down: faster", 710, 125, 8, GRAY)

        if paused:
            draw_text("PAUSED", 720, 160, 12, ALIVE)

        draw_text(String("step: ") + String(frames_per_step), 710, 200, 8, GRAY)
        draw_fps(712, 426)

        end_drawing()

    unload_shader(shader)
    unload_render_texture(world_a)
    unload_render_texture(world_b)
    close_window()
