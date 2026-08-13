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
var _botones_comida := {}

var _parpadeando := false
var _proximo_parpadeo := INTERVALO_PARPADEO


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		_sin_extension()
		return

	_core = ClassDB.instantiate("PetBitsCore")
	_construir_interfaz()
	_cargar_o_nacer()

	_refrescar_sprite()
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Guardado
# ---------------------------------------------------------------------------

## Dónde vive la partida.
##
## `user://` es la carpeta de datos del usuario que resuelve Godot en cada
## sistema operativo. Nunca `res://`: eso es el proyecto, y en un juego exportado
## viene dentro del paquete y es de solo lectura.
const RUTA_SAVE := "user://partida.json"

## Nombre del archivo cuando un save no se puede leer.
##
## El save roto NO se borra: se corre de lugar. Alguien puede haber perdido meses
## de partida por un corte de luz a mitad de una escritura, y un archivo que no
## carga hoy puede ser recuperable a mano mañana. Borrarlo es una decisión que le
## toca al dueño, no al programa.
const RUTA_CUARENTENA := "user://partida.rota.json"


func _cargar_o_nacer() -> void:
	if not FileAccess.file_exists(RUTA_SAVE):
		_nacer_nueva()
		return

	var archivo := FileAccess.open(RUTA_SAVE, FileAccess.READ)
	if archivo == null:
		_anotar("No se pudo abrir la partida guardada. Empezamos de nuevo.", AVISO)
		_nacer_nueva()
		return

	var texto := archivo.get_as_text()
	archivo.close()

	var r: Dictionary = _core.cargar(texto)
	if not r["ok"]:
		_anotar("La partida guardada no se pudo leer: %s" % r["mensaje"], AVISO)
		_anotar("Se guardó una copia en %s por las dudas." % RUTA_CUARENTENA, TENUE)
		DirAccess.rename_absolute(
			ProjectSettings.globalize_path(RUTA_SAVE),
			ProjectSettings.globalize_path(RUTA_CUARENTENA)
		)
		_nacer_nueva()
		return

	# El tiempo corrió mientras el juego estaba cerrado. Esta es la llamada que
	# hace que la criatura haya vivido en serio durante la ausencia.
	var sim: Dictionary = _core.simular(_ahora_ms())
	_anotar("Volviste.", FOSFORO)
	if sim.get("ticks", 0) > 0:
		_anotar_eventos(sim)


func _nacer_nueva() -> void:
	_core.nacer(_core.seed_al_azar(), _ahora_ms(), _tz_min())
	_anotar("Nació recién.", TENUE)
	_guardar()


func _guardar() -> void:
	var texto: String = _core.guardar(_ahora_ms())
	if texto == "":
		return
	var archivo := FileAccess.open(RUTA_SAVE, FileAccess.WRITE)
	if archivo == null:
		return
	archivo.store_string(texto)
	archivo.close()


## Guardar al cerrar la ventana.
##
## Sin esto se perdería lo hecho desde el último tick guardado. El juego escribe
## el archivo entero cada vez —son un par de kilobytes— así que no hay estado
## parcial posible: o está el archivo viejo o está el nuevo.
func _notification(que: int) -> void:
	if que == NOTIFICATION_WM_CLOSE_REQUEST or que == NOTIFICATION_PREDELETE:
		if _core != null:
			_guardar()


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
		# Se guarda solo cuando el tiempo avanzó de verdad. El reloj consulta
		# una vez por segundo y un tick dura un minuto: escribir en cada consulta
		# serían sesenta escrituras al pedo por cada una que sirve.
		_guardar()
	_refrescar_estado()


# ---------------------------------------------------------------------------
# Acciones
# ---------------------------------------------------------------------------

## Se pone al día ANTES de actuar, igual que catchUp() en la web.
##
## Si no, la acción se aplicaría sobre un estado viejo y el tiempo transcurrido
## se descontaría después, pisándola. Con el reloj consultando cada segundo casi
## nunca hay ticks pendientes, pero "casi nunca" no es una garantía.
func _actuar(accion: Callable) -> void:
	_core.simular(_ahora_ms())
	_on_accion(accion.call())


func _on_accion(resultado: Dictionary) -> void:
	# Un rechazo NO es un error de la interfaz: es información. "No le da la
	# energía para jugar" le dice al jugador qué hacer, y por eso se muestra
	# igual que cualquier otro mensaje, solo que en ámbar.
	_anotar(resultado["mensaje"], AVISO if not resultado["ok"] else TEXTO)
	_refrescar_estado()
	if resultado["ok"]:
		_guardar()


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
		var boton := _boton(fila, alimento["nombre"], func(): _actuar(func(): return _core.alimentar(id, _ahora_ms())))
		# Se guarda para poder actualizarle el contador cuando el stock cambia.
		_botones_comida[id] = { "boton": boton, "nombre": alimento["nombre"] }

	_boton(fila, "Jugar", func(): _actuar(func(): return _core.jugar(_ahora_ms())))
	_boton(fila, "Mimos", func(): _actuar(func(): return _core.acariciar(_ahora_ms())))
	_refrescar_despensa()


func _boton(padre: Node, texto: String, al_apretar: Callable) -> Button:
	var boton := Button.new()
	boton.text = texto
	boton.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	boton.add_theme_font_size_override("font_size", 9)
	boton.pressed.connect(al_apretar)
	padre.add_child(boton)
	return boton


## Cuánto queda de cada comida, en el botón.
##
## El botón sigue habilitado con stock cero a propósito: apretarlo contesta "no
## te queda de eso, mandala a buscar", que le dice al jugador qué hacer. Un botón
## gris no explica nada, y encima esconde que el alimento existe.
func _refrescar_despensa() -> void:
	for alimento in _core.alimentos():
		var id: String = alimento["id"]
		if not _botones_comida.has(id):
			continue
		var cantidad: int = alimento["cantidad"]
		var entrada: Dictionary = _botones_comida[id]
		var boton: Button = entrada["boton"]
		boton.text = "%s %d" % [entrada["nombre"], cantidad]
		boton.modulate = Color(1, 1, 1) if cantidad > 0 else Color(0.55, 0.55, 0.55)


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

	_refrescar_despensa()

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
