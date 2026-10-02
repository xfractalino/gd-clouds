extends Camera3D

@export var speed := 50.0
@export var fast_multiplier := 10.0
@export var mouse_sensitivity := 0.002


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if event.pressed else Input.MOUSE_MODE_VISIBLE
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		rotation.y -= event.relative.x * mouse_sensitivity
		rotation.x = clampf(rotation.x - event.relative.y * mouse_sensitivity, -PI / 2.0, PI / 2.0)


func _process(delta: float) -> void:
	var direction := Vector3(
			float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)),
			float(Input.is_physical_key_pressed(KEY_E)) - float(Input.is_physical_key_pressed(KEY_Q)),
			float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W)))

	if direction.is_zero_approx():
		return

	var current_speed := speed

	if Input.is_physical_key_pressed(KEY_SHIFT):
		current_speed *= fast_multiplier

	global_position += global_basis * direction.normalized() * current_speed * delta
