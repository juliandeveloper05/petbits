## Inicio.gd — los créditos y la pantalla de título.
##
## Es lo primero que se ve al abrir el juego. Antes no había nada de esto: se caía
## directo en la ficha de la criatura, con un código de dieciséis caracteres sin
## rótulo y diez botones, sin que nada dijera qué era esto ni qué había que hacer.
##
## ---
##
## LOS CRÉDITOS SE PUEDEN SALTEAR, Y SOLO SE VEN AL ABRIR EL PROGRAMA.
##
## Cualquier tecla los corta. Y si se llega acá apretando Esc desde la ficha, se
## va derecho al menú: ver "Un juego de…" cada vez que uno vuelve al título es
## la manera más rápida de que lo odien.
##
## ---
##
## TRES CRIATURAS, CADA UNA CON SU SEMILLA ABAJO.
##
## Se generan en el momento, distintas cada vez que se abre el título. Es la idea
## entera del juego dicha sin una palabra: esto es un número, y ese número es
## esta criatura. Antes esa idea vivía solamente en la descripción del proyecto,
## que el jugador no ve nunca.
##
## ---
##
## "NUEVA PARTIDA" NO BORRA NADA.
##
## Si ya había una, pide confirmación y la aparta con la fecha en el nombre. Es la
## misma regla que la cuarentena de un save roto: la partida de alguien no la
## borra el programa. Y si no se la puede apartar, NO se empieza otra, porque se
## escribiría encima.

extends Control

const Tema = preload("res://scripts/Tema.gd")

const FONDO := Color("#0a0e0a")
const BORDE := Color("#3d5c46")
const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")
const AVISO := Color("#ffc23d")

const FORMAS := ["coloso", "vaporoso", "oraculo", "guardian", "petreo", "errante"]

## Cuánto dura cada cartel de los créditos: entrar, quedarse, salir.
const CREDITOS := [
	{"lineas": [["Un juego de", 1, TENUE], ["Julian Soto", 2, TEXTO]], "entra": 0.7, "queda": 1.5, "sale": 0.6},
	{"lineas": [["Hecho con Godot y C++", 1, TENUE]], "entra": 0.5, "queda": 0.9, "sale": 0.5},
]

var _creditos: Control = null
var _titulo: Control = null
var _menu: VBoxContainer = null
var _confirmacion: PanelContainer = null
var _error: Label = null

var _criaturas: Array = []
var _tiempo := 0.0

## Después de saltear los créditos se ignora el teclado un momento: la misma
## tecla que los cortó no tiene que apretar también el primer botón del menú.
var _sordo_hasta := 0.0

var _en_creditos := false
var _tween: Tween = null


func _ready() -> void:
	if not Partida.preparar():
		_sin_extension()
		return


	var fondo := ColorRect.new()
	fondo.color = FONDO
	fondo.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(fondo)

	_construir_titulo()

	# Volviendo desde el juego no hay créditos: derecho al menú.
	if Partida.partida_cargada():
		_mostrar_titulo()
	else:
		_pasar_creditos()

	set_process(true)


# ---------------------------------------------------------------------------
# Créditos
# ---------------------------------------------------------------------------

func _pasar_creditos() -> void:
	_en_creditos = true
	_titulo.visible = false

	_creditos = Control.new()
	_creditos.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_creditos)

	_tween = create_tween()
	for cartel in CREDITOS:
		var caja := _cartel_centrado(cartel["lineas"])
		caja.modulate.a = 0.0
		_creditos.add_child(caja)
		_tween.tween_property(caja, "modulate:a", 1.0, cartel["entra"])
		_tween.tween_interval(cartel["queda"])
		_tween.tween_property(caja, "modulate:a", 0.0, cartel["sale"])
	_tween.tween_callback(_terminar_creditos)


func _terminar_creditos() -> void:
	if not _en_creditos:
		return
	_en_creditos = false
	if _tween != null:
		_tween.kill()
	if _creditos != null:
		_creditos.queue_free()
		_creditos = null
	_sordo_hasta = _tiempo + 0.3
	_mostrar_titulo()


## Un bloque de renglones centrado en la pantalla.
func _cartel_centrado(lineas: Array) -> VBoxContainer:
	var caja := VBoxContainer.new()
	caja.set_anchors_preset(Control.PRESET_FULL_RECT)
	caja.alignment = BoxContainer.ALIGNMENT_CENTER
	caja.add_theme_constant_override("separation", 6)
	for l in lineas:
		caja.add_child(_texto(l[0], l[1], l[2]))
	return caja


# ---------------------------------------------------------------------------
# El título
# ---------------------------------------------------------------------------

func _construir_titulo() -> void:
	_titulo = Control.new()
	_titulo.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(_titulo)

	var columna := VBoxContainer.new()
	columna.set_anchors_preset(Control.PRESET_FULL_RECT)
	columna.offset_top = 14
	columna.offset_bottom = -8
	columna.add_theme_constant_override("separation", 4)
	_titulo.add_child(columna)

	columna.add_child(_texto("PetBits", 4, FOSFORO))
	columna.add_child(_texto("Cada criatura es una semilla de 64 bits.", 1, TENUE))

	var aire := Control.new()
	aire.custom_minimum_size = Vector2(0, 4)
	columna.add_child(aire)

	columna.add_child(_fila_de_criaturas())

	var aire2 := Control.new()
	aire2.custom_minimum_size = Vector2(0, 6)
	columna.add_child(aire2)

	var centro := CenterContainer.new()
	columna.add_child(centro)

	_menu = VBoxContainer.new()
	_menu.add_theme_constant_override("separation", 3)
	_menu.custom_minimum_size = Vector2(140, 0)
	centro.add_child(_menu)

	_error = _texto("", 1, AVISO)
	_error.autowrap_mode = TextServer.AUTOWRAP_WORD
	_error.custom_minimum_size = Vector2(440, 0)
	columna.add_child(_error)

	# Abajo: la versión a la derecha y los controles a la izquierda.
	var version := _texto(_version(), 1, BORDE)
	version.set_anchors_preset(Control.PRESET_BOTTOM_RIGHT)
	version.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	version.offset_left = -200
	version.offset_top = -16
	version.offset_right = -6
	version.offset_bottom = -3
	_titulo.add_child(version)

	var ayuda := _texto("Flechas y Enter", 1, BORDE)
	ayuda.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	ayuda.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	ayuda.offset_left = 6
	ayuda.offset_top = -16
	ayuda.offset_right = 200
	ayuda.offset_bottom = -3
	_titulo.add_child(ayuda)


## Tres criaturas al azar, cada una con su semilla abajo.
func _fila_de_criaturas() -> HBoxContainer:
	var fila := HBoxContainer.new()
	fila.alignment = BoxContainer.ALIGNMENT_CENTER
	fila.add_theme_constant_override("separation", 26)

	var formas := FORMAS.duplicate()
	formas.shuffle()

	for i in 3:
		var seed: String = Partida.core.seed_al_azar()
		var forma: String = formas[i]

		var columna := VBoxContainer.new()
		columna.add_theme_constant_override("separation", 0)
		fila.add_child(columna)

		# El sprite va adentro de un Control fijo para poder moverlo de arriba
		# abajo sin que el contenedor lo vuelva a acomodar en cada cuadro.
		var marco := Control.new()
		marco.custom_minimum_size = Vector2(64, 66)
		columna.add_child(marco)

		var sprite := TextureRect.new()
		sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		sprite.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
		sprite.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		sprite.size = Vector2(64, 64)
		marco.add_child(sprite)

		var abierta: Image = Partida.core.sprite(seed, "adulto", forma, false)
		var cerrada: Image = Partida.core.sprite(seed, "adulto", forma, true)
		var t_abierta := ImageTexture.create_from_image(abierta) if abierta != null else null
		var t_cerrada := ImageTexture.create_from_image(cerrada) if cerrada != null else null
		sprite.texture = t_abierta

		columna.add_child(_texto(seed, 1, TENUE))

		_criaturas.append({
			"sprite": sprite,
			"abierta": t_abierta,
			"cerrada": t_cerrada,
			"fase": i * 2.1,
			"parpadeo": randf_range(1.5, 4.5),
		})
	return fila


func _mostrar_titulo() -> void:
	_titulo.visible = true
	_armar_menu()


func _armar_menu() -> void:
	for hijo in _menu.get_children():
		hijo.queue_free()

	var primero: Button = null
	if Partida.partida_cargada() or Partida.hay_partida_guardada():
		primero = _boton("Continuar", _continuar)
	var nueva := _boton("Nueva partida", _nueva)
	if primero == null:
		primero = nueva
	_boton("Salir", _salir)

	# El foco va al primer botón: sin esto, el teclado y el joystick no tienen
	# desde dónde empezar y el menú solo anda con el mouse.
	primero.call_deferred("grab_focus")


func _boton(texto: String, al_apretar: Callable) -> Button:
	var boton := Button.new()
	boton.text = texto
	boton.pressed.connect(al_apretar)
	_menu.add_child(boton)
	return boton


# ---------------------------------------------------------------------------
# Lo que hace cada opción
# ---------------------------------------------------------------------------

func _continuar() -> void:
	get_tree().change_scene_to_file("res://scenes/PetView.tscn")


func _nueva() -> void:
	if Partida.partida_cargada() or Partida.hay_partida_guardada():
		_confirmar()
	else:
		_empezar_nueva()


func _empezar_nueva() -> void:
	# Si se estaba jugando, primero se guarda lo último y se suelta: el archivo
	# que se aparta tiene que ser el de ahora, no el del último tick.
	Partida.descargar()

	if Partida.hay_partida_guardada() and not Partida.archivar_partida():
		_cerrar_confirmacion()
		_error.text = (
			"No se pudo apartar la partida anterior, así que no se empezó otra: "
			+ "se la habría pisado. Cerrá lo que esté usando el archivo y probá de nuevo."
		)
		return

	get_tree().change_scene_to_file("res://scenes/PetView.tscn")


func _salir() -> void:
	# `quit()` no manda el aviso de cierre de ventana, así que se guarda acá.
	# Los dos están protegidos: si no se cargó ninguna partida, no escriben nada.
	Partida.guardar()
	Partida.guardar_mundo()
	get_tree().quit()


# ---------------------------------------------------------------------------
# La confirmación
# ---------------------------------------------------------------------------

func _confirmar() -> void:
	_menu.visible = false

	_confirmacion = PanelContainer.new()
	_confirmacion.set_anchors_preset(Control.PRESET_CENTER)
	_confirmacion.grow_horizontal = Control.GROW_DIRECTION_BOTH
	_confirmacion.grow_vertical = Control.GROW_DIRECTION_BOTH
	add_child(_confirmacion)

	var caja := VBoxContainer.new()
	caja.add_theme_constant_override("separation", 8)
	_confirmacion.add_child(caja)

	var pregunta := _texto(
		"Ya hay una partida. Si empezás otra, la de ahora no se borra: queda guardada aparte.",
		1, TEXTO
	)
	pregunta.autowrap_mode = TextServer.AUTOWRAP_WORD
	pregunta.custom_minimum_size = Vector2(300, 0)
	caja.add_child(pregunta)

	var fila := HBoxContainer.new()
	fila.alignment = BoxContainer.ALIGNMENT_CENTER
	fila.add_theme_constant_override("separation", 8)
	caja.add_child(fila)

	var volver := Button.new()
	volver.text = "Volver"
	volver.pressed.connect(_cerrar_confirmacion)
	fila.add_child(volver)

	var si := Button.new()
	si.text = "Empezar otra"
	si.pressed.connect(_empezar_nueva)
	fila.add_child(si)

	# Por defecto, la opción que no hace nada. Un Enter apurado no tiene que
	# empezar una partida nueva.
	volver.call_deferred("grab_focus")


func _cerrar_confirmacion() -> void:
	if _confirmacion != null:
		_confirmacion.queue_free()
		_confirmacion = null
	_menu.visible = true
	_armar_menu()


# ---------------------------------------------------------------------------
# Entrada y animación
# ---------------------------------------------------------------------------

func _input(evento: InputEvent) -> void:
	if _en_creditos:
		var corta: bool = (
			(evento is InputEventKey and evento.pressed and not evento.echo)
			or (evento is InputEventMouseButton and evento.pressed)
			or (evento is InputEventJoypadButton and evento.pressed)
		)
		if corta:
			get_viewport().set_input_as_handled()
			_terminar_creditos()
		return

	if _tiempo < _sordo_hasta and (evento is InputEventKey or evento is InputEventJoypadButton):
		get_viewport().set_input_as_handled()
		return

	if evento.is_action_pressed("action_cancel") and _confirmacion != null:
		get_viewport().set_input_as_handled()
		_cerrar_confirmacion()


func _process(delta: float) -> void:
	_tiempo += delta
	for c in _criaturas:
		# Un rebote de dos píxeles, en píxeles enteros: una posición con decimales
		# haría titilar el pixel art.
		c["sprite"].position.y = roundf(sin(_tiempo * 2.2 + c["fase"]) * 2.0)

		c["parpadeo"] -= delta
		if c["parpadeo"] <= 0.0:
			var cerrada: bool = c["sprite"].texture == c["cerrada"]
			c["sprite"].texture = c["abierta"] if cerrada else c["cerrada"]
			c["parpadeo"] = randf_range(2.5, 5.5) if cerrada else 0.14


# ---------------------------------------------------------------------------
# Utilidades
# ---------------------------------------------------------------------------

## Un texto centrado. `escala` es un MULTIPLICADOR entero de la fuente: a 1 se
## dibuja tal cual, a 2 se duplica cada píxel. Cualquier otro número la
## interpola y le rompe los trazos.
func _texto(contenido: String, escala: int, color: Color) -> Label:
	var etiqueta := Label.new()
	etiqueta.text = contenido
	etiqueta.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_font_size_override("font_size", Partida.tam_fuente * escala)
	return etiqueta


func _version() -> String:
	var v: String = ProjectSettings.get_setting("application/config/version", "")
	if OS.is_debug_build():
		return "v%s · versión de prueba" % v
	return "v%s" % v


func _sin_extension() -> void:
	var etiqueta := Label.new()
	etiqueta.text = (
		"No se pudo cargar el motor del juego.\n"
		+ "Si lo abriste desde adentro del .zip, descomprimilo primero\n"
		+ "y abrí PetBits.exe desde la carpeta."
	)
	etiqueta.set_anchors_preset(Control.PRESET_FULL_RECT)
	etiqueta.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	etiqueta.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	etiqueta.add_theme_color_override("font_color", AVISO)
	add_child(etiqueta)
