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

	_print_properties("VolumetricCloudsEffect", _effect)

	if _effect.clouds_config != null:
		_print_properties("CloudsConfig", _effect.clouds_config)


func _process(_delta: float) -> void:
	# A directional light shines along its -Z axis, so +Z points back towards the sun.
	_effect.sun_direction = _sun.global_basis.z


## Prints the properties the extension class itself adds, leaving out the inherited ones.
func _print_properties(title: String, object: Object) -> void:
	print(title, ":")

	for property in ClassDB.class_get_property_list(object.get_class(), true):
		print("  ", property.name, " = ", object.get(property.name))
