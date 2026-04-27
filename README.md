# mojo-raylib

<a name="readme-top"></a>
<br />

<div align="center">
    <img src="assets/image.png" alt="Logo" width="300" height="300">

  <h3 align="center">Mojo Raylib</h3>

  <p align="center">
    🔥 Mojo bindings for <a href="https://github.com/raysan5/raylib">raylib</a> v6.x 🔥
    <br/>

![Written in Mojo][language-shield]
[![MIT License][license-shield]][license-url]
[![CodeQL](https://github.com/kivicode/mojo-raylib/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/kivicode/mojo-raylib/actions/workflows/github-code-scanning/codeql)

<br/>

[![Contributors Welcome][contributors-shield]][contributors-url]

  </p>
</div>

Code generation is fully automated and provides 100% coverage, although not everything has been manually tested yet.

## Requirements

- [Pixi](https://prefix.dev/) — installs everything else.

## Run an example

```sh
cd examples/basic_window
pixi run run
```

The first run builds raylib + the C shim (`mojo-raylib-shim`) and the Mojo package (`mojo_raylib`) via [`pixi-build`](https://pixi.sh/dev/build/). Subsequent runs hit the build cache.

## Use it in your own project

Each example is a self-contained `pixi.toml`:

```toml
[workspace]
channels = ["https://conda.modular.com/max", "https://prefix.dev/conda-forge"]
platforms = ["osx-arm64", "linux-64"]
preview = ["pixi-build"]

[dependencies]
mojo_raylib = { path = "/path/to/mojo-raylib" }

[tasks.run]
cmd = "mojo build main.mojo -Xlinker -L$CONDA_PREFIX/lib -Xlinker -lmojo_raylib_shim -Xlinker -lraylib -o .pixi/main && .pixi/main"
```

```mojo
from mojo_raylib import Color, init_window, close_window, window_should_close
from mojo_raylib import begin_drawing, end_drawing, clear_background, draw_text, set_target_fps

def main():
    init_window(800, 450, "hello")
    set_target_fps(60)
    while not window_should_close():
        begin_drawing()
        clear_background(Color(245, 245, 245, 255))
        draw_text("Hello, raylib!", 190, 200, 20, Color(50, 50, 50, 255))
        end_drawing()
    close_window()
```

## Regenerating bindings

The Mojo bindings + C shim are emitted by a patched `rlparser` (`patch/rlparser.c`) reading raylib's headers.

```sh
pixi run -e codegen generate      # regenerate mojo_raylib/ + native/mojo_raylib_shim.c
pixi run -e codegen regen-check   # CI: fail if regeneration would change anything
```

## Layout

```
pixi.toml                workspace + mojo_raylib package (pixi-build-mojo backend)
shim/                    mojo-raylib-shim package (pixi-build-rattler-build, builds raylib + the C shim)
mojo_raylib/             generated safe API + raw FFI bindings
native/mojo_raylib_shim.c   generated C shim
patch/rlparser.c         patched raylib code-gen tool
vendor/raylib/           git submodule, pinned by parent commit
examples/<name>/         per-example pixi.toml + main.mojo
```

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->

[language-shield]: https://img.shields.io/badge/language-mojo-orange
[license-shield]: https://img.shields.io/github/license/kivicode/mojo-raylib?logo=github
[license-url]: https://github.com/kivicode/mojo-raylib/blob/main/LICENSE
[contributors-shield]: https://img.shields.io/badge/contributors-welcome!-blue
[contributors-url]: https://github.com/kivicode/mojo-raylib#contributing
