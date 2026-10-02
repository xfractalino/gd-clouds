# gd-clouds

Volumetric clouds for Godot, as a GDExtension.

The clouds are a layer wrapped around a planet. They are ray marched with compute shaders in a
[compositor effect](https://docs.godotengine.org/en/stable/tutorials/rendering/compositor.html) at reduced resolution,
then upsampled and composited over the scene.

It targets Godot 4.5 and newer, and is developed against 4.7. It needs a renderer with a `RenderingDevice`, so it does
not work with the Compatibility renderer. It has been tested with Forward+.

## Building

You need Python 3, a C++17 compiler, and either SCons or CMake.

```shell
git clone --recurse-submodules https://github.com/xfractalino/gd-clouds
cd gd-clouds
```

With SCons:

```shell
scons
```

Or with CMake:

```shell
cmake -S . -B build -G Ninja
cmake --build build
```

Either way, the library ends up in `project/addons/gd_clouds/bin/<platform>/`.

On Windows with MinGW, use the MSYS2 toolchain. The MinGW bundled with CLion fails to link the library.

## Using it in Godot

1. Build the library.
2. Copy `project/addons/gd_clouds` into the `addons` folder of your project. It has to stay at
   `res://addons/gd_clouds`, as the shaders are loaded from there.
3. Restart the editor.
4. Add a `VolumetricCloudsEffect` to the `Compositor` of your `WorldEnvironment` or `Camera3D`, and give it a noise
   texture:

```gdscript
var effect := VolumetricCloudsEffect.new()
effect.clouds_config = CloudsConfig.new()
effect.noise_texture = CloudNoise.bake(effect.clouds_config.seed)
effect.sun_direction = $Sun.global_basis.z

var compositor := Compositor.new()
compositor.compositor_effects = [effect]
$WorldEnvironment.compositor = compositor
```

Things to know:

- `CloudsConfig` describes the planet and the cloud layer. By default the planet is at the origin with a radius of
  1000, and the clouds sit between 10 and 50 units above its surface.
- Baking the noise takes a few seconds. Save the texture with `ResourceSaver` and load it afterwards, instead of
  baking it on every run.
- The clouds are only drawn in the running game, not in the editor viewport.

The three classes are documented in the editor's built-in help.

## Demo

Open the `project` folder in Godot and run it. The first run bakes the noise, which takes a few seconds. Hold the
right mouse button to look around, move with WASD, go down and up with Q and E, and hold Shift to move faster.

## License

See [LICENSE.md](LICENSE.md).
