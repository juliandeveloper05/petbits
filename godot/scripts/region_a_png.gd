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

extends Node

const TILE := 16

## Cuántos tiles de lado tiene cada región dibujada.
##
## Bajó de 160 a 96 al meter el pueblo adentro del mundo. Con 160 el pueblo era
## una manchita de treinta tiles en el medio y no se veía si se integraba o
## parecía pegado encima, que es justamente la pregunta nueva. Sigue habiendo
## lagos enteros en cuadro.
const LADO := 96

## Un píxel de pantalla por tile del mundo sería ilegible. A tres se ven las
## formas grandes y todavía se distingue el moteado de cada tile.
const ESCALA := 5

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

	var ancho := SEMILLAS.size() * (LADO + SEPARACION) - SEPARACION
	var salida := Image.create_empty(ancho * ESCALA, LADO * ESCALA, false, Image.FORMAT_RGBA8)
	salida.fill(Color("#0a0e0a"))

	var x0 := 0
	for semilla in SEMILLAS:
		_dibujar_region(salida, core, atlas, semilla, x0)
		x0 += LADO + SEPARACION

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
func _dibujar_region(
	destino: Image, core: RefCounted, atlas: Image, semilla: String, x0: int
) -> void:
	var lado_chunk: int = core.lado_de_chunk()
	var desde := -LADO / 2

	# Se pide chunk por chunk y no tile por tile: es la misma llamada que va a
	# hacer el juego, así que si la API de chunks estuviera mal armada se vería
	# acá antes de escribir el cargador.
	var c0 := int(floor(float(desde) / lado_chunk))
	var c1 := int(floor(float(desde + LADO - 1) / lado_chunk))

	for cy in range(c0, c1 + 1):
		for cx in range(c0, c1 + 1):
			var datos: PackedByteArray = core.mundo_chunk(semilla, cx, cy)
			if datos.is_empty():
				continue
			for ty in lado_chunk:
				for tx in lado_chunk:
					var wx: int = cx * lado_chunk + tx
					var wy: int = cy * lado_chunk + ty
					var px: int = wx - desde
					var py: int = wy - desde
					if px < 0 or py < 0 or px >= LADO or py >= LADO:
						continue
					_pintar(destino, atlas, datos[ty * lado_chunk + tx], x0 + px, py)

	# Una cruz en el origen, para saber dónde va a caer el pueblo.
	for d in range(-6, 7):
		_marcar(destino, x0 + LADO / 2 + d, LADO / 2)
		_marcar(destino, x0 + LADO / 2, LADO / 2 + d)


## Un tile del mundo, reducido a un solo color.
##
## Se promedia el tile de 16×16 en vez de dibujarlo entero. A esta escala el
## detalle de cada tile no se ve y solo agregaría ruido; lo que se está mirando
## son las formas grandes: si los lagos tienen costa, si los bosques tienen
## claros, si los biomas se tocan de forma creíble.
func _pintar(destino: Image, atlas: Image, indice: int, px: int, py: int) -> void:
	var color := _promedio(atlas, indice)
	for dy in ESCALA:
		for dx in ESCALA:
			var x := px * ESCALA + dx
			var y := py * ESCALA + dy
			if x >= 0 and y >= 0 and x < destino.get_width() and y < destino.get_height():
				destino.set_pixel(x, y, color)


var _cache_promedio := {}


func _promedio(atlas: Image, indice: int) -> Color:
	if _cache_promedio.has(indice):
		return _cache_promedio[indice]

	var r := 0.0
	var g := 0.0
	var b := 0.0
	for y in TILE:
		for x in TILE:
			var c := atlas.get_pixel(indice * TILE + x, y)
			r += c.r
			g += c.g
			b += c.b
	var n := float(TILE * TILE)
	var color := Color(r / n, g / n, b / n)
	_cache_promedio[indice] = color
	return color


func _marcar(destino: Image, px: int, py: int) -> void:
	for dy in ESCALA:
		for dx in ESCALA:
			var x := px * ESCALA + dx
			var y := py * ESCALA + dy
			if x >= 0 and y >= 0 and x < destino.get_width() and y < destino.get_height():
				destino.set_pixel(x, y, Color("#ff6b6b"))
