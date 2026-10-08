@tool
extends Node
class_name SeedInteractable

signal interacted(interactor: Node)
signal enabled_changed(is_enabled: bool)

@export_category("Interaction")
@export var prompt_text: String = "Interact"
@export var interaction_enabled: bool = true:
    set(value):
        interaction_enabled = value
        enabled_changed.emit(interaction_enabled)

func can_interact(_interactor: Node = null) -> bool:
    return interaction_enabled

func interact(interactor: Node) -> bool:
    if not can_interact(interactor):
        return false

    interacted.emit(interactor)
    return true
