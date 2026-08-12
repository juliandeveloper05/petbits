## PetView.gd
##
## La criatura en pantalla: sprite, estadísticas, botones y el paso del tiempo.
##
## Toda la lógica vive en PetBitsCore (C++). Este script no calcula nada — lee el
## estado y lo dibuja, y cuando apretás un botón le pasa la acción al núcleo y
## muestra lo que contesta. Es a propósito: si el ánimo se decidiera acá, la web
## y el nativo tendrían dos reglas distintas y la promesa del proyecto se caería.
##
## ---
##
## EL PRESUPUESTO DE PANTALLA MANDA.
##
## El proyecto corre a 480×270 —la resolución de una consola portátil, que es la
## identidad visual del juego— y eso son 254 píxeles útiles de alto. La primera
## versión de esta pantalla apilaba todo en una columna y pedía unos 400: Salud y
## Vínculo quedaban abajo del borde y el registro no se veía nunca.
##
## Ahora va en dos columnas, que es lo que pide un 16:9. El sprite a la
## izquierda, la ficha a la derecha, y debajo los botones y el registro. Cada
## bloque tiene su altura contada; si se agrega algo, hay que sacar otra cosa.

extends Control

# La consola verde fósforo que el proyecto ya tenía del lado web. Se mantiene
# porque es lo mejor que tiene su identidad visual.
const FONDO := Color("#0a0e0a")
const BORDE := Color("#3d5c46")
const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")
const ALERTA := Color("#ff6b6b")
const AVISO := Color("#ffc23d")

## Un tick del juego es un minuto real. Preguntar más seguido no cambia nada
## —simulate() solo avanza en ticks enteros— pero mantiene la pantalla al día.
const INTERVALO_CONSULTA := 1.0

## Cada cuánto parpadea, y cuánto dura. Es la animación más barata que existe y
## la que más hace por que algo lea como vivo.
const INTERVALO_PARPADEO := 4.2
const DURACION_PARPADEO := 0.16

const LADO_SPRITE := 96

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
	_core.nacer(_core.seed_al_azar(), _ahora_ms(), _tz_min())

	_refrescar_sprite()
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Tiempo
# ---------------------------------------------------------------------------

## El reloj del sistema en milisegundos.
##
## Godot lo da en segundos y como float. Se redondea acá para que al C++ le
## llegue un entero: la simulación cuenta ticks de un minuto y una parte decimal
## en los milisegundos no aporta nada y sí puede correr una frontera.
func _ahora_ms() -> int:
	return int(Time.get_unix_time_from_system()) * 1000


## Minutos de desfasaje horario respecto de UTC.
##
## Se lee UNA vez, al nacer, y después vive dentro del estado de la criatura. Si
## se leyera en cada tick, mudarse de zona horaria movería la hora local de ticks
## ya procesados y rompería el invariante de que simular por pedazos da lo mismo
## que de corrido.
func _tz_min() -> int:
	return int(Time.get_time_zone_from_system().get("bias", 0))


func _process(delta: float) -> void:
	if _core == null:
		return

	_proximo_parpadeo -= delta
	if _proximo_parpadeo > 0.0:
		return

	_parpadeando = not _parpadeando
	_proximo_parpadeo = DURACION_PARPADEO if _parpadeando else INTERVALO_PARPADEO
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
# Acciones
# ---------------------------------------------------------------------------

func _on_accion(resultado: Dictionary) -> void:
	# Un rechazo NO es un error de la interfaz: es información. "No le da la
	# energía para jugar" le dice al jugador qué hacer, y por eso se muestra
	# igual que cualquier otro mensaje, solo que en ámbar.
	_anotar(resultado["mensaje"], AVISO if not resultado["ok"] else TEXTO)
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
	raiz.offset_left = 8
	raiz.offset_top = 6
	raiz.offset_right = -8
	raiz.offset_bottom = -6
	raiz.add_theme_constant_override("separation", 4)
	add_child(raiz)

	_construir_ficha(raiz)
	raiz.add_child(HSeparator.new())
	_construir_botones(raiz)
	raiz.add_child(HSeparator.new())

	_registro = RichTextLabel.new()
	_registro.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_registro.add_theme_color_override("default_color", TENUE)
	_registro.add_theme_font_size_override("normal_font_size", 9)
	raiz.add_child(_registro)
	_anotar("Nació recién.", TENUE)

	# El reloj de la simulación. Se consulta seguido y barato.
	var reloj := Timer.new()
	reloj.wait_time = INTERVALO_CONSULTA
	reloj.timeout.connect(_on_consultar)
	reloj.autostart = true
	add_child(reloj)


## Fila de arriba: el sprite a la izquierda, la ficha y las barras a la derecha.
func _construir_ficha(padre: Node) -> void:
	var fila := HBoxContainer.new()
	fila.add_theme_constant_override("separation", 10)
	padre.add_child(fila)

	_sprite = TextureRect.new()
	_sprite.custom_minimum_size = Vector2(LADO_SPRITE, LADO_SPRITE)
	_sprite.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	# Nearest-neighbor: sin esto el pixel art de 32×32 se ve borroso al ampliar,
	# que es exactamente lo contrario de lo que se busca.
	_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	fila.add_child(_sprite)

	var ficha := VBoxContainer.new()
	ficha.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	ficha.add_theme_constant_override("separation", 1)
	fila.add_child(ficha)

	_etiquetas["seed"] = _linea(ficha, FOSFORO, 12)
	_etiquetas["quien"] = _linea(ficha, TEXTO, 10)
	_etiquetas["etapa"] = _linea(ficha, TENUE, 9)

	var aire := Control.new()
	aire.custom_minimum_size = Vector2(0, 4)
	ficha.add_child(aire)

	for clave in ["energia", "animo", "salud", "vinculo"]:
		_barras[clave] = _barra(ficha, clave)


func _construir_botones(padre: Node) -> void:
	var fila := HBoxContainer.new()
	fila.add_theme_constant_override("separation", 3)
	padre.add_child(fila)

	# El catálogo lo da el C++: si mañana se agrega un alimento, el botón aparece
	# solo. Repetir la lista acá sería tener dos fuentes de verdad para lo mismo.
	for alimento in _core.alimentos():
		var id: String = alimento["id"]
		_boton(fila, alimento["nombre"], func(): _on_accion(_core.alimentar(id, _ahora_ms())))

	_boton(fila, "Jugar", func(): _on_accion(_core.jugar(_ahora_ms())))
	_boton(fila, "Mimos", func(): _on_accion(_core.acariciar(_ahora_ms())))


func _boton(padre: Node, texto: String, al_apretar: Callable) -> void:
	var boton := Button.new()
	boton.text = texto
	boton.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	boton.add_theme_font_size_override("font_size", 9)
	boton.pressed.connect(al_apretar)
	padre.add_child(boton)


func _linea(padre: Node, color: Color, tamano: int) -> Label:
	var etiqueta := Label.new()
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_font_size_override("font_size", tamano)
	padre.add_child(etiqueta)
	return etiqueta


## Una fila de estadística: nombre, barra y número.
##
## Los anchos de los extremos son mínimos fijos. Del lado web esto mismo tuvo un
## bug que vale recordar: el número quedaba en una columna de ancho cero y solo
## se veía porque desbordaba contra el marco.
func _barra(padre: Node, clave: String) -> Dictionary:
	var fila := HBoxContainer.new()
	fila.add_theme_constant_override("separation", 5)
	padre.add_child(fila)

	var nombre := Label.new()
	nombre.text = clave.capitalize()
	nombre.custom_minimum_size = Vector2(48, 0)
	nombre.add_theme_color_override("font_color", TENUE)
	nombre.add_theme_font_size_override("font_size", 9)
	fila.add_child(nombre)

	var barra := ProgressBar.new()
	barra.max_value = 100
	barra.show_percentage = false
	barra.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	barra.custom_minimum_size = Vector2(0, 9)
	fila.add_child(barra)

	var valor := Label.new()
	valor.custom_minimum_size = Vector2(24, 0)
	valor.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	valor.add_theme_color_override("font_color", TEXTO)
	valor.add_theme_font_size_override("font_size", 9)
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

	var descripcion: String = e["etapa"].capitalize()
	if e["forma"] != "Sin definir":
		descripcion += " · " + e["forma"]
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
		return ALERTA
	if valor < 50.0:
		return AVISO
	return FOSFORO


func _anotar(texto: String, color: Color) -> void:
	_registro.push_color(color)
	_registro.append_text(texto + "\n")
	_registro.pop()


func _anotar_eventos(r: Dictionary) -> void:
	for ev in r["eventos"]:
		var color := TENUE
		if ev["tipo"] == "evolucion":
			color = FOSFORO
		elif ev["tipo"] in ["salud", "letargo"]:
			color = ALERTA
		_anotar(ev["texto"], color)

	if r.get("omitidos", 0) > 0:
		_anotar("(y %d cosas más)" % r["omitidos"], TENUE)


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
	etiqueta.add_theme_color_override("font_color", AVISO)
	add_child(etiqueta)
