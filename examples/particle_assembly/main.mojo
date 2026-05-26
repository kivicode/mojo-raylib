from collections import List
from math import sqrt
from mojo_raylib import *
from physics import ParticleSystem, EFFECT_R

comptime SCREEN_W = 1000
comptime SCREEN_H = 540

comptime BG = Color(236, 238, 242, 255)
comptime TITLE = Color(40, 44, 52, 255)
comptime HINT = Color(110, 116, 124, 255)
comptime TRACK = Color(206, 210, 216, 255)
comptime MOJO_ORANGE = Color(255, 90, 30, 255)

comptime PARTICLE_R = Float32(2.0)    # drawn particle radius

comptime SAMPLE_STEP = 2

# raylib logo: 
comptime RL_GRID = 160 # downscaled grid
comptime RL_SCALE = Float32(2.3) # px/sample
comptime RL_X = 560 # placement
comptime RL_Y = 70

# Mojo wordmark (left): canvas, font size, placement
comptime MJ_W = 420
comptime MJ_H = 170
comptime MJ_FONT = 130
comptime MJ_X = 70
comptime MJ_Y = 150

# Speed slider geometry + range (UI side; speed is fed to the physics step).
comptime SLIDER_X = 95
comptime SLIDER_Y = 62
comptime SLIDER_W = 200
comptime SPEED_MIN = Float32(0.25)
comptime SPEED_MAX = Float32(3.0)


def main():
    init_window(SCREEN_W, SCREEN_H, "mojo-raylib - particle assembly")
    # set_target_fps(60)

    # Window icon, from the raylib logo.
    var icon = load_image("resources/raylib_logo.png")
    image_resize(icon, 64, 64)
    image_format(icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    set_window_icon(icon)
    unload_image(icon)

    # --- sample the two logos into particle targets + per-particle colors ---
    var thx = List[Float32]()
    var thy = List[Float32]()
    var cols = List[Color]()

    var rimg = load_image("resources/raylib_logo.png")
    image_resize(rimg, RL_GRID, RL_GRID)

    for y in range(0, RL_GRID, SAMPLE_STEP):
        for x in range(0, RL_GRID, SAMPLE_STEP):
            var c = get_image_color(rimg, x, y)

            if Int(c.a) < 16:
                continue

            if Int(c.r) > 235 and Int(c.g) > 235 and Int(c.b) > 235:
                continue

            thx.append(Float32(RL_X) + Float32(x) * RL_SCALE)
            thy.append(Float32(RL_Y) + Float32(y) * RL_SCALE)
            cols.append(c)

    unload_image(rimg)

    var mimg = gen_image_color(MJ_W, MJ_H, Color(0, 0, 0, 0))
    image_draw_text(mimg, "Mojo", 8, 16, MJ_FONT, MOJO_ORANGE)

    for y in range(0, MJ_H, SAMPLE_STEP):
        for x in range(0, MJ_W, SAMPLE_STEP):
            var c = get_image_color(mimg, x, y)
            if Int(c.a) < 50:
                continue

            thx.append(Float32(MJ_X) + Float32(x))
            thy.append(Float32(MJ_Y) + Float32(y))
            cols.append(c)

    unload_image(mimg)

    # hand the targets to the pure-Mojo simulation
    var sim = ParticleSystem(thx, thy, Float32(SCREEN_W), Float32(SCREEN_H))

    var speed = Float32(1.0)
    var editing_slider = False
    var slider_hit = Rectangle(
        Float32(SLIDER_X - 10), Float32(SLIDER_Y - 14),
        Float32(SLIDER_W + 20), Float32(28),
    )
    var mp0 = get_mouse_position()
    var pmx = mp0.x
    var pmy = mp0.y

    while not window_should_close():
        var dt = max(min(get_frame_time(), Float32(0.033)), Float32(0.0005))
        var mp = get_mouse_position()
        var mx = mp.x
        var my = mp.y
        var lmb = is_mouse_button_down(MOUSE_BUTTON_LEFT)

        # speed slider
        var over_slider = check_collision_point_rec(Vector2(mx, my), slider_hit)
        if lmb and (over_slider or editing_slider):
            editing_slider = True
            var t = (mx - Float32(SLIDER_X)) / Float32(SLIDER_W)
            t = min(max(t, Float32(0)), Float32(1))
            speed = SPEED_MIN + t * (SPEED_MAX - SPEED_MIN)
        if not lmb:
            editing_slider = False

        var dragging = lmb and not over_slider and not editing_slider

        # cursor velocity (passed to the sim so a drag can carry particles along)
        var mvx = (mx - pmx) / dt
        var mvy = (my - pmy) / dt
        var mv2 = mvx * mvx + mvy * mvy

        if mv2 > Float32(2500.0) * Float32(2500.0):
            var ms = Float32(2500.0) / sqrt(mv2)
            mvx *= ms
            mvy *= ms

        pmx = mx
        pmy = my

        if is_key_pressed(KEY_SPACE):
            sim.reset()

        # advance the simulation (pure Mojo)
        sim.step(dt, mx, my, mvx, mvy, dragging, speed)

        # draw it (raylib)
        begin_drawing()
        clear_background(BG)
        draw_fps(SCREEN_W - 100, 10)

        for i in range(sim.count()):
            draw_circle_v(Vector2(sim.pos_x(i), sim.pos_y(i)), PARTICLE_R, cols[i])

        if dragging:
            draw_circle_lines(Int(mx), Int(my), EFFECT_R, Color(120, 124, 130, 120))

        draw_text("speed", 20, SLIDER_Y - 8, 16, TITLE)
        draw_rectangle(SLIDER_X, SLIDER_Y - 2, SLIDER_W, 4, TRACK)

        var t_now = (speed - SPEED_MIN) / (SPEED_MAX - SPEED_MIN)
        var handle_x = Float32(SLIDER_X) + t_now * Float32(SLIDER_W)
        
        draw_circle_v(Vector2(handle_x, Float32(SLIDER_Y)), Float32(8.0), MOJO_ORANGE)

        draw_text("mojo-raylib  -  particle assembly", 20, 16, 20, TITLE)
        draw_text("hold & drag to shove particles   -   hover to nudge   -   SPACE to reset",
                  20, SCREEN_H - 30, 16, HINT)
        end_drawing()

    close_window()
