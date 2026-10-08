@tool
extends EditorPlugin

var dock: Control

func _enter_tree() -> void:
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
    hint.text = "Select an object, then add what it can do."
    hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    root.add_child(hint)

    root.add_child(HSeparator.new())

    var categories := [
        ["Interaction", "Make the selected object interactable."],
        ["Door", "Turn the selected object into an opening/closing door."],
        ["Health", "Give the selected object health and damage support."],
        ["Pickup", "Let the player collect the selected object."],
        ["Enemy", "Add beginner-friendly enemy behaviour."],
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
    status.text = "Prototype: workflow shell"
    status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    root.add_child(status)

    return root

func _on_gameplay_requested(gameplay_type: String) -> void:
    var selection := get_editor_interface().get_selection().get_selected_nodes()
    var status := dock.get_node("Status") as Label

    if selection.is_empty():
        status.text = "Select an object first, then choose %s." % gameplay_type
        return

    var selected := selection[0]
    status.text = "%s selected for %s. Component authoring comes next." % [selected.name, gameplay_type]
