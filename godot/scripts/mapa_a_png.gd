## mapa_a_png.gd
##
## Compone los INTERIORES en un PNG, sin abrir una ventana.
##
##   godot --headless --path godot res://scenes/MapaAPng.tscn
##   → godot/mapa.png
##
## ---
##
## ANTES DIBUJABA TAMBIÉN EL PUEBLO, Y YA NO.
##
## El pueblo dejó de tener grilla propia: su terreno se mudó al generador del
## mundo, porque ahora ES una región del mundo. Esta herramienta lo pedía con
## `generar()` y esa función ya no existe — el render se colgaba sin decir nada,
## que es como se descubrió.
##
## Y está bien que no exista: el pueblo se mira en `region_a_png`, junto con el
## terreno que lo rodea, que es donde de verdad se juzga si se integra. Acá quedan
## los dos interiores, que sí son grillas fijas y sí tienen sentido mirar solas.
##
## ---
##
## Godot en modo headless no dibuja nada —pedirle una captura devuelve negro— así
## que la imagen se compone a mano, tile por tile, con el mismo atlas que usa el
## juego.

extends Node

const Escritura = preload("res://scripts/Escritura.gd")
const CajaDialogo = preload("res://scripts/CajaDialogo.gd")
const Mapas = preload("res://scripts/Mapas.gd")

const TILE := 16
const ESCALA := 2

## Aire entre un mapa y el siguiente, en píxeles del juego.
const SEPARACION := 10

var _atlas: Image = null
var _layout := {}


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	_atlas = core.atlas_tiles()
	_layout = core.atlas_layout()

	# Hace falta una criatura para poder dibujarla. Se usa un seed fijo y no uno
	# al azar: así dos corridas dan el mismo PNG y un diff visual significa que
	# cambió el mapa, no que salió otro bicho.
	core.nacer("A3F0-91C4-77BE-2D08", 1786406400000, -180)

	var tipo: Dictionary = Escritura.preparar(core)

	# Solo los mapas que NO son infinitos: los que tienen grilla propia.
	var ids: Array = []
	for id in Mapas.todos():
		if not Mapas.script_de(id).INFINITO:
			ids.append(id)

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
				var d: Dictionary = core.tile_de_grilla(celdas[y][x])
				_pegar_tile(salida, int(d["col"]), int(d["fila"]), x * TILE, y0 + y * TILE)

		var sprite: Image = core.sprite_actual(false)
		_pegar_sprite(salida, sprite, def.ENTRADA.x - 8, y0 + def.ENTRADA.y - 8)

		_cartel(salida, tipo, id, 6, y0 + 4, Color("#9bbc0f"))
		y0 += def.ALTO * TILE + SEPARACION

	_caja(
		salida, tipo,
		"Los pedestales. Traé dos criaturas adultas y algo va a pasar.",
		alto_total - SEPARACION
	)

	var error := salida.save_png("res://mapa.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		get_tree().quit(1)
		return

	print("Interiores escritos en %s" % ProjectSettings.globalize_path("res://mapa.png"))
	print("%d interiores: %s" % [ids.size(), ", ".join(ids)])
	get_tree().quit(0)


## Un tile llano del atlas, en su fila.
func _pegar_tile(destino: Image, columna: int, fila: int, px: int, py: int) -> void:
	for y in TILE:
		for x in TILE:
			var c := _atlas.get_pixel(columna * TILE + x, fila * TILE + y)
			if c.a == 0.0:
				continue
			for dy in ESCALA:
				for dx in ESCALA:
					destino.set_pixel((px + x) * ESCALA + dx, (py + y) * ESCALA + dy, c)


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
