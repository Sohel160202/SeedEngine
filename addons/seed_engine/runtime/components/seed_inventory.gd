@tool
extends Node
class_name SeedInventory

signal item_added(item_id: StringName, quantity: int, new_total: int)
signal item_removed(item_id: StringName, quantity: int, new_total: int)
signal inventory_changed

@export_category("Inventory")
@export var starting_items: Dictionary = {}

var _items: Dictionary = {}

func _ready() -> void:
    _items = starting_items.duplicate(true)
    _sanitize_inventory()

func add_item(item_id: StringName, quantity: int = 1) -> bool:
    if item_id == StringName() or quantity <= 0:
        return false

    var current := get_item_count(item_id)
    var new_total := current + quantity
    _items[item_id] = new_total
    item_added.emit(item_id, quantity, new_total)
    inventory_changed.emit()
    return true

func has_item(item_id: StringName, quantity: int = 1) -> bool:
    if quantity <= 0:
        return true
    return get_item_count(item_id) >= quantity

func remove_item(item_id: StringName, quantity: int = 1) -> bool:
    if item_id == StringName() or quantity <= 0:
        return false

    var current := get_item_count(item_id)
    if current < quantity:
        return false

    var new_total := current - quantity
    if new_total == 0:
        _items.erase(item_id)
    else:
        _items[item_id] = new_total

    item_removed.emit(item_id, quantity, new_total)
    inventory_changed.emit()
    return true

func get_item_count(item_id: StringName) -> int:
    return int(_items.get(item_id, 0))

func get_items() -> Dictionary:
    return _items.duplicate(true)

func clear() -> void:
    if _items.is_empty():
        return
    _items.clear()
    inventory_changed.emit()

func _sanitize_inventory() -> void:
    var sanitized: Dictionary = {}
    for raw_key in _items.keys():
        var key := StringName(str(raw_key))
        var quantity := maxi(0, int(_items[raw_key]))
        if key != StringName() and quantity > 0:
            sanitized[key] = quantity
    _items = sanitized
