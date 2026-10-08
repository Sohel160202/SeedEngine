@tool
extends Node
class_name SeedPickup

signal picked_up(interactor: Node, item_id: StringName, quantity: int)
signal pickup_failed(interactor: Node, reason: String)

@export_category("Pickup")
@export var item_id: StringName = &"Item"
@export var display_name: String = "Item"
@export_range(1, 9999, 1) var quantity: int = 1
@export var remove_object_after_pickup: bool = true

var _interactable: SeedInteractable

func _ready() -> void:
    _interactable = _find_sibling_interactable()
    if _interactable and not _interactable.interacted.is_connected(_on_interacted):
        _interactable.interacted.connect(_on_interacted)

func _on_interacted(interactor: Node) -> void:
    var inventory := _find_inventory(interactor)
    if inventory == null:
        pickup_failed.emit(interactor, "Interactor has no SeedInventory component.")
        return

    if not inventory.add_item(item_id, quantity):
        pickup_failed.emit(interactor, "Item could not be added to inventory.")
        return

    picked_up.emit(interactor, item_id, quantity)

    if remove_object_after_pickup:
        var host := get_parent()
        if host:
            host.queue_free()

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
