@tool
extends Node
class_name SeedHealth

signal health_changed(current_health: float, max_health: float)
signal damaged(amount: float, instigator: Node)
signal healed(amount: float)
signal died(instigator: Node)

@export_category("Health")
@export_range(1.0, 1000000.0, 1.0) var max_health: float = 100.0
@export var start_full: bool = true
@export_range(0.0, 1000000.0, 1.0) var starting_health: float = 100.0
@export var invulnerable: bool = false

var current_health: float = 0.0
var is_dead: bool = false

func _ready() -> void:
    current_health = max_health if start_full else clampf(starting_health, 0.0, max_health)
    is_dead = current_health <= 0.0
    health_changed.emit(current_health, max_health)

func apply_damage(amount: float, instigator: Node = null) -> float:
    if invulnerable or is_dead or amount <= 0.0:
        return 0.0

    var previous := current_health
    current_health = maxf(0.0, current_health - amount)
    var applied := previous - current_health

    damaged.emit(applied, instigator)
    health_changed.emit(current_health, max_health)

    if current_health <= 0.0:
        is_dead = true
        died.emit(instigator)

    return applied

func heal(amount: float) -> float:
    if is_dead or amount <= 0.0:
        return 0.0

    var previous := current_health
    current_health = minf(max_health, current_health + amount)
    var applied := current_health - previous

    if applied > 0.0:
        healed.emit(applied)
        health_changed.emit(current_health, max_health)

    return applied

func restore_full() -> void:
    current_health = max_health
    is_dead = false
    health_changed.emit(current_health, max_health)

func get_health_normalized() -> float:
    if max_health <= 0.0:
        return 0.0
    return current_health / max_health
