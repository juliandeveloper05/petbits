## mapa_a_png.gd
##
## Compone el mundo en un PNG, sin abrir una ventana.
##
##   godot --headless --path godot res://scenes/MapaAPng.tscn
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
##
## ---
##
## ES UNA ESCENA Y NO UN `--script`.
##
## Pasó a serlo al agregarle la caja de diálogo: para no reimplementar el corte
## de línea usa el de `CajaDialogo`, y ese script menciona el autoload `Partida`.
## Con `--script` los autoloads no llegan a instanciarse, el script no compila,
## `load()` devuelve null y la herramienta se cuelga sin decir por qué. Como
## escena no pasa — y de paso no toca tu partida, porque `Partida` no se inicia
## solo.

extends Node

const Escritura = preload("res://scripts/Escritura.gd")
const CajaDialogo = preload("res://scripts/CajaDialogo.gd")
const Mapas = preload("res://scripts/Mapas.gd")

const TILE := 16
const ESCALA := 2

## Aire entre un mapa y el siguiente, en píxeles del juego.
const SEPARACION := 10

var _atlas: Image = null


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	_atlas = core.atlas_tiles()

	# Hace falta una criatura para poder dibujarla. Se usa un seed fijo y no uno
	# al azar: así dos corridas dan el mismo PNG y un diff visual significa que
	# cambió el mapa, no que salió otro bicho.
	core.nacer("A3F0-91C4-77BE-2D08", 1786406400000, -180)

	# Los mapas se leen de donde están definidos, así que si alguno se rediseña
	# esta herramienta muestra el nuevo sin tocar una línea. Y no se instancia
	# nada: `generar()` es estática y devuelve la grilla y ya.
	var tipo: Dictionary = Escritura.preparar(core)
	var ids: Array = Mapas.todos()

	# Los tres van uno debajo del otro, en una sola imagen. Verlos juntos es lo
	# que dice si conviven: el pueblo es verde y frío y los interiores son
	# madera, y esa diferencia es la que hace que entrar a un lugar se sienta
	# como entrar a un lugar sin necesidad de ninguna transición.
	var ancho_total := 0
	var alto_total := 0
	for id in ids:
		var def = Mapas.script_de(id)
		ancho_total = maxi(ancho_total, def.ANCHO * TILE)
		alto_total += def.ALTO * TILE + SEPARACION

	var salida := Image.create_empty(
		ancho_total * ESCALA, alto_total * ESCALA, false, Image.FORMAT_RGBA8
	)
	salida.fill(Color("#0a0e0a"))

	var y0 := 0
	for id in ids:
		var def = Mapas.script_de(id)
		var celdas: Array = def.generar()

		for y in def.ALTO:
			for x in def.ANCHO:
				_pegar_tile(salida, celdas[y][x], x, y + y0 / TILE)

		# La criatura, donde arranca en ese mapa.
		var sprite: Image = core.sprite_actual(false)
		_pegar_sprite(salida, sprite, def.ENTRADA.x - 8, y0 + def.ENTRADA.y - 8)

		# Y el nombre del mapa arriba a la izquierda, con la tipografía del
		# juego. No es decoración: el texto sobre pasto y sobre madera es donde se
		# ve si la fuente tiene contraste suficiente, y eso no se puede contestar
		# mirando el atlas sobre negro.
		_cartel(salida, tipo, id, 6, y0 + 4, Color("#9bbc0f"))

		y0 += def.ALTO * TILE + SEPARACION

	# Y la caja de diálogo sobre el último mapa, que es lo que de verdad hay que
	# mirar: si el marco pisa el mapa, si el texto respira, si tres renglones
	# alcanzan.
	_caja(
		salida, tipo,
		"El bosque. Hay cosas raras entre los árboles, pero está lejos:"
		+ " hora y media de ida y vuelta.",
		alto_total - SEPARACION
	)

	var error := salida.save_png("res://mapa.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		get_tree().quit(1)
		return

	print("Mapa escrito en %s" % ProjectSettings.globalize_path("res://mapa.png"))
	print("%d mapas: %s" % [ids.size(), ", ".join(ids)])
	print("%d x %d px" % [salida.get_width(), salida.get_height()])
	get_tree().quit(0)


## La caja de diálogo, tal como la va a dibujar Godot.
##
## El corte de línea NO se reimplementa: sale de `CajaDialogo.partir()`, que es
## la misma función que usa el juego. Lo único que se repite acá son las tres
## llamadas del marco —un relleno y dos rectángulos— y hasta eso lee sus colores
## y sus márgenes de las constantes de la caja, así que no puede irse quedando
## vieja sin que se note.
func _caja(destino: Image, tipo: Dictionary, texto: String, abajo_de: int) -> void:
	var alto_fuente := int(tipo["m"]["alto"])
	var margen := CajaDialogo.MARGEN

	# Las mismas cuentas que hace la escena: la caja se ancla al borde de abajo
	# del viewport, no al del mapa.
	var vp := Vector2i(
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height")
	)
	var w: int = vp.x - 8
	var h: int = CajaDialogo.RENGLONES * (alto_fuente + 2) + margen * 2
	var x0 := 4
	var y0: int = abajo_de - h - 4

	_rect(destino, x0, y0, w, h, CajaDialogo.FONDO, true)
	_rect(destino, x0, y0, w, h, CajaDialogo.BORDE, false)
	_rect(destino, x0 + 2, y0 + 2, w - 4, h - 4, CajaDialogo.FOSFORO, false)

	var columnas := int((w - (margen + 2) * 2) / 6.0)
	var renglones: Array = CajaDialogo.partir(texto, columnas)
	for i in mini(renglones.size(), CajaDialogo.RENGLONES):
		Escritura.escribir(
			destino, tipo, renglones[i],
			(x0 + margen + 2) * ESCALA, (y0 + margen + i * (alto_fuente + 2)) * ESCALA,
			CajaDialogo.TEXTO, ESCALA
		)

	# El triangulito de "seguí", abajo a la derecha.
	Escritura.escribir(
		destino, tipo, "▼",
		(x0 + w - margen - 6) * ESCALA, (y0 + h - margen - alto_fuente) * ESCALA,
		CajaDialogo.FOSFORO, ESCALA
	)


func _rect(destino: Image, x: int, y: int, w: int, h: int, color: Color, relleno: bool) -> void:
	for f in h:
		for c in w:
			if not relleno and not (f == 0 or f == h - 1 or c == 0 or c == w - 1):
				continue
			for dy in ESCALA:
				for dx in ESCALA:
					var px := (x + c) * ESCALA + dx
					var py := (y + f) * ESCALA + dy
					if px >= 0 and py >= 0 and px < destino.get_width() and py < destino.get_height():
						destino.set_pixel(px, py, color)


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
