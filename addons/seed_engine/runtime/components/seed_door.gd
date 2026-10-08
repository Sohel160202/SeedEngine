@tool
extends Node
class_name SeedDoor

enum OpenMode {
    ROTATE_Y,
    SLIDE
}

signal opened(interactor: Node)
signal closed(interactor: Node)
signal interaction_denied(interactor: Node, reason: String)

@export_category("Door")
@export var open_mode: OpenMode = OpenMode.ROTATE_Y
@export_range(-180.0, 180.0, 1.0) var open_angle_degrees: float = 90.0
@export var slide_offset: Vector3 = Vector3(1.5, 0.0, 0.0)
@export_range(0.0, 10.0, 0.05) var duration: float = 1.0
@export var starts_open: bool = false

@export_category("Lock")
@export var required_item_id: StringName = &""
@export var consume_required_item: bool = false

@export_category("Advanced")
@export var target_path: NodePath = NodePath("..")

var _interactable: SeedInteractable
var _target: Node3D
var _closed_position: Vector3
var _closed_rotation: Vector3
var _open_position: Vector3
var _open_rotation: Vector3
var _is_open: bool = false
var _active_tween: Tween

func _ready() -> void:
    _target = get_node_or_null(target_path) as Node3D
    _interactable = _find_sibling_interactable()

    if _target == null:
        push_warning("SeedDoor requires a Node3D target.")
        return

    _closed_position = _target.position
    _closed_rotation = _target.rotation
    _open_position = _closed_position + slide_offset
    _open_rotation = _closed_rotation + Vector3(0.0, deg_to_rad(open_angle_degrees), 0.0)

    _is_open = starts_open
    _apply_state_immediately()

    if _interactable and not _interactable.interacted.is_connected(_on_interacted):
        _interactable.interacted.connect(_on_interacted)

func toggle(interactor: Node = null) -> bool:
    if _target == null:
        return false

    if not _is_open and not _can_open(interactor):
        interaction_denied.emit(interactor, "Requires item: %s" % String(required_item_id))
        return false

    if not _is_open and consume_required_item and required_item_id != StringName():
        var inventory := _find_inventory(interactor)
        if inventory == null or not inventory.remove_item(required_item_id, 1):
            interaction_denied.emit(interactor, "Required item could not be consumed.")
            return false

    _is_open = not _is_open
    _animate_to_state()

    if _is_open:
        opened.emit(interactor)
    else:
        closed.emit(interactor)

    return true

func open(interactor: Node = null) -> bool:
    if _is_open:
        return true
    return toggle(interactor)

func close(interactor: Node = null) -> bool:
    if not _is_open:
        return true
    return toggle(interactor)

func is_open() -> bool:
    return _is_open

func _on_interacted(interactor: Node) -> void:
    toggle(interactor)

func _can_open(interactor: Node) -> bool:
    if required_item_id == StringName():
        return true

    var inventory := _find_inventory(interactor)
    return inventory != null and inventory.has_item(required_item_id, 1)

func _animate_to_state() -> void:
    if _active_tween and _active_tween.is_valid():
        _active_tween.kill()

    var destination_position := _open_position if _is_open else _closed_position
    var destination_rotation := _open_rotation if _is_open else _closed_rotation

    if duration <= 0.0 or not is_inside_tree():
        _apply_state_immediately()
        return

    _active_tween = create_tween()
    _active_tween.set_trans(Tween.TRANS_SINE)
    _active_tween.set_ease(Tween.EASE_IN_OUT)

    match open_mode:
        OpenMode.ROTATE_Y:
            _active_tween.tween_property(_target, "rotation", destination_rotation, duration)
        OpenMode.SLIDE:
            _active_tween.tween_property(_target, "position", destination_position, duration)

func _apply_state_immediately() -> void:
    if _target == null:
        return

    match open_mode:
        OpenMode.ROTATE_Y:
            _target.rotation = _open_rotation if _is_open else _closed_rotation
        OpenMode.SLIDE:
            _target.position = _open_position if _is_open else _closed_position

func _find_sibling_interactable() -> SeedInteractable:
    var host := get_parent()
    if host == null:
        return null

    for child in host.get_children():
        if child is SeedInteractable:
            return child
    return null

func _find_inventory(node: Node) -> SeedInventory:
    var current := node
    while current:
        if current is SeedInventory:
            return current

        for child in current.get_children():
            if child is SeedInventory:
                return child

        current = current.get_parent()

    return null
