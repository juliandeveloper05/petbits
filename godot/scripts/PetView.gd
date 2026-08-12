## PetView.gd
##
## La criatura en pantalla: sprite, estadísticas y el paso del tiempo.
##
## Toda la lógica vive en PetBitsCore (C++). Este script no calcula nada — lee
## el estado y lo dibuja. Es a propósito: si el ánimo se decidiera acá, la web y
## el nativo tendrían dos reglas distintas y la promesa del proyecto se caería.
##
## ---
##
## La interfaz se arma por código en _ready() en vez de en el .tscn. Un .tscn
## escrito a mano es fácil de romper de maneras que Godot reporta mal, y todavía
## no hay diseño visual definitivo que justifique fijarlo en un archivo de
## escena. Cuando la pantalla se estabilice, conviene pasarla al editor.

extends Control

# La consola verde fósforo que el proyecto ya tenía del lado web. Se mantiene
# porque es lo mejor que tiene su identidad visual.
const FONDO := Color("#0a0e0a")
const PANEL := Color("#0c140d")
const BORDE := Color("#3d5c46")
const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")

## Un tick del juego es un minuto real. Preguntar más seguido no cambia nada
## —simulate() solo avanza en ticks enteros— pero mantiene el reloj de pantalla
## al día sin costo.
const INTERVALO_CONSULTA := 1.0

## Cada cuánto parpadea. Es la animación más barata que existe y la que más hace
## por que algo lea como vivo.
const INTERVALO_PARPADEO := 4.2
const DURACION_PARPADEO := 0.16

var _core: RefCounted = null
var _sprite: TextureRect = null
var _barras := {}
var _etiquetas := {}
var _registro: RichTextLabel = null

var _parpadeando := false
var _proximo_parpadeo := INTERVALO_PARPADEO


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		_sin_extension()
		return

	_core = ClassDB.instantiate("PetBitsCore")
	_construir_interfaz()

	# Nace con un seed al azar. Cuando haya guardado, acá se carga la partida.
	var seed: String = _core.seed_al_azar()
	_core.nacer(seed, _ahora_ms(), _tz_min())

	_refrescar_sprite()
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Tiempo
# ---------------------------------------------------------------------------

## El reloj del sistema en milisegundos.
##
## Godot lo da en segundos y como float. Se multiplica y se redondea acá para
## que al C++ le llegue un entero: la simulación cuenta ticks de un minuto y un
## flotante con parte decimal en los milisegundos no aporta nada y sí puede
## correr una frontera.
func _ahora_ms() -> int:
	return int(Time.get_unix_time_from_system()) * 1000


## Minutos de desfasaje horario respecto de UTC.
##
## Se lee UNA vez, al nacer, y después vive dentro del estado de la criatura. Si
## se leyera en cada tick, mudarse de zona horaria —o simplemente viajar— movería
## la hora local de ticks ya procesados y rompería el invariante de que simular
## por pedazos da lo mismo que de corrido.
func _tz_min() -> int:
	return int(Time.get_time_zone_from_system().get("bias", 0))


func _process(delta: float) -> void:
	if _core == null:
		return

	_proximo_parpadeo -= delta
	if _parpadeando and _proximo_parpadeo <= 0.0:
		_parpadeando = false
		_proximo_parpadeo = INTERVALO_PARPADEO
		_refrescar_sprite()
	elif not _parpadeando and _proximo_parpadeo <= 0.0:
		_parpadeando = true
		_proximo_parpadeo = DURACION_PARPADEO
		_refrescar_sprite()


func _on_consultar() -> void:
	var r: Dictionary = _core.simular(_ahora_ms())
	if r.get("ticks", 0) > 0:
		_anotar_eventos(r)
		# La etapa o la forma pueden haber cambiado con la evolución, y eso
		# cambia el dibujo.
		_refrescar_sprite()
	_refrescar_estado()


# ---------------------------------------------------------------------------
# Interfaz
# ---------------------------------------------------------------------------

func _construir_interfaz() -> void:
	var fondo := ColorRect.new()
	fondo.color = FONDO
	fondo.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(fondo)

	var raiz := VBoxContainer.new()
	raiz.set_anchors_preset(Control.PRESET_FULL_RECT)
	raiz.offset_left = 12
	raiz.offset_top = 10
	raiz.offset_right = -12
	raiz.offset_bottom = -10
	raiz.add_theme_constant_override("separation", 6)
	add_child(raiz)

	# --- criatura ---
	_sprite = TextureRect.new()
	_sprite.custom_minimum_size = Vector2(128, 128)
	_sprite.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	# Nearest-neighbor: sin esto el pixel art de 32×32 se ve borroso al ampliar,
	# que es exactamente lo contrario de lo que se busca.
	_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	raiz.add_child(_sprite)

	_etiquetas["seed"] = _linea(raiz, FOSFORO, 14)
	_etiquetas["quien"] = _linea(raiz, TEXTO, 12)
	_etiquetas["etapa"] = _linea(raiz, TENUE, 11)

	raiz.add_child(HSeparator.new())

	# --- estadísticas ---
	for clave in ["energia", "animo", "salud", "vinculo"]:
		_barras[clave] = _barra(raiz, clave)

	raiz.add_child(HSeparator.new())

	# --- registro ---
	_registro = RichTextLabel.new()
	_registro.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_registro.custom_minimum_size = Vector2(0, 60)
	_registro.add_theme_color_override("default_color", TENUE)
	_registro.append_text("Nació recién.\n")
	raiz.add_child(_registro)

	# El reloj de la simulación. Se consulta seguido y barato.
	var reloj := Timer.new()
	reloj.wait_time = INTERVALO_CONSULTA
	reloj.timeout.connect(_on_consultar)
	reloj.autostart = true
	add_child(reloj)


func _linea(padre: Node, color: Color, tamano: int) -> Label:
	var etiqueta := Label.new()
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_font_size_override("font_size", tamano)
	padre.add_child(etiqueta)
	return etiqueta


## Una fila de estadística: nombre, barra y número.
##
## La grilla es de tres columnas con anchos fijos en los extremos. Del lado web
## esto mismo tuvo un bug que vale recordar: el número quedaba en una columna de
## ancho cero y solo se veía porque desbordaba contra el marco. Acá el ancho
## mínimo de la etiqueta del valor lo evita.
func _barra(padre: Node, clave: String) -> Dictionary:
	var fila := HBoxContainer.new()
	fila.add_theme_constant_override("separation", 8)
	padre.add_child(fila)

	var nombre := Label.new()
	nombre.text = clave.capitalize()
	nombre.custom_minimum_size = Vector2(62, 0)
	nombre.add_theme_color_override("font_color", TENUE)
	nombre.add_theme_font_size_override("font_size", 11)
	fila.add_child(nombre)

	var barra := ProgressBar.new()
	barra.max_value = 100
	barra.show_percentage = false
	barra.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	barra.custom_minimum_size = Vector2(0, 12)
	fila.add_child(barra)

	var valor := Label.new()
	valor.custom_minimum_size = Vector2(34, 0)
	valor.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	valor.add_theme_color_override("font_color", TEXTO)
	valor.add_theme_font_size_override("font_size", 11)
	fila.add_child(valor)

	return {"barra": barra, "valor": valor}


# ---------------------------------------------------------------------------
# Refresco
# ---------------------------------------------------------------------------

func _refrescar_sprite() -> void:
	var imagen: Image = _core.sprite_actual(_parpadeando)
	if imagen == null:
		return
	_sprite.texture = ImageTexture.create_from_image(imagen)


func _refrescar_estado() -> void:
	var e: Dictionary = _core.estado()
	if e.is_empty():
		return

	var genes: Dictionary = _core.decodificar(e["seed"])

	_etiquetas["seed"].text = e["seed"]
	_etiquetas["quien"].text = "%s · %s" % [genes["linaje"], genes["temperamento"]]

	var forma: String = e["forma"]
	var descripcion: String = e["etapa"].capitalize()
	if forma != "Sin definir":
		descripcion += " · " + forma
	if e["letargico"]:
		descripcion += " · en letargo"
	elif e["durmiendo"]:
		descripcion += " · durmiendo"
	_etiquetas["etapa"].text = descripcion

	var stats: Dictionary = e["stats"]
	for clave in _barras:
		var valor: float = stats[clave]
		_barras[clave]["barra"].value = valor
		_barras[clave]["valor"].text = "%d" % int(valor)
		_barras[clave]["barra"].modulate = _color_de(clave, valor)


## Semáforo. Con la barra sola, un ánimo en 8 y uno en 80 se distinguen mal de
## reojo; el color se lee sin mirar el número.
func _color_de(clave: String, valor: float) -> Color:
	if clave == "vinculo":
		return FOSFORO
	if valor < 25.0:
		return Color("#ff6b6b")
	if valor < 50.0:
		return Color("#ffc23d")
	return FOSFORO


func _anotar_eventos(r: Dictionary) -> void:
	for ev in r["eventos"]:
		var color := TENUE
		if ev["tipo"] == "evolucion":
			color = FOSFORO
		elif ev["tipo"] in ["salud", "letargo"]:
			color = Color("#ff6b6b")
		_registro.push_color(color)
		_registro.append_text(ev["texto"] + "\n")
		_registro.pop()

	if r.get("omitidos", 0) > 0:
		_registro.append_text("(y %d cosas más)\n" % r["omitidos"])


func _sin_extension() -> void:
	var etiqueta := Label.new()
	etiqueta.text = (
		"La GDExtension no cargó, así que no hay criatura que mostrar.\n"
		+ "Compilá el C++ con `scons` en gdext/ y volvé a abrir Godot.\n"
		+ "La escena scenes/Arranque.tscn da el diagnóstico completo."
	)
	etiqueta.set_anchors_preset(Control.PRESET_FULL_RECT)
	etiqueta.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	etiqueta.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	etiqueta.add_theme_color_override("font_color", Color("#ffc23d"))
	add_child(etiqueta)
