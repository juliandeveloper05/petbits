## PetView.gd
##
## La criatura en pantalla: sprite, estadísticas, botones y el paso del tiempo.
##
## Toda la lógica vive en PetBitsCore (C++). Este script no calcula nada — lee el
## estado y lo dibuja, y cuando apretás un botón le pasa la acción al núcleo y
## muestra lo que contesta. Es a propósito: si el ánimo se decidiera acá, la web
## y el nativo tendrían dos reglas distintas y la promesa del proyecto se caería.
##
## Y la partida tampoco vive acá: el core, el guardado y el reloj están en el
## autoload `Partida`, que sobrevive al cambio de pantalla. Esta escena es una de
## las dos ventanas a la misma criatura; la otra es el mapa.
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

## Cada cuánto parpadea, y cuánto dura. Es la animación más barata que existe y
## la que más hace por que algo lea como vivo.
const INTERVALO_PARPADEO := 4.2
const DURACION_PARPADEO := 0.16

const LADO_SPRITE := 96

var _sprite: TextureRect = null
var _barras := {}
var _etiquetas := {}
var _registro: RichTextLabel = null
var _botones_comida := {}
var _botones_salida := {}

var _parpadeando := false
var _proximo_parpadeo := INTERVALO_PARPADEO


func _ready() -> void:
	if not Partida.iniciar():
		_sin_extension()
		return

	_construir_interfaz()

	# La bitácora se rearma desde lo que ya estaba anotado. Ir al pueblo y volver
	# no tiene por qué borrar el "mientras no estabas": el registro es del juego,
	# no de esta pantalla.
	for linea in Partida.bitacora:
		_anotar(linea["texto"], _color_de_tono(linea["tono"]))

	Partida.nota.connect(func(t, tono): _anotar(t, _color_de_tono(tono)))
	Partida.avanzo.connect(func(_r): _refrescar_sprite())
	Partida.cambio.connect(_refrescar_estado)

	_refrescar_sprite()
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Tiempo
# ---------------------------------------------------------------------------

## El reloj lo lleva el autoload; esto es un atajo para no escribirlo diez veces.
func _ahora_ms() -> int:
	return Partida.ahora_ms()


func _process(delta: float) -> void:
	_proximo_parpadeo -= delta
	if _proximo_parpadeo > 0.0:
		return

	_parpadeando = not _parpadeando
	_proximo_parpadeo = DURACION_PARPADEO if _parpadeando else INTERVALO_PARPADEO
	_refrescar_sprite()


# ---------------------------------------------------------------------------
# Acciones
# ---------------------------------------------------------------------------

func _actuar(accion: Callable) -> void:
	_on_accion(Partida.actuar(accion))


func _on_accion(resultado: Dictionary) -> void:
	# Un rechazo NO es un error de la interfaz: es información. "No le da la
	# energía para jugar" le dice al jugador qué hacer, y por eso se muestra
	# igual que cualquier otro mensaje, solo que en ámbar.
	_anotar(resultado["mensaje"], AVISO if not resultado["ok"] else TEXTO)


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
	_registro.add_theme_font_size_override("normal_font_size", Partida.tam_fuente)
	raiz.add_child(_registro)


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

	_etiquetas["seed"] = _linea(ficha, FOSFORO)
	_etiquetas["quien"] = _linea(ficha, TEXTO)
	_etiquetas["etapa"] = _linea(ficha, TENUE)

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
	for alimento in Partida.core.alimentos():
		var id: String = alimento["id"]
		var boton := _boton(fila, alimento["nombre"], func(): _actuar(func(): return Partida.core.alimentar(id, _ahora_ms())))
		# Se guarda para poder actualizarle el contador cuando el stock cambia.
		_botones_comida[id] = { "boton": boton, "nombre": alimento["nombre"] }

	_boton(fila, "Jugar", func(): _actuar(func(): return Partida.core.jugar(_ahora_ms())))
	_boton(fila, "Mimos", func(): _actuar(func(): return Partida.core.acariciar(_ahora_ms())))

	# La expedición va en su propia fila: es la única acción que saca a la
	# criatura de casa, y mezclarla con las de cuidado la haría parecer una más.
	var fila_salidas := HBoxContainer.new()
	fila_salidas.add_theme_constant_override("separation", 3)
	padre.add_child(fila_salidas)

	for destino in Partida.core.destinos():
		var id: String = destino["id"]
		_botones_salida[id] = _boton(
			fila_salidas, destino["nombre"], func(): _actuar(func(): return Partida.core.enviar(id, _ahora_ms()))
		)

	# La puerta al mapa. Va con las salidas y no con las de cuidado porque hace
	# lo mismo que una expedición: sacar a la criatura de esta pantalla.
	_boton(fila_salidas, "Pueblo", func(): get_tree().change_scene_to_file("res://scenes/Mundo.tscn"))

	_refrescar_despensa()
	_refrescar_salidas()


func _boton(padre: Node, texto: String, al_apretar: Callable) -> Button:
	var boton := Button.new()
	boton.text = texto
	boton.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	boton.add_theme_font_size_override("font_size", Partida.tam_fuente)
	boton.pressed.connect(al_apretar)
	padre.add_child(boton)
	return boton


## Los destinos, con su estado.
##
## El motivo del rechazo viene del C++ —"todavía está muy chica para ir tan
## lejos"— y se muestra como tooltip. La regla vive de un solo lado; acá solo se
## pinta.
func _refrescar_salidas() -> void:
	for destino in Partida.core.destinos():
		var id: String = destino["id"]
		if not _botones_salida.has(id):
			continue
		var boton: Button = _botones_salida[id]
		var puede: bool = destino["puede"]
		boton.tooltip_text = destino["descripcion"] if puede else destino["motivo"]
		boton.modulate = Color(1, 1, 1) if puede else Color(0.55, 0.55, 0.55)


## Cuánto queda de cada comida, en el botón.
##
## El botón sigue habilitado con stock cero a propósito: apretarlo contesta "no
## te queda de eso, mandala a buscar", que le dice al jugador qué hacer. Un botón
## gris no explica nada, y encima esconde que el alimento existe.
func _refrescar_despensa() -> void:
	for alimento in Partida.core.alimentos():
		var id: String = alimento["id"]
		if not _botones_comida.has(id):
			continue
		var cantidad: int = alimento["cantidad"]
		var entrada: Dictionary = _botones_comida[id]
		var boton: Button = entrada["boton"]
		boton.text = "%s %d" % [entrada["nombre"], cantidad]
		boton.modulate = Color(1, 1, 1) if cantidad > 0 else Color(0.55, 0.55, 0.55)


## Una línea de texto de la ficha.
##
## `escala` es un MULTIPLICADOR entero, no un tamaño en píxeles. La fuente es una
## bitmap de 11 px: a 1 se dibuja tal cual y a 2 se duplica cada píxel. Cualquier
## otro número la interpola y le rompe los trazos, así que ni se ofrece.
func _linea(padre: Node, color: Color, escala: int = 1) -> Label:
	var etiqueta := Label.new()
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_font_size_override("font_size", Partida.tam_fuente * escala)
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
	nombre.add_theme_font_size_override("font_size", Partida.tam_fuente)
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
	valor.add_theme_font_size_override("font_size", Partida.tam_fuente)
	fila.add_child(valor)

	return {"barra": barra, "valor": valor}


# ---------------------------------------------------------------------------
# Refresco
# ---------------------------------------------------------------------------

func _refrescar_sprite() -> void:
	var imagen: Image = Partida.core.sprite_actual(_parpadeando)
	if imagen == null:
		return
	_sprite.texture = ImageTexture.create_from_image(imagen)


func _refrescar_estado() -> void:
	var e: Dictionary = Partida.core.estado()
	if e.is_empty():
		return

	var genes: Dictionary = Partida.core.decodificar(e["seed"])

	_etiquetas["seed"].text = e["seed"]
	_etiquetas["quien"].text = "%s · %s" % [genes["linaje"], genes["temperamento"]]

	var descripcion: String = e["etapa"].capitalize()
	if e["forma"] != "Sin definir":
		descripcion += " · " + e["forma"]
	var falta: int = Partida.core.falta_para_volver(_ahora_ms())
	if falta > 0:
		# Minutos redondeados hacia arriba: decir "vuelve en 0 min" cuando todavía
		# faltan cuarenta segundos es mentir.
		descripcion += " · vuelve en %d min" % ceili(falta / 60000.0)
	elif e["letargico"]:
		descripcion += " · en letargo"
	elif e["durmiendo"]:
		descripcion += " · durmiendo"
	_etiquetas["etapa"].text = descripcion

	_refrescar_despensa()
	_refrescar_salidas()

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


## Los tonos que emite `Partida`, en los colores de esta consola.
##
## El autoload no sabe de colores a propósito: emite "aviso" o "raro", y cada
## pantalla decide. El mapa pinta sus carteles con otra paleta.
func _color_de_tono(tono: String) -> Color:
	match tono:
		"bien": return FOSFORO
		"aviso": return AVISO
		"alerta": return ALERTA
		"tenue": return TENUE
		"raro": return Color("#c07cff")
		_: return TEXTO


func _anotar(texto: String, color: Color) -> void:
	_registro.push_color(color)
	_registro.append_text(texto + "\n")
	_registro.pop()


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
