@tool
extends EditorPlugin

const INTERACTABLE_SCRIPT := preload("res://addons/seed_engine/runtime/components/seed_interactable.gd")
const INVENTORY_SCRIPT := preload("res://addons/seed_engine/runtime/components/seed_inventory.gd")
const DOOR_SCRIPT := preload("res://addons/seed_engine/runtime/components/seed_door.gd")
const HEALTH_SCRIPT := preload("res://addons/seed_engine/runtime/components/seed_health.gd")
const PICKUP_SCRIPT := preload("res://addons/seed_engine/runtime/components/seed_pickup.gd")
const PLAYER_INTERACTOR_SCRIPT := preload("res://addons/seed_engine/runtime/player/seed_player_interactor.gd")

var dock: Control

func _enter_tree() -> void:
    _ensure_default_input()
    dock = _build_dock()
    add_control_to_dock(DOCK_SLOT_RIGHT_UL, dock)

func _exit_tree() -> void:
    if dock:
        remove_control_from_docks(dock)
        dock.queue_free()

func _build_dock() -> Control:
    var root := VBoxContainer.new()
    root.name = "Seed"
    root.custom_minimum_size = Vector2(280, 0)

    var heading := Label.new()
    heading.text = "Add Gameplay"
    heading.add_theme_font_size_override("font_size", 18)
    root.add_child(heading)

    var hint := Label.new()
    hint.text = "Select an object, then choose what it can do."
    hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    root.add_child(hint)

    root.add_child(HSeparator.new())

    var categories := [
        ["Interaction", "Make the selected object interactable."],
        ["Inventory", "Give the selected object an item inventory."],
        ["Door", "Add an opening/closing door with optional key requirement."],
        ["Health", "Give the selected object health and damage support."],
        ["Pickup", "Let the player collect the selected object into inventory."],
        ["Player Interaction", "Let the selected player interact with objects from its camera."],
    ]

    for item in categories:
        var button := Button.new()
        button.text = item[0]
        button.tooltip_text = item[1]
        button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        button.pressed.connect(_on_gameplay_requested.bind(item[0]))
        root.add_child(button)

    root.add_child(HSeparator.new())

    var status := Label.new()
    status.name = "Status"
    status.text = "Seed v0.1: first gameplay slice ready to author."
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    root.add_child(status)

    return root

func _on_gameplay_requested(gameplay_type: String) -> void:
    var selection := get_editor_interface().get_selection().get_selected_nodes()
    if selection.is_empty():
        _set_status("Select an object first, then choose %s." % gameplay_type)
        return

    var selected := selection[0]
    var scene_root := get_editor_interface().get_edited_scene_root()
    if scene_root == null:
        _set_status("Open or create a scene before adding Seed gameplay.")
        return

    match gameplay_type:
        "Interaction":
            _add_component(selected, "SeedInteractable", INTERACTABLE_SCRIPT, scene_root)
        "Inventory":
            _add_component(selected, "SeedInventory", INVENTORY_SCRIPT, scene_root)
        "Health":
            _add_component(selected, "SeedHealth", HEALTH_SCRIPT, scene_root)
        "Player Interaction":
            _add_component(selected, "SeedPlayerInteractor", PLAYER_INTERACTOR_SCRIPT, scene_root)
        "Door":
            _ensure_component(selected, "SeedInteractable", INTERACTABLE_SCRIPT, scene_root)
            _add_component(selected, "SeedDoor", DOOR_SCRIPT, scene_root)
        "Pickup":
            _ensure_component(selected, "SeedInteractable", INTERACTABLE_SCRIPT, scene_root)
            _add_component(selected, "SeedPickup", PICKUP_SCRIPT, scene_root)
        _:
            _set_status("%s is not implemented yet." % gameplay_type)

func _add_component(host: Node, node_name: String, script: Script, scene_root: Node) -> Node:
    var existing := host.get_node_or_null(NodePath(node_name))
    if existing:
        _set_status("%s already has %s." % [host.name, _friendly_component_name(node_name)])
        get_editor_interface().get_selection().clear()
        get_editor_interface().get_selection().add_node(existing)
        return existing

    var component := Node.new()
    component.name = node_name
    component.set_script(script)
    host.add_child(component)
    component.owner = scene_root

    get_editor_interface().get_selection().clear()
    get_editor_interface().get_selection().add_node(component)
    _set_status("Added %s to %s. Configure it in the Inspector." % [_friendly_component_name(node_name), host.name])
    return component

func _ensure_component(host: Node, node_name: String, script: Script, scene_root: Node) -> Node:
    var existing := host.get_node_or_null(NodePath(node_name))
    if existing:
        return existing

    var component := Node.new()
    component.name = node_name
    component.set_script(script)
    host.add_child(component)
    component.owner = scene_root
    return component

func _friendly_component_name(node_name: String) -> String:
    return node_name.trim_prefix("Seed")

func _set_status(message: String) -> void:
    if dock == null:
        return
    var status := dock.get_node_or_null("Status") as Label
    if status:
        status.text = message

func _ensure_default_input() -> void:
    if ProjectSettings.has_setting("input/seed_interact"):
        return

    var key_event := InputEventKey.new()
    key_event.physical_keycode = KEY_E

    ProjectSettings.set_setting("input/seed_interact", {
        "deadzone": 0.5,
        "events": [key_event],
    })
    ProjectSettings.save()
