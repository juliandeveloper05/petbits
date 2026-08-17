## fuente_a_png.gd
##
## Compone una muestra de la tipografía, sin abrir una ventana.
##
##   godot --headless --path godot --script res://scripts/fuente_a_png.gd
##   → godot/fuente.png
##
## ---
##
## POR QUÉ EXISTE.
##
## Los tests del C++ dicen que la fuente está completa, que ningún glifo se
## repite y que ninguno se sale de su caja. Los de Godot dicen que el motor los
## encuentra a todos. Nada de eso dice si se LEE. Una `S` que parece un `5`, una
## `rn` que se confunde con una `m`, un interlineado que amontona los renglones:
## todo eso pasa los tests con las mejores notas.
##
## Las frases de muestra no son inventadas: son textos que el juego imprime de
## verdad. Un panel de "AaBbCc" no avisa de que "expedición" lleva una tilde que
## puede chocar con el renglón de arriba.

extends SceneTree

const Escritura = preload("res://scripts/Escritura.gd")

const ESCALA := 3

## Fondo y tinta de la consola verde fósforo, para verla como se va a ver.
const FONDO := Color("#0a0e0a")
const TINTA := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")


func _init() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	var tipo: Dictionary = Escritura.preparar(core)
	var m: Dictionary = tipo["m"]
	var atlas: Image = tipo["atlas"]

	# [texto, color]. Las de arriba son el juego de caracteres; las de abajo,
	# frases reales de la interfaz.
	var lineas := [
		["PetBits", TINTA],
		["", TENUE],
		["ABCDEFGHIJKLMNOPQRSTUVWXYZ", TEXTO],
		["abcdefghijklmnopqrstuvwxyz", TEXTO],
		["0123456789", TEXTO],
		[".,:;!?()[]{}<>/\\|-_=+*&%$#@", TEXTO],
		["\"'`~^", TEXTO],
		["", TENUE],
		["acentuadas: á é í ó ú ü ñ", TEXTO],
		["ÁÉÍÓÚ  áéíóú  ñÑ  üÜ  ¿¡", TEXTO],
		["punto medio ·  raya —  flecha →  rombo ◆", TEXTO],
		["", TENUE],
		["-- lo que el juego dice de verdad --", TENUE],
		["Musgo · Plácido", TINTA],
		["Bebé · vuelve en 15 min", TINTA],
		["Nébula está de expedición", TINTA],
		["No te queda de eso. Mandala a buscar.", Color("#ffc23d")],
		["El bosque — Enter para mandarla", TEXTO],
		["¿Volviste? Mientras no estabas...", TEXTO],
		["A3F0-91C4-77BE-2D08", TINTA],
		["Energía 72  Ánimo 48  Salud 91", TEXTO],
		["", TENUE],
		["y al doble, como en el arranque:", TENUE],
	]

	var alto_linea: int = int(m["alto"]) + 2
	var margen := 6

	var ancho := 0
	for l in lineas:
		ancho = maxi(ancho, Escritura.ancho(tipo, l[0]))
	ancho = maxi(ancho, Escritura.ancho(tipo, "PETBITS 3.0", 2))
	ancho = maxi(ancho, atlas.get_width())

	var alto := margen
	alto += lineas.size() * alto_linea
	alto += int(m["alto"]) * 2 + margen        # el renglón al doble
	alto += margen + atlas.get_height() + margen

	var salida := Image.create_empty(
		(ancho + margen * 2) * ESCALA, alto * ESCALA, false, Image.FORMAT_RGBA8
	)
	salida.fill(FONDO)

	var y := margen
	for l in lineas:
		_linea(salida, tipo, l[0], margen, y, l[1], 1)
		y += alto_linea

	_linea(salida, tipo, "PETBITS 3.0", margen, y, TINTA, 2)
	y += int(m["alto"]) * 2 + margen

	# El atlas crudo abajo: el mapa de todo lo que existe, tal como sale del C++.
	y += margen
	for f in atlas.get_height():
		for c in atlas.get_width():
			if atlas.get_pixel(c, f).a > 0.0:
				for dy in ESCALA:
					for dx in ESCALA:
						salida.set_pixel(
							(margen + c) * ESCALA + dx, (y + f) * ESCALA + dy, TENUE
						)

	var error := salida.save_png("res://fuente.png")
	if error != OK:
		print("No se pudo guardar: %d" % error)
		quit(1)
		return

	print("Muestra escrita en %s" % ProjectSettings.globalize_path("res://fuente.png"))
	print("%d glifos · caja %dx%d · avance %d · ascenso %d · descenso %d" % [
		core.fuente_glifos().size(), m["ancho"], m["alto"],
		m["avance"], m["ascenso"], m["descenso"]
	])
	print("atlas %dx%d px" % [atlas.get_width(), atlas.get_height()])
	quit(0)


## Escribe en la imagen de salida, que está a ESCALA.
func _linea(
	destino: Image, tipo: Dictionary, texto: String,
	x: int, y: int, color: Color, escala: int
) -> void:
	Escritura.escribir(destino, tipo, texto, x * ESCALA, y * ESCALA, color, escala * ESCALA)
