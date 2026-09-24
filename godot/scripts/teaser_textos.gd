## teaser_textos.gd — los subtítulos del teaser, en la tipografía del juego.
##
##   godot --headless --path godot --script res://scripts/teaser_textos.gd
##   → godot/teaser/tramo_*.png
##
## Una imagen de 1080×1920 por tramo, transparente salvo el texto y el marco. El
## armado las pone encima del video del juego, cada una en su momento.
##
## ---
##
## LA LETRA ES LA DEL JUEGO, NO UNA PARECIDA.
##
## Se dibuja glifo por glifo desde el atlas que genera el C++, igual que la
## muestra de la tipografía. Una fuente de 8 bits cualquiera se habría visto
## "retro" y ajena; ésta es la misma que el tester va a leer en el juego, con
## sus tildes y sus eñes.
##
## Escalas enteras y nada más: a 4 cada píxel de la fuente son 4×4 píxeles de
## video, y el texto queda tan nítido como el juego ampliado al doble.

extends SceneTree

const Escritura = preload("res://scripts/Escritura.gd")

const ANCHO := 1080
const ALTO := 1920

## Dónde va la banda del juego en el video: 480×270 al doble, centrada a lo
## ancho. `tools/armar_teaser.ps1` la pone en el mismo lugar con `pad`: si se
## mueve una, se mueve la otra.
##
## A lo alto, lo que importa es el conjunto —título, paso, juego, subtítulo—, que
## queda centrado entre las dos franjas que tapa Instagram.
const BANDA := Rect2i(60, 700, 960, 540)

const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")
const BORDE := Color("#3d5c46")
const FONDO := Color("#0a0e0a")

## Cada tramo: el paso (arriba) y dos renglones (abajo).
const TRAMOS := {
	"titulo": ["", "Una mascota virtual,", "como las de antes."],
	"semilla": ["1 · Nace de un número", "Ese número es su forma,", "sus colores y su carácter."],
	"cuidala": ["2 · Cuidala", "Come, juega, duerme,", "y crece aunque no estés."],
	"pueblo": ["3 · Salí a caminar", "Un pueblo con gente,", "y un mundo que no termina."],
	"mundo": ["4 · Buscá semillas", "Cada una es una criatura", "que nadie vio antes."],
}


func _init() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	var tipo: Dictionary = Escritura.preparar(core)
	var alto_letra: int = int(tipo["m"]["alto"])

	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://teaser"))

	for nombre in TRAMOS:
		var t: Array = TRAMOS[nombre]
		var img := Image.create_empty(ANCHO, ALTO, false, Image.FORMAT_RGBA8)
		img.fill(Color(0, 0, 0, 0))

		_centrado(img, tipo, "PetBits", BANDA.position.y - 270, FOSFORO, 8)
		if t[0] != "":
			_centrado(img, tipo, t[0], BANDA.position.y - 140, FOSFORO, 4)
		_marco(img)
		_centrado(img, tipo, t[1], BANDA.end.y + 60, TEXTO, 4)
		_centrado(img, tipo, t[2], BANDA.end.y + 60 + alto_letra * 4 + 14, TEXTO, 4)

		img.save_png("res://teaser/tramo_%s.png" % nombre)
		print("  tramo_%s.png" % nombre)

	# El cierre: pantalla entera, opaca.
	var cierre := Image.create_empty(ANCHO, ALTO, false, Image.FORMAT_RGBA8)
	cierre.fill(FONDO)
	_centrado(cierre, tipo, "PetBits", 780, FOSFORO, 10)
	_centrado(cierre, tipo, "Muy pronto.", 980, TEXTO, 5)
	_centrado(cierre, tipo, "Un juego de Julian Soto", 1120, TENUE, 3)
	cierre.save_png("res://teaser/tramo_cierre.png")
	print("  tramo_cierre.png")

	quit(0)


func _centrado(img: Image, tipo: Dictionary, texto: String, y: int, color: Color, escala: int) -> void:
	var ancho := Escritura.ancho(tipo, texto, escala)
	Escritura.escribir(img, tipo, texto, (ANCHO - ancho) / 2, y, color, escala)


## Un marco doble alrededor de la banda del juego, como el de la caja de diálogo.
func _marco(img: Image) -> void:
	for par in [[6, BORDE], [3, FOSFORO]]:
		var separacion: int = par[0]
		var color: Color = par[1]
		var r := BANDA.grow(separacion)
		for x in range(r.position.x, r.end.x):
			for grosor in 2:
				img.set_pixel(x, r.position.y + grosor, color)
				img.set_pixel(x, r.end.y - 1 - grosor, color)
		for y in range(r.position.y, r.end.y):
			for grosor in 2:
				img.set_pixel(r.position.x + grosor, y, color)
				img.set_pixel(r.end.x - 1 - grosor, y, color)
