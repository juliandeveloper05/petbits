## Mundo.gd
##
## El mapa navegable. Caminás con tu criatura por el pueblo y las zonas se
## visitan yendo hasta ellas.
##
## ---
##
## LA DECISIÓN DE DISEÑO QUE ORDENA TODO ESTO.
##
## Tres de las zonas —el patio, el bosque, las ruinas— ya existen como mecánica:
## son los destinos de expedición, y tardan quince minutos, hora y media o cuatro
## horas de tiempo REAL. Ocurren mientras el juego está cerrado.
##
## Si caminar hasta el bosque tardara tres segundos, esa mecánica moriría: el
## jugador iría, agarraría el botín y volvería, y la espera —que es medio
## corazón del juego— dejaría de existir.
##
## Así que el mapa reemplaza al MENÚ, no a la espera. Llegás caminando hasta la
## entrada del bosque y ahí la mandás; la expedición sigue tardando lo que tarda.
## El mundo vuelve tangible una elección que hoy es una lista de botones.
##
## ---
##
## ES LA MISMA CRIATURA QUE EN PetView, NO UNA COPIA.
##
## Todo sale del autoload `Partida`: el core, el guardado, el reloj. Mandarla
## desde acá es exactamente la misma llamada que apretar el botón en la otra
## pantalla, con el mismo estado y el mismo archivo. Si esta escena creara su
## propio `PetBitsCore` —como hizo en su primera versión— el pueblo mostraría una
## criatura al azar y las expediciones no tendrían efecto sobre tu partida.
##
## ---
##
## El tilemap se arma por código, con el atlas que genera el C++. No hay ningún
## PNG dibujado a mano en el proyecto, y esto no iba a ser la excepción.

extends Node2D

## La forma del pueblo vive aparte, en un script sin dependencias. Esta escena la
## muestra; no la define. Ver el encabezado de PuebloMapa.gd para por qué.
const Pueblo = preload("res://scripts/PuebloMapa.gd")

const TILE := Pueblo.TILE
const ANCHO := Pueblo.ANCHO
const ALTO := Pueblo.ALTO
const ZONAS := Pueblo.ZONAS

const VELOCIDAD := 46.0

## La paleta del mapa. Es la misma consola verde fósforo que PetView, pero acá el
## texto va sobre pasto y árboles en vez de sobre negro: los carteles llevan
## contorno oscuro y no fondo.
const TEXTO := Color("#d6e6d0")
const SOMBRA := Color("#0a0e0a")
const FOSFORO := Color("#9bbc0f")
const AVISO := Color("#ffc23d")

var _capa: TileMapLayer = null
var _criatura: Sprite2D = null
var _cartel: Label = null
var _estado: Label = null

## Sobre qué zona está parada, o {} si está en el medio del campo.
var _zona_cerca: Dictionary = {}

var _mapa: Array = []


func _ready() -> void:
	if not Partida.iniciar():
		_sin_extension()
		return

	_mapa = Pueblo.generar()
	_construir_tilemap()
	_construir_criatura()
	_construir_carteles()

	Partida.cambio.connect(_refrescar_estado)
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Construcción de la escena
# ---------------------------------------------------------------------------

## Arma el TileSet desde el atlas que genera el C++.
##
## Se construye en tiempo de ejecución en vez de guardarse como .tres: el atlas
## sale de código, así que un recurso guardado sería una copia que se desincroniza
## en cuanto alguien toque una receta de color.
func _construir_tilemap() -> void:
	var imagen: Image = Partida.core.atlas_tiles()
	var textura := ImageTexture.create_from_image(imagen)

	var fuente := TileSetAtlasSource.new()
	fuente.texture = textura
	fuente.texture_region_size = Vector2i(TILE, TILE)
	for i in Partida.core.cantidad_tiles():
		fuente.create_tile(Vector2i(i, 0))

	var conjunto := TileSet.new()
	conjunto.tile_size = Vector2i(TILE, TILE)
	conjunto.add_source(fuente, 0)

	_capa = TileMapLayer.new()
	_capa.tile_set = conjunto
	# Nearest: sin esto el pixel art se ve borroso, igual que con el sprite.
	_capa.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	add_child(_capa)

	for y in ALTO:
		for x in ANCHO:
			_capa.set_cell(Vector2i(x, y), 0, Vector2i(_mapa[y][x], 0))


func _construir_criatura() -> void:
	_criatura = Sprite2D.new()
	_criatura.texture = ImageTexture.create_from_image(Partida.core.sprite_actual(false))
	_criatura.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	# El sprite es de 32×32 y los tiles de 16: a escala 1 la criatura ocuparía
	# cuatro tiles y taparía el mapa. A la mitad ocupa uno, que es la proporción
	# de un personaje de consola portátil.
	_criatura.scale = Vector2(0.5, 0.5)
	_criatura.position = Pueblo.ENTRADA
	add_child(_criatura)


func _construir_carteles() -> void:
	_cartel = _etiqueta(Vector2(6, ALTO * TILE - 18), TEXTO)
	_estado = _etiqueta(Vector2(6, 4), FOSFORO)


## Texto legible sobre cualquier tile.
##
## El contorno grueso hace el trabajo de una caja de diálogo sin ocupar el lugar
## de una: sobre el pasto claro y sobre la sombra de los árboles se lee igual.
func _etiqueta(donde: Vector2, color: Color) -> Label:
	var etiqueta := Label.new()
	etiqueta.position = donde
	etiqueta.add_theme_font_size_override("font_size", Partida.tam_fuente)
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_color_override("font_outline_color", SOMBRA)
	etiqueta.add_theme_constant_override("outline_size", 2)
	add_child(etiqueta)
	return etiqueta


# ---------------------------------------------------------------------------
# Caminar
# ---------------------------------------------------------------------------

func _process(delta: float) -> void:
	if _criatura == null:
		return

	var dir := Vector2(
		Input.get_axis("move_left", "move_right"),
		Input.get_axis("move_up", "move_down")
	)
	if dir == Vector2.ZERO:
		_mirar_alrededor()
		return

	# Normalizado: sin esto, ir en diagonal sería un 41% más rápido que ir
	# derecho, que es el bug de movimiento más viejo del mundo.
	var paso := dir.normalized() * VELOCIDAD * delta

	# Los dos ejes se prueban por separado. Moviéndolos juntos, rozar una
	# esquina frena el movimiento entero y el personaje se traba en las paredes
	# en vez de deslizarse.
	if not _choca(_criatura.position + Vector2(paso.x, 0)):
		_criatura.position.x += paso.x
	if not _choca(_criatura.position + Vector2(0, paso.y)):
		_criatura.position.y += paso.y

	_mirar_alrededor()


## ¿Ese punto cae sobre un tile sólido?
##
## Se prueban las cuatro esquinas de una caja más chica que el sprite, no el
## centro: con el centro solo, media criatura se mete adentro del árbol antes de
## que algo la frene.
func _choca(pos: Vector2) -> bool:
	const MEDIO := 5.0
	for dx in [-MEDIO, MEDIO]:
		for dy in [-MEDIO, MEDIO]:
			var celda := Vector2i(int((pos.x + dx) / TILE), int((pos.y + dy) / TILE))
			if celda.x < 0 or celda.y < 0 or celda.x >= ANCHO or celda.y >= ALTO:
				return true
			if Partida.core.tile_solido(_mapa[celda.y][celda.x]):
				return true
	return false


## Si está parada sobre una zona, la nombra y la deja lista para mandar.
func _mirar_alrededor() -> void:
	var celda := Vector2i(int(_criatura.position.x / TILE), int(_criatura.position.y / TILE))

	for zona in ZONAS:
		# Se acepta el tile de la zona y sus vecinos: pararse EXACTO sobre una
		# celda de 16 píxeles con movimiento continuo es pedirle demasiado a
		# quien juega.
		if absi(celda.x - zona["x"]) <= 1 and absi(celda.y - zona["y"]) <= 1:
			if _zona_cerca != zona:
				_zona_cerca = zona
				_anunciar_zona()
			return

	if not _zona_cerca.is_empty():
		_zona_cerca = {}
		_cartel.text = "Flechas para caminar · Esc para volver"


## Qué dice el cartel al pararse en una entrada.
##
## El motivo del rechazo lo da el C++ —"todavía está muy chica para ir tan
## lejos"— y se muestra tal cual. Es la misma regla que el tooltip de PetView,
## leída del mismo lado: acá no se decide nada, se pinta.
func _anunciar_zona() -> void:
	var nombre: String = _zona_cerca["nombre"]
	var destino: String = _zona_cerca["destino"]

	if destino == "":
		_cartel.text = "%s — todavía no está listo" % nombre
		return

	for d in Partida.core.destinos():
		if d["id"] != destino:
			continue
		if d["puede"]:
			_cartel.text = "%s — Enter para mandarla · %s" % [nombre, d["descripcion"]]
		else:
			_cartel.text = "%s — %s" % [nombre, d["motivo"]]
		return

	_cartel.text = nombre


# ---------------------------------------------------------------------------
# Mandarla, y volver
# ---------------------------------------------------------------------------

func _unhandled_input(evento: InputEvent) -> void:
	if evento.is_action_pressed("action_cancel"):
		get_tree().change_scene_to_file("res://scenes/PetView.tscn")
		return

	if evento.is_action_pressed("action_confirm"):
		_enviar()


func _enviar() -> void:
	if _zona_cerca.is_empty():
		return

	var destino: String = _zona_cerca["destino"]
	if destino == "":
		return

	# Exactamente la misma llamada que el botón de PetView, sobre el mismo core.
	var r: Dictionary = Partida.actuar(func(): return Partida.core.enviar(destino, Partida.ahora_ms()))
	_cartel.text = r["mensaje"]

	# También va a la bitácora: si el jugador vuelve a PetView quiere ver ahí que
	# la mandó, no tener que acordarse de un cartel que ya no está.
	if r["ok"]:
		Partida.anotar(r["mensaje"], "bien")


# ---------------------------------------------------------------------------
# Refresco
# ---------------------------------------------------------------------------

## La línea de arriba: quién es y qué está haciendo.
##
## Cuando está de expedición el sprite se apaga a media luz en vez de esconderse.
## Esconderlo dejaría el mapa sin nada que mover y la pantalla muerta hasta que
## vuelva; a media luz se entiende que no está del todo acá, y se puede seguir
## caminando para ver dónde queda cada cosa.
func _refrescar_estado() -> void:
	if _criatura == null:
		return

	var e: Dictionary = Partida.core.estado()
	if e.is_empty():
		return

	var falta: int = Partida.core.falta_para_volver(Partida.ahora_ms())
	if falta > 0:
		# Minutos redondeados hacia arriba: decir "vuelve en 0 min" cuando todavía
		# faltan cuarenta segundos es mentir.
		_estado.text = "%s · vuelve en %d min" % [e["seed"], ceili(falta / 60000.0)]
		_estado.add_theme_color_override("font_color", AVISO)
		_criatura.modulate = Color(1, 1, 1, 0.35)
	else:
		_estado.text = e["seed"]
		_estado.add_theme_color_override("font_color", FOSFORO)
		_criatura.modulate = Color(1, 1, 1, 1)

	# La etapa o la forma pueden haber cambiado con la evolución mientras
	# caminabas, y eso cambia el dibujo.
	var imagen: Image = Partida.core.sprite_actual(false)
	if imagen != null:
		_criatura.texture = ImageTexture.create_from_image(imagen)

	if _zona_cerca.is_empty():
		_cartel.text = "Flechas para caminar · Esc para volver"


func _sin_extension() -> void:
	var etiqueta := Label.new()
	etiqueta.text = "La GDExtension no cargó. Compilá con `scons` en gdext/."
	etiqueta.position = Vector2(20, 120)
	etiqueta.add_theme_color_override("font_color", AVISO)
	add_child(etiqueta)
