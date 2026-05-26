"""Particle physics for the assembly demo - pure Mojo, no raylib.

Positions/velocities live in flat Float32 buffers; each frame `step` runs a
SIMD kernel (W lanes) spread across cores with `parallelize`. The renderer
(main.mojo) only reads positions back out to draw them - this module never
touches raylib, so the compute and the rendering can be shown off separately.
"""

from collections import List
from math import sqrt
from algorithm import parallelize
from random import random_float64

# --- kernel shape ---
comptime W = 4       # SIMD lanes (128-bit NEON / Float32)
comptime CHUNKS = 8  # parallel work chunks

# --- tunables ---
comptime STIFF = Float32(70.0)          # spring stiffness (pull toward home)
comptime DAMP = Float32(6.0)            # velocity damping (low = coasting inertia)
comptime REPEL_HOVER = Float32(5000.0)  # gentle nudge when just hovering
comptime REPEL_DRAG = Float32(32000.0)  # forceful shove while the button is held
comptime EFFECT_R = Float32(95.0)       # mouse effect radius (px)
comptime REPEL_FLOOR = Float32(0.35)    # min push fraction inside the disc
comptime SWEEP = Float32(9.0)           # how strongly a drag carries particles along
comptime VMAX = Float32(3800.0)         # speed clamp (px/s)
comptime TURB = Float32(95.0)           # turbulence on displaced particles
comptime TURB_FADE = Float32(60.0)      # distance over which turbulence fades in


fn frand(lo: Float32, hi: Float32) -> Float32:
    """Uniform random Float32 in [lo, hi)."""
    return lo + (hi - lo) * Float32(random_float64())


struct ParticleSystem:
    """Owns the particle buffers and advances them one step at a time."""

    var xs: List[Float32]
    var ys: List[Float32]
    var vxs: List[Float32]
    var vys: List[Float32]
    var hxs: List[Float32]  # home / target positions
    var hys: List[Float32]
    var n: Int              # real particle count
    var np: Int             # padded to a whole number of SIMD groups
    var num_groups: Int
    var per: Int            # SIMD groups per parallel chunk
    var frame: UInt32
    var sw: Float32
    var sh: Float32

    fn __init__(out self, hx: List[Float32], hy: List[Float32],
                screen_w: Float32, screen_h: Float32):
        self.n = len(hx)
        self.num_groups = (self.n + W - 1) // W
        self.np = self.num_groups * W
        self.per = (self.num_groups + CHUNKS - 1) // CHUNKS
        self.frame = UInt32(0)
        self.sw = screen_w
        self.sh = screen_h
        self.xs = List[Float32]()
        self.ys = List[Float32]()
        self.vxs = List[Float32]()
        self.vys = List[Float32]()
        self.hxs = List[Float32]()
        self.hys = List[Float32]()
        for i in range(self.n):
            self.xs.append(frand(Float32(0), screen_w))
            self.ys.append(frand(Float32(0), screen_h))
            self.vxs.append(Float32(0))
            self.vys.append(Float32(0))
            self.hxs.append(hx[i])
            self.hys.append(hy[i])
        for _ in range(self.n, self.np):  # padding lanes: parked at home, off-screen
            self.xs.append(Float32(-100000.0))
            self.ys.append(Float32(-100000.0))
            self.vxs.append(Float32(0))
            self.vys.append(Float32(0))
            self.hxs.append(Float32(-100000.0))
            self.hys.append(Float32(-100000.0))

    fn count(self) -> Int:
        return self.n

    fn pos_x(self, i: Int) -> Float32:
        return self.xs[i]

    fn pos_y(self, i: Int) -> Float32:
        return self.ys[i]

    fn reset(mut self):
        """Re-scatter every particle so the swarm re-assembles."""
        for i in range(self.n):
            self.xs[i] = frand(Float32(0), self.sw)
            self.ys[i] = frand(Float32(0), self.sh)
            self.vxs[i] = Float32(0)
            self.vys[i] = Float32(0)

    fn step(mut self, dt: Float32, mx: Float32, my: Float32,
            mvx: Float32, mvy: Float32, dragging: Bool, speed: Float32):
        """Advance the whole system one frame (SIMD kernel across cores)."""
        var xp = self.xs.unsafe_ptr()
        var yp = self.ys.unsafe_ptr()
        var vxp = self.vxs.unsafe_ptr()
        var vyp = self.vys.unsafe_ptr()
        var hxp = self.hxs.unsafe_ptr()
        var hyp = self.hys.unsafe_ptr()
        var per = self.per
        var num_groups = self.num_groups
        var frame = self.frame
        var k = STIFF * speed
        var dmp = DAMP * sqrt(speed)              # keep the damping ratio fixed
        var strength = REPEL_HOVER
        if dragging:
            strength = REPEL_DRAG

        @parameter
        fn kern(c: Int):
            var lane = SIMD[DType.uint32, W](0)
            @parameter
            for j in range(W):
                lane[j] = UInt32(j)
            var mxv = SIMD[DType.float32, W](mx)
            var myv = SIMD[DType.float32, W](my)

            var g0 = c * per
            var g1 = min(g0 + per, num_groups)
            for g in range(g0, g1):
                var base = g * W
                var x = xp.load[width=W](base)
                var y = yp.load[width=W](base)
                var vx = vxp.load[width=W](base)
                var vy = vyp.load[width=W](base)
                var hx = hxp.load[width=W](base)
                var hy = hyp.load[width=W](base)

                # spring toward home
                var ehx = hx - x
                var ehy = hy - y
                var ax = ehx * k - vx * dmp
                var ay = ehy * k - vy * dmp

                # turbulence from a per-lane hash, faded in by distance from home
                var idx = SIMD[DType.uint32, W](base) + lane
                var s = idx * UInt32(1664525) + (frame * UInt32(2654435761) + UInt32(1013904223))
                s = s ^ (s >> 16)
                s = s * UInt32(2246822519)
                var nfx = s.cast[DType.float32]() * Float32(2.3283064e-10) - Float32(0.5)
                s = s ^ (s >> 13)
                s = s * UInt32(3266489917)
                var nfy = s.cast[DType.float32]() * Float32(2.3283064e-10) - Float32(0.5)
                var ehm = sqrt(ehx * ehx + ehy * ehy)
                var tscale = min(ehm / TURB_FADE, Float32(1.0))
                ax += nfx * TURB * tscale
                ay += nfy * TURB * tscale

                # mouse repulsor (soft radial falloff), masked to the disc.
                # SIMD `<` reduces to a scalar in this Mojo, so the disc/clamp
                # masks are built branchlessly with clamp instead.
                var dx = x - mxv
                var dy = y - myv
                var d2 = dx * dx + dy * dy
                var dist = sqrt(max(d2, SIMD[DType.float32, W](1.0e-6)))
                var edge = (SIMD[DType.float32, W](EFFECT_R) - dist) * Float32(1000.0)
                var nmask = min(max(edge, SIMD[DType.float32, W](0.0)), SIMD[DType.float32, W](1.0))
                var falloff = max(Float32(1.0) - dist / EFFECT_R, SIMD[DType.float32, W](0.0))
                var push = strength * (REPEL_FLOOR + (Float32(1.0) - REPEL_FLOOR) * falloff)
                ax += (dx / dist) * push * nmask
                ay += (dy / dist) * push * nmask

                vx = vx + ax * dt
                vy = vy + ay * dt

                # a drag also sweeps nearby particles toward the cursor's velocity
                if dragging:
                    var sw = SWEEP * falloff * dt * nmask
                    vx += (SIMD[DType.float32, W](mvx) - vx) * sw
                    vy += (SIMD[DType.float32, W](mvy) - vy) * sw

                # branchless speed clamp: scale down only when over VMAX
                var spd = sqrt(max(vx * vx + vy * vy, SIMD[DType.float32, W](1.0e-6)))
                var scale = min(SIMD[DType.float32, W](1.0), SIMD[DType.float32, W](VMAX) / spd)
                vx *= scale
                vy *= scale

                xp.store(base, x + vx * dt)
                yp.store(base, y + vy * dt)
                vxp.store(base, vx)
                vyp.store(base, vy)

        parallelize[kern](CHUNKS)
        self.frame += UInt32(1)
