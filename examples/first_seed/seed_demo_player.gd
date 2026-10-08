extends CharacterBody3D

@export_range(0.5, 20.0, 0.1) var move_speed: float = 5.0
@export_range(0.0005, 0.02, 0.0005) var mouse_sensitivity: float = 0.0025
@export_range(0.0, 50.0, 0.1) var gravity_strength: float = 9.8

@onready var camera: Camera3D = $Camera3D

func _ready() -> void:
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _physics_process(delta: float) -> void:
    var input_vector := Vector2.ZERO

    if Input.is_key_pressed(KEY_A):
        input_vector.x -= 1.0
    if Input.is_key_pressed(KEY_D):
        input_vector.x += 1.0
    if Input.is_key_pressed(KEY_W):
        input_vector.y += 1.0
    if Input.is_key_pressed(KEY_S):
        input_vector.y -= 1.0

    input_vector = input_vector.normalized()

    var forward := -global_transform.basis.z
    var right := global_transform.basis.x
    var movement := (right * input_vector.x + forward * input_vector.y) * move_speed

    velocity.x = movement.x
    velocity.z = movement.z

    if is_on_floor():
        velocity.y = 0.0
    else:
        velocity.y -= gravity_strength * delta

    move_and_slide()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
        rotate_y(-event.relative.x * mouse_sensitivity)
        camera.rotation.x = clampf(
            camera.rotation.x - event.relative.y * mouse_sensitivity,
            deg_to_rad(-80.0),
            deg_to_rad(80.0)
        )

    if event is InputEventKey and event.pressed and event.physical_keycode == KEY_ESCAPE:
        Input.mouse_mode = (
            Input.MOUSE_MODE_VISIBLE
            if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
            else Input.MOUSE_MODE_CAPTURED
        )
