extends CanvasLayer

@export var interactor_path: NodePath
@export var inventory_path: NodePath

@onready var prompt_label: Label = $Prompt
@onready var status_label: Label = $Status
@onready var inventory_label: Label = $Inventory

var _interactor: SeedPlayerInteractor
var _inventory: SeedInventory
var _status_timer: SceneTreeTimer

func _ready() -> void:
    _interactor = get_node_or_null(interactor_path) as SeedPlayerInteractor
    _inventory = get_node_or_null(inventory_path) as SeedInventory

    if _interactor:
        _interactor.interactable_focused.connect(_on_interactable_focused)
        _interactor.interactable_cleared.connect(_on_interactable_cleared)

    if _inventory:
        _inventory.item_added.connect(_on_item_added)
        _inventory.item_removed.connect(_on_item_removed)
        _inventory.inventory_changed.connect(_refresh_inventory)

    _connect_door_feedback(get_tree().current_scene)
    _on_interactable_cleared()
    _refresh_inventory()

func _on_interactable_focused(interactable: SeedInteractable) -> void:
    prompt_label.text = "[E] %s" % interactable.prompt_text
    prompt_label.visible = true

func _on_interactable_cleared() -> void:
    prompt_label.visible = false

func _on_item_added(item_id: StringName, quantity: int, _new_total: int) -> void:
    _show_status("Picked up %s ×%d" % [String(item_id), quantity])

func _on_item_removed(item_id: StringName, quantity: int, _new_total: int) -> void:
    _show_status("Used %s ×%d" % [String(item_id), quantity])

func _on_door_denied(_interactor_node: Node, reason: String) -> void:
    _show_status("Locked — %s" % reason)

func _refresh_inventory() -> void:
    if _inventory == null:
        inventory_label.text = "Inventory: unavailable"
        return

    var items := _inventory.get_items()
    if items.is_empty():
        inventory_label.text = "Inventory: empty"
        return

    var parts := PackedStringArray()
    for item_id in items.keys():
        parts.append("%s ×%d" % [String(item_id), int(items[item_id])])
    inventory_label.text = "Inventory: %s" % ", ".join(parts)

func _show_status(message: String) -> void:
    status_label.text = message
    status_label.visible = true

    _status_timer = get_tree().create_timer(2.2)
    var timer := _status_timer
    timer.timeout.connect(func() -> void:
        if _status_timer == timer:
            status_label.visible = false
    )

func _connect_door_feedback(node: Node) -> void:
    if node is SeedDoor:
        var door := node as SeedDoor
        if not door.interaction_denied.is_connected(_on_door_denied):
            door.interaction_denied.connect(_on_door_denied)

    for child in node.get_children():
        _connect_door_feedback(child)
