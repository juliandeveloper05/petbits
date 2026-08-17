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

	# Se instancia el script del mundo para no duplicar el mapa: si el pueblo se
	# rediseña, esta herramienta muestra el nuevo sin tocar una línea.
	#
	# Es un Node2D que nunca entra en el árbol, así que nadie lo va a liberar: hay
	# que hacerlo a mano. Sin eso Godot avisa de RIDs filtrados al salir y devuelve
	# 1, y una herramienta que termina en error es una herramienta que no se puede
	# encadenar con nada.
	var mundo: Node2D = load("res://scripts/Mundo.gd").new()
	mundo._construir_mapa()

	var ancho: int = mundo.ANCHO
	var alto: int = mundo.ALTO
	var celdas: Array = mundo._mapa.duplicate(true)
	mundo.free()

	var salida := Image.create_empty(ancho * TILE * ESCALA, alto * TILE * ESCALA, false, Image.FORMAT_RGBA8)

	for y in alto:
		for x in ancho:
			_pegar_tile(salida, celdas[y][x], x, y)

	# La criatura, donde arranca.
	var sprite: Image = core.sprite_actual(false)
	_pegar_sprite(salida, sprite, 15.5 * TILE - 8, 8.5 * TILE - 8)

	var error := salida.save_png("res://mapa.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		quit(1)
		return

	print("Mapa escrito en %s" % ProjectSettings.globalize_path("res://mapa.png"))
	print("%d x %d tiles, %d x %d px" % [ancho, alto, salida.get_width(), salida.get_height()])
	quit(0)


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
