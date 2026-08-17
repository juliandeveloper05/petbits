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
## El tilemap se arma por código, con el atlas que genera el C++. No hay ningún
## PNG dibujado a mano en el proyecto, y esto no iba a ser la excepción.

extends Node2D

const TILE := 16

## Los tipos de tile, en el mismo orden que el enum del C++.
##
## El orden es un contrato: los mapas de abajo guardan índices, así que
## reordenar acá los rompe todos. Si se agrega un tile, va al final.
enum T { PASTO, CAMINO, AGUA, PIEDRA, ARBOL, PASTO_ALTO, ARENA, MUSGO }

const ANCHO := 30
const ALTO := 17

const VELOCIDAD := 46.0

var _core: RefCounted = null
var _capa: TileMapLayer = null
var _criatura: Sprite2D = null
var _cartel: Label = null

## Dónde está cada zona y qué destino de expedición le corresponde.
##
## Las que no tienen destino todavía no hacen nada: están puestas para que el
## pueblo tenga forma de pueblo desde el principio, y para no tener que rehacer
## el mapa cuando existan.
const ZONAS := [
	{"x": 4, "y": 3, "nombre": "El patio", "destino": "patio"},
	{"x": 25, "y": 3, "nombre": "El bosque", "destino": "bosque"},
	{"x": 25, "y": 13, "nombre": "Las ruinas", "destino": "ruinas"},
	{"x": 4, "y": 13, "nombre": "El criadero", "destino": ""},
	{"x": 15, "y": 2, "nombre": "El codex", "destino": ""},
]

var _mapa: Array = []


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		_sin_extension()
		return

	_core = ClassDB.instantiate("PetBitsCore")
	_core.nacer(_core.seed_al_azar(), _ahora_ms(), 0)

	_construir_mapa()
	_construir_tilemap()
	_construir_criatura()
	_construir_cartel()
	set_process(true)


# ---------------------------------------------------------------------------
# El mapa
# ---------------------------------------------------------------------------

## Genera el pueblo.
##
## A mano y no procedural: es el lugar donde el jugador arranca y tiene que
## leerse como un sitio pensado, no como ruido. Los caminos van de la plaza
## central a cada zona, que es lo que enseña sin decir nada que esas son las
## salidas.
func _construir_mapa() -> void:
	_mapa = []
	for y in ALTO:
		var fila := []
		for x in ANCHO:
			fila.append(T.PASTO)
		_mapa.append(fila)

	# Un borde de árboles: encierra el pueblo sin necesidad de paredes
	# invisibles, que son la manera fea de resolver lo mismo.
	for x in ANCHO:
		_mapa[0][x] = T.ARBOL
		_mapa[ALTO - 1][x] = T.ARBOL
	for y in ALTO:
		_mapa[y][0] = T.ARBOL
		_mapa[y][ANCHO - 1] = T.ARBOL

	# La plaza, en el centro.
	for y in range(7, 11):
		for x in range(12, 19):
			_mapa[y][x] = T.CAMINO

	# Un caminito a cada zona.
	for zona in ZONAS:
		_trazar_camino(15, 8, zona["x"], zona["y"])

	# Un estanque, para que no sea todo verde y camino.
	for y in range(11, 15):
		for x in range(19, 24):
			_mapa[y][x] = T.AGUA
	for x in range(19, 24):
		_mapa[10][x] = T.ARENA

	# Pasto alto en dos manchones. Es decorativo hoy; cuando haya criaturas
	# salvajes, es donde aparecerán.
	for p in [[7, 4], [8, 4], [7, 5], [21, 4], [22, 4], [21, 5], [22, 5]]:
		_mapa[p[1]][p[0]] = T.PASTO_ALTO

	# Y las zonas, marcadas en piedra.
	for zona in ZONAS:
		_mapa[zona["y"]][zona["x"]] = T.PIEDRA


## Un camino en L entre dos puntos. Primero horizontal, después vertical.
func _trazar_camino(x0: int, y0: int, x1: int, y1: int) -> void:
	for x in range(min(x0, x1), max(x0, x1) + 1):
		if _dentro(x, y0):
			_mapa[y0][x] = T.CAMINO
	for y in range(min(y0, y1), max(y0, y1) + 1):
		if _dentro(x1, y):
			_mapa[y][x1] = T.CAMINO


func _dentro(x: int, y: int) -> bool:
	return x > 0 and y > 0 and x < ANCHO - 1 and y < ALTO - 1


# ---------------------------------------------------------------------------
# Construcción de la escena
# ---------------------------------------------------------------------------

## Arma el TileSet desde el atlas que genera el C++.
##
## Se construye en tiempo de ejecución en vez de guardarse como .tres: el atlas
## sale de código, así que un recurso guardado sería una copia que se desincroniza
## en cuanto alguien toque una receta de color.
func _construir_tilemap() -> void:
	var imagen: Image = _core.atlas_tiles()
	var textura := ImageTexture.create_from_image(imagen)

	var fuente := TileSetAtlasSource.new()
	fuente.texture = textura
	fuente.texture_region_size = Vector2i(TILE, TILE)
	for i in _core.cantidad_tiles():
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
	_criatura.texture = ImageTexture.create_from_image(_core.sprite_actual(false))
	_criatura.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	# El sprite es de 32×32 y los tiles de 16: a escala 1 la criatura ocuparía
	# cuatro tiles y taparía el mapa. A la mitad ocupa uno, que es la proporción
	# de un personaje de consola portátil.
	_criatura.scale = Vector2(0.5, 0.5)
	_criatura.position = Vector2(15.5 * TILE, 8.5 * TILE)
	add_child(_criatura)


func _construir_cartel() -> void:
	_cartel = Label.new()
	_cartel.position = Vector2(6, 4)
	_cartel.add_theme_font_size_override("font_size", 10)
	_cartel.add_theme_color_override("font_color", Color("#d6e6d0"))
	_cartel.add_theme_color_override("font_outline_color", Color("#0a0e0a"))
	_cartel.add_theme_constant_override("outline_size", 4)
	_cartel.text = "Flechas para caminar"
	add_child(_cartel)


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
			if _core.tile_solido(_mapa[celda.y][celda.x]):
				return true
	return false


## Si está parada sobre una zona, la nombra.
func _mirar_alrededor() -> void:
	var celda := Vector2i(int(_criatura.position.x / TILE), int(_criatura.position.y / TILE))

	for zona in ZONAS:
		# Se acepta el tile de la zona y sus vecinos: pararse EXACTO sobre una
		# celda de 16 píxeles con movimiento continuo es pedirle demasiado a
		# quien juega.
		if absi(celda.x - zona["x"]) <= 1 and absi(celda.y - zona["y"]) <= 1:
			if zona["destino"] == "":
				_cartel.text = "%s — todavía no está listo" % zona["nombre"]
			else:
				_cartel.text = "%s — Enter para mandarla" % zona["nombre"]
			return

	_cartel.text = "Flechas para caminar"


func _ahora_ms() -> int:
	return int(Time.get_unix_time_from_system()) * 1000


func _sin_extension() -> void:
	var etiqueta := Label.new()
	etiqueta.text = "La GDExtension no cargó. Compilá con `scons` en gdext/."
	etiqueta.position = Vector2(20, 120)
	etiqueta.add_theme_color_override("font_color", Color("#ffc23d"))
	add_child(etiqueta)
