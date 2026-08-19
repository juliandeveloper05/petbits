## region_a_png.gd
##
## Compone un pedazo grande del mundo infinito en un PNG, sin abrir una ventana.
##
##   godot --headless --path godot res://scenes/RegionAPng.tscn
##   → godot/region.png
##
## ---
##
## POR QUÉ ESTO VA ANTES QUE LA CÁMARA Y LOS CHUNKS.
##
## Los tests del generador dicen que las costuras cierran, que es determinista,
## que ningún bioma se come el mundo y que desde el centro se puede caminar a
## todos lados. Las cuatro cosas las cumple un mundo horrible: manchas de colores
## sin forma, bosques que parecen sarpullido, costas que se leen como ruido.
##
## Nada de eso se puede contestar sin mirarlo, y mirarlo cuesta muchísimo menos
## acá que después de escribir la cámara, el sistema de chunks y la carga por
## anillos. Este PNG es la oportunidad de equivocarse barato en la parte más
## difícil.
##
## Se dibujan varias semillas, una al lado de la otra. Una sola podría salir
## linda de casualidad; tres seguidas ya dicen algo del generador y no de la
## suerte.
##
## ---
##
## DIBUJA LOS TILES ENTEROS, Y ANTES NO.
##
## La primera versión promediaba cada tile de 16×16 a un solo color. Tenía
## sentido mientras la pregunta era sobre formas grandes —si los lagos tienen
## costa, si los biomas se tocan— y dejó de tenerlo en cuanto la pregunta pasó a
## ser cómo se ven los materiales al tocarse: promediando, una transición y un
## escalón se ven exactamente igual.
##
## Con los tiles enteros entran menos tiles en la misma imagen, y está bien. Para
## mirar formas grandes está el mapa del pueblo; esto ahora mira la textura.

extends Node

const TILE := 16

## Cuántos tiles de lado tiene cada región dibujada.
##
## Con los tiles dibujados enteros, 48 tiles son 768 píxeles por semilla: alcanza
## para ver un pedazo de costa, un claro de bosque y el pueblo, que es la escala
## a la que se juzga cómo se tocan los materiales. Con 96 no entraba en una
## imagen que se pueda mirar.
const LADO := 48

## Sin escalar: un píxel del tile es un píxel de la imagen. Ampliar acá no
## agregaría información, solo tamaño.
const ESCALA := 1

## Las semillas que se dibujan. Fijas: dos corridas tienen que dar el mismo PNG,
## así que un diff visual significa que cambió el generador y no que salió otra.
const SEMILLAS := [
	"A3F0-91C4-77BE-2D08",
	"FEDC-BA98-7654-3210",
	"0000-0000-0000-0000",
]

const SEPARACION := 8


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	var atlas: Image = core.atlas_tiles()

	var ancho := SEMILLAS.size() * (LADO * TILE + SEPARACION) - SEPARACION
	var salida := Image.create_empty(ancho * ESCALA, LADO * TILE * ESCALA, false, Image.FORMAT_RGBA8)
	salida.fill(Color("#0a0e0a"))

	var x0 := 0
	for semilla in SEMILLAS:
		_dibujar_region(salida, core, atlas, semilla, x0)
		x0 += LADO * TILE + SEPARACION

	var error := salida.save_png("res://region.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		get_tree().quit(1)
		return

	print("Región escrita en %s" % ProjectSettings.globalize_path("res://region.png"))
	print("%d semillas de %dx%d tiles cada una" % [SEMILLAS.size(), LADO, LADO])
	print("chunk = %d tiles" % core.lado_de_chunk())

	# Qué hay alrededor del origen, contado y no mirado. El pueblo va ahí, así que
	# es el único pedazo del mundo que tiene que cumplir algo: nada de agua, nada
	# de roca, y suelo suficiente para poner un pueblo de 30 x 17.
	var NOMBRES := ["pasto", "camino", "agua", "piedra", "arbol", "pastoalto",
		"arena", "musgo", "piso", "pared", "alfombra", "pedestal"]
	for semilla in SEMILLAS:
		var cuenta := {}
		var solidos := 0
		for y in range(-10, 11):
			for x in range(-16, 17):
				var t: int = core.mundo_tile(semilla, x, y)
				cuenta[t] = cuenta.get(t, 0) + 1
				if core.tile_solido(t):
					solidos += 1
		var partes: Array = []
		for t in cuenta:
			partes.append("%s %d" % [NOMBRES[t], cuenta[t]])
		print("  %s · alrededor del origen: %s · sólidos %d de 693 · %s" % [
			semilla, ", ".join(partes), solidos, core.mundo_bioma(semilla, 0, 0)
		])

	get_tree().quit(0)


## Dibuja una región centrada en el origen del mundo.
##
## Centrada en (0, 0) y no en un punto cualquiera a propósito: ahí es donde va a
## estar el pueblo, así que es la parte del mundo que todos los jugadores ven
## primero. Si el origen cae en medio de un lago, eso hay que saberlo ahora.
## `x0_px` viene en pixeles; adentro se trabaja en tiles.
##
## Se dibuja capa por capa, de abajo hacia arriba, con el mismo corrimiento de
## medio tile que usa el juego. Es literalmente el mismo apilado: si aca se ve una
## costura, en pantalla tambien.
func _dibujar_region(
	destino: Image, core: RefCounted, atlas: Image, semilla: String, x0_px: int
) -> void:
	var x0 := x0_px / TILE
	var layout: Dictionary = core.atlas_layout()
	var desde := -LADO / 2

	# El medio tile de corrimiento se aplica al DIBUJAR, no al pedir: la mascara
	# de (x, y) ya mira las cuatro celdas correctas.
	var medio := TILE / 2

	for capa in int(layout["capas"]):
		for ty in range(LADO + 1):
			for tx in range(LADO + 1):
				var wx: int = desde + tx
				var wy: int = desde + ty
				var m: int = core.mundo_mascara(semilla, wx, wy, capa)
				if m == 0:
					continue
				_pegar(destino, atlas, m, capa, (x0 + tx) * TILE - medio, ty * TILE - medio)

	# Los objetos van encima y sin corrimiento: un arbol se para sobre su celda.
	var fila_obj: int = int(layout["fila_objetos"])
	for ty in LADO:
		for tx in LADO:
			var wx: int = desde + tx
			var wy: int = desde + ty
			var col: int = core.mundo_objeto_columna(semilla, wx, wy)
			if col < 0:
				continue
			_pegar(destino, atlas, col, fila_obj, (x0 + tx) * TILE, ty * TILE)

	# Una cruz de un tile en el origen, para saber donde cae el pueblo.
	_marcar(destino, x0 + LADO / 2, LADO / 2)


## Copia un tile del atlas, salteando la transparencia.
##
## Saltear el alfa cero no es una optimizacion: con las capas apiladas, una que
## pintara su transparencia como negro taparia a la de abajo. Es la diferencia
## entre ver una transicion y ver un agujero.
func _pegar(destino: Image, atlas: Image, col: int, fila: int, px: int, py: int) -> void:
	for ty in TILE:
		for tx in TILE:
			var c := atlas.get_pixel(col * TILE + tx, fila * TILE + ty)
			if c.a == 0.0:
				continue
			for dy in ESCALA:
				for dx in ESCALA:
					var x := (px + tx) * ESCALA + dx
					var y := (py + ty) * ESCALA + dy
					if x >= 0 and y >= 0 and x < destino.get_width() and y < destino.get_height():
						destino.set_pixel(x, y, c)


func _marcar(destino: Image, px: int, py: int) -> void:
	for ty in TILE:
		for tx in TILE:
			# Solo el marco del tile: relleno taparía lo que se quiere mirar.
			if ty > 0 and ty < TILE - 1 and tx > 0 and tx < TILE - 1:
				continue
			for dy in ESCALA:
				for dx in ESCALA:
					var x := (px * TILE + tx) * ESCALA + dx
					var y := (py * TILE + ty) * ESCALA + dy
					if x >= 0 and y >= 0 and x < destino.get_width() and y < destino.get_height():
						destino.set_pixel(x, y, Color("#ff6b6b"))
