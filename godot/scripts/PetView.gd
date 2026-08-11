## PetView.gd
## Escena principal de la criatura activa.
##
## Muestra los stats en tiempo real, maneja las acciones del jugador
## y recibe señales del nodo C++ PetCore.
##
## La lógica de juego NO vive aquí — vive en PetCore (C++ GDExtension).
## Este script es solo glue: escucha señales y actualiza la UI.

extends Node2D

# ---------------------------------------------------------------------------
# Nodos (se conectan en _ready)
# ---------------------------------------------------------------------------

@onready var sprite: AnimatedSprite2D     = $PetSprite
@onready var bar_energia: ProgressBar     = $HUD/Bars/Energia/Bar
@onready var bar_animo: ProgressBar       = $HUD/Bars/Animo/Bar
@onready var bar_salud: ProgressBar       = $HUD/Bars/Salud/Bar
@onready var bar_vinculo: ProgressBar     = $HUD/Bars/Vinculo/Bar
@onready var lbl_nombre: Label            = $HUD/Nombre
@onready var lbl_etapa: Label             = $HUD/Etapa
@onready var lbl_forma: Label             = $HUD/Forma
@onready var dialog_box: Control          = $DialogBox
@onready var lbl_dialog: RichTextLabel   = $DialogBox/Text
@onready var btn_alimentar: Button        = $ActionMenu/Alimentar
@onready var btn_jugar: Button            = $ActionMenu/Jugar
@onready var btn_acariciar: Button        = $ActionMenu/Acariciar
@onready var btn_expedicion: Button       = $ActionMenu/Expedicion
@onready var btn_codex: Button            = $ActionMenu/Codex
@onready var pet_core: Node               = $PetCore  # Nodo C++ GDExtension

# ---------------------------------------------------------------------------
# Estado local de UI
# ---------------------------------------------------------------------------

var _dialog_queue: Array[String] = []
var _dialog_timer: float = 0.0
const DIALOG_DURATION := 3.0
const TYPEWRITER_SPEED := 0.03 # segundos por caracter

# ---------------------------------------------------------------------------
# _ready
# ---------------------------------------------------------------------------

func _ready() -> void:
	# Conectar señales del núcleo C++
	pet_core.pet_evolved.connect(_on_evolved)
	pet_core.expedition_returned.connect(_on_expedition_returned)
	pet_core.lethargy_entered.connect(_on_lethargy_entered)
	pet_core.stats_updated.connect(_on_stats_updated)

	# Conectar botones
	btn_alimentar.pressed.connect(_on_btn_alimentar)
	btn_jugar.pressed.connect(_on_btn_jugar)
	btn_acariciar.pressed.connect(_on_btn_acariciar)
	btn_expedicion.pressed.connect(_on_btn_expedicion)
	btn_codex.pressed.connect(_on_btn_codex)

	# Ocultar dialog al inicio
	dialog_box.hide()

	# Primer render
	_refresh_ui()

# ---------------------------------------------------------------------------
# _process
# ---------------------------------------------------------------------------

func _process(delta: float) -> void:
	# Actualizar simulación (el PetCore lo hace por tick, no cada frame)
	pet_core.update(Time.get_ticks_msec())

	# Dialog queue
	if _dialog_timer > 0.0:
		_dialog_timer -= delta
		if _dialog_timer <= 0.0:
			dialog_box.hide()
			if _dialog_queue.size() > 0:
				_show_dialog(_dialog_queue.pop_front())

# ---------------------------------------------------------------------------
# Señales del PetCore (C++)
# ---------------------------------------------------------------------------

func _on_stats_updated(_stats: Dictionary) -> void:
	_refresh_ui()

func _on_evolved(new_form: String, new_stage: String) -> void:
	sprite.play("evolve")
	await sprite.animation_finished
	_show_dialog("¡Evolucionó a " + new_form + "!")
	_refresh_ui()

func _on_expedition_returned(destino: String, botin: String) -> void:
	sprite.play("happy")
	_show_dialog("Volvió " + destino + ". " + botin)
	_refresh_ui()

func _on_lethargy_entered() -> void:
	sprite.play("sad")
	_show_dialog("Cayó en letargo... Necesita atención.")

# ---------------------------------------------------------------------------
# Botones de acción
# ---------------------------------------------------------------------------

func _on_btn_alimentar() -> void:
	# Abre el submenú de comida (se implementa en Fase 2)
	get_tree().change_scene_to_file("res://scenes/ui/FoodMenu.tscn")

func _on_btn_jugar() -> void:
	const result: Dictionary = pet_core.play()
	if result.ok:
		sprite.play("happy")
		_show_dialog(result.message)
	else:
		_show_dialog(result.reason)
	_refresh_ui()

func _on_btn_acariciar() -> void:
	const result: Dictionary = pet_core.pet()
	if result.ok:
		sprite.play("happy")
		_show_dialog(result.message)
	else:
		_show_dialog(result.reason)
	_refresh_ui()

func _on_btn_expedicion() -> void:
	get_tree().change_scene_to_file("res://scenes/world/ExpeditionMenu.tscn")

func _on_btn_codex() -> void:
	get_tree().change_scene_to_file("res://scenes/codex/Codex.tscn")

# ---------------------------------------------------------------------------
# Helpers de UI
# ---------------------------------------------------------------------------

func _refresh_ui() -> void:
	const stats: Dictionary = pet_core.get_stats()
	const info: Dictionary  = pet_core.get_info()

	bar_energia.value = stats.get("energia", 0.0)
	bar_animo.value   = stats.get("animo",   0.0)
	bar_salud.value   = stats.get("salud",   0.0)
	bar_vinculo.value = stats.get("vinculo", 0.0)

	lbl_nombre.text = info.get("nombre", "???")
	lbl_etapa.text  = info.get("etapa",  "")
	lbl_forma.text  = info.get("forma",  "")

	# Colorear barras según nivel
	_set_bar_color(bar_energia, stats.get("energia", 100.0))
	_set_bar_color(bar_animo,   stats.get("animo",   100.0))
	_set_bar_color(bar_salud,   stats.get("salud",   100.0))

	# Animación idle según estado
	const letargico: bool = info.get("letargico", false)
	if letargico:
		sprite.play("sad")
	elif stats.get("animo", 100.0) < 25.0:
		sprite.play("sad")
	else:
		sprite.play("idle")

func _set_bar_color(bar: ProgressBar, value: float) -> void:
	const style = bar.get_theme_stylebox("fill").duplicate()
	if value < 25.0:
		style.bg_color = Color(0.9, 0.2, 0.2)  # rojo
	elif value < 50.0:
		style.bg_color = Color(0.9, 0.7, 0.1)  # amarillo
	else:
		style.bg_color = Color(0.2, 0.8, 0.3)  # verde
	bar.add_theme_stylebox_override("fill", style)

func _show_dialog(text: String) -> void:
	lbl_dialog.text = ""
	dialog_box.show()
	_dialog_timer = DIALOG_DURATION

	# Efecto typewriter
	var i := 0
	while i < text.length():
		lbl_dialog.text += text[i]
		i += 1
		await get_tree().create_timer(TYPEWRITER_SPEED).timeout
