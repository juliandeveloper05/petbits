## mapa_a_png.gd
##
## Compone el mundo en un PNG, sin abrir una ventana.
##
##   godot --headless --path godot --script res://scripts/mapa_a_png.gd
##   → godot/mapa.png
##
## ---
##
## Existe por la misma razón que la hoja de contacto de las criaturas: los tests
## dicen que los tiles son opacos y distinguibles, pero no dicen si el pueblo se
## LEE como un pueblo. Eso solo se contesta mirándolo.
##
## Y hace falta que sea headless porque el modo sin ventana de Godot no dibuja
## nada: pedirle una captura de pantalla devuelve negro. Así que el mapa se
## compone a mano, tile por tile, con las mismas imágenes que usa el juego.

extends SceneTree

const Escritura = preload("res://scripts/Escritura.gd")

const TILE := 16
const ESCALA := 2

var _atlas: Image = null


func _init() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	_atlas = core.atlas_tiles()

	# Hace falta una criatura para poder dibujarla. Se usa un seed fijo y no uno
	# al azar: así dos corridas dan el mismo PNG y un diff visual significa que
	# cambió el mapa, no que salió otro bicho.
	core.nacer("A3F0-91C4-77BE-2D08", 1786406400000, -180)

	# El pueblo se lee de donde está definido, así que si se rediseña esta
	# herramienta muestra el nuevo sin tocar una línea. Y no se instancia nada:
	# `generar()` es estática y devuelve la grilla y ya.
	var Pueblo = load("res://scripts/PuebloMapa.gd")
	var ancho: int = Pueblo.ANCHO
	var alto: int = Pueblo.ALTO
	var celdas: Array = Pueblo.generar()

	var salida := Image.create_empty(ancho * TILE * ESCALA, alto * TILE * ESCALA, false, Image.FORMAT_RGBA8)

	for y in alto:
		for x in ancho:
			_pegar_tile(salida, celdas[y][x], x, y)

	# La criatura, donde arranca.
	var sprite: Image = core.sprite_actual(false)
	_pegar_sprite(salida, sprite, Pueblo.ENTRADA.x - 8, Pueblo.ENTRADA.y - 8)

	# Y los carteles, con la tipografía del juego. No es decoración: el texto
	# sobre pasto y sobre camino es donde se ve si la fuente tiene contraste
	# suficiente, y eso no se puede contestar mirando el atlas sobre negro.
	var tipo: Dictionary = Escritura.preparar(core)
	_cartel(salida, tipo, core.estado()["seed"], 6, 4, Color("#9bbc0f"))
	_cartel(salida, tipo, "El patio — Enter para mandarla", 6, alto * TILE - 18, Color("#d6e6d0"))

	var error := salida.save_png("res://mapa.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		quit(1)
		return

	print("Mapa escrito en %s" % ProjectSettings.globalize_path("res://mapa.png"))
	print("%d x %d tiles, %d x %d px" % [ancho, alto, salida.get_width(), salida.get_height()])
	quit(0)


## Un cartel del mapa: el texto con su contorno oscuro.
##
## El contorno hace el trabajo de una caja de diálogo sin ocupar el lugar de una.
## Se dibuja pintando el texto ocho veces corrido un píxel en cada dirección y
## después encima en el color bueno: es la forma barata, y a este tamaño es
## indistinguible de la buena.
func _cartel(destino: Image, tipo: Dictionary, texto: String, x: int, y: int, color: Color) -> void:
	const SOMBRA := Color("#0a0e0a")
	for dy in [-1, 0, 1]:
		for dx in [-1, 0, 1]:
			if dx == 0 and dy == 0:
				continue
			Escritura.escribir(
				destino, tipo, texto, (x + dx) * ESCALA, (y + dy) * ESCALA, SOMBRA, ESCALA
			)
	Escritura.escribir(destino, tipo, texto, x * ESCALA, y * ESCALA, color, ESCALA)


func _pegar_tile(destino: Image, indice: int, cx: int, cy: int) -> void:
	for y in TILE:
		for x in TILE:
			var c := _atlas.get_pixel(indice * TILE + x, y)
			for dy in ESCALA:
				for dx in ESCALA:
					destino.set_pixel(
						(cx * TILE + x) * ESCALA + dx,
						(cy * TILE + y) * ESCALA + dy,
						c
					)


## El sprite va a media escala, igual que en el juego: 32×32 a 0.5 ocupa un tile.
func _pegar_sprite(destino: Image, sprite: Image, px: float, py: float) -> void:
	for y in sprite.get_height():
		for x in sprite.get_width():
			var c := sprite.get_pixel(x, y)
			if c.a == 0.0:
				continue
			var dx := int((px + x * 0.5) * ESCALA)
			var dy := int((py + y * 0.5) * ESCALA)
			for oy in ESCALA:
				for ox in ESCALA:
					if dx + ox < destino.get_width() and dy + oy < destino.get_height():
						destino.set_pixel(dx + ox, dy + oy, c)
