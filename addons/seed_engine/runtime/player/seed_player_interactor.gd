extends Node
class_name SeedPlayerInteractor

signal interactable_focused(interactable: SeedInteractable)
signal interactable_cleared
signal interaction_attempted(interactable: SeedInteractable, succeeded: bool)

@export_category("Interaction")
@export var camera_path: NodePath
@export var actor_path: NodePath = NodePath("..")
@export_range(0.5, 20.0, 0.1) var interaction_distance: float = 3.0
@export var input_action: StringName = &"seed_interact"
@export_flags_3d_physics var collision_mask: int = 0xFFFFFFFF

var _focused: SeedInteractable

func _process(_delta: float) -> void:
    var next_focus := find_interactable()
    if next_focus == _focused:
        return

    _focused = next_focus
    if _focused:
        interactable_focused.emit(_focused)
    else:
        interactable_cleared.emit()

func _unhandled_input(event: InputEvent) -> void:
    if event.is_action_pressed(input_action):
        try_interact()

func try_interact() -> bool:
    var interactable := find_interactable()
    if interactable == null:
        return false

    var succeeded := interactable.interact(get_actor())
    interaction_attempted.emit(interactable, succeeded)
    return succeeded

func find_interactable() -> SeedInteractable:
    var camera := _get_camera()
    if camera == null or not camera.is_inside_tree():
        return null

    var viewport := camera.get_viewport()
    if viewport == null:
        return null

    var screen_center := viewport.get_visible_rect().size * 0.5
    var origin := camera.project_ray_origin(screen_center)
    var direction := camera.project_ray_normal(screen_center)
    var target := origin + direction * interaction_distance

    var query := PhysicsRayQueryParameters3D.create(origin, target, collision_mask)
    var actor := get_actor()
    if actor is CollisionObject3D:
        query.exclude = [(actor as CollisionObject3D).get_rid()]

    var result := camera.get_world_3d().direct_space_state.intersect_ray(query)
    if result.is_empty():
        return null

    var collider := result.get("collider") as Node
    return _find_interactable_from_node(collider)

func get_actor() -> Node:
    var actor := get_node_or_null(actor_path)
    return actor if actor else get_parent()

func _get_camera() -> Camera3D:
    if not camera_path.is_empty():
        var explicit_camera := get_node_or_null(camera_path) as Camera3D
        if explicit_camera:
            return explicit_camera

    var actor := get_actor()
    if actor:
        return _find_camera_recursive(actor)
    return null

func _find_camera_recursive(node: Node) -> Camera3D:
    if node is Camera3D:
        return node

    for child in node.get_children():
        var found := _find_camera_recursive(child)
        if found:
            return found
    return null

func _find_interactable_from_node(node: Node) -> SeedInteractable:
    var current := node
    while current:
        if current is SeedInteractable:
            return current

        for child in current.get_children():
            if child is SeedInteractable:
                return child

        current = current.get_parent()

    return null
