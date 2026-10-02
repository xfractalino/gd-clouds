extends Node3D

@onready var _world_environment: WorldEnvironment = $WorldEnvironment
@onready var _sun: DirectionalLight3D = $Sun

var _effect: VolumetricCloudsEffect


func _ready() -> void:
	for effect in _world_environment.compositor.compositor_effects:
		if effect is VolumetricCloudsEffect:
			_effect = effect
			break

	if _effect == null:
		push_error("The compositor of the WorldEnvironment has no VolumetricCloudsEffect.")
		set_process(false)
		return

	if RenderingServer.get_rendering_device() == null:
		push_warning("This renderer has no RenderingDevice, so the clouds can't be rendered.")
	elif _effect.noise_texture == null and _effect.clouds_config != null:
		_effect.noise_texture = _load_or_bake_noise(_effect.clouds_config.seed)

	_print_properties("VolumetricCloudsEffect", _effect)

	if _effect.clouds_config != null:
		_print_properties("CloudsConfig", _effect.clouds_config)


func _process(_delta: float) -> void:
	# A directional light shines along its -Z axis, so +Z points back towards the sun.
	_effect.sun_direction = _sun.global_basis.z


## Baking the noise takes a while, so it is only done once per seed and then kept in the user data folder.
func _load_or_bake_noise(noise_seed: int) -> ImageTexture3D:
	var path := "user://cloud_noise_%d.res" % noise_seed

	if ResourceLoader.exists(path):
		var cached := load(path) as ImageTexture3D

		if cached != null:
			return cached

	var start := Time.get_ticks_msec()
	var texture := CloudNoise.bake(noise_seed)
	print("Baked the cloud noise in ", Time.get_ticks_msec() - start, " ms.")

	var error := ResourceSaver.save(texture, path)

	if error != OK:
		push_warning("Could not save the cloud noise to ", path, ": ", error_string(error))

	return texture


## Prints the properties the extension class itself adds, leaving out the inherited ones.
func _print_properties(title: String, object: Object) -> void:
	print(title, ":")

	for property in ClassDB.class_get_property_list(object.get_class(), true):
		print("  ", property.name, " = ", object.get(property.name))
