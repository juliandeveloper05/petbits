## hoja_de_contacto.gd
##
## Genera una hoja con muchas criaturas y la guarda como PNG.
##
##   godot --headless --path godot --script res://scripts/hoja_de_contacto.gd
##   → godot/hoja_de_contacto.png
##
## ---
##
## Es el equivalente nativo de `npm run sheet`, y existe por la misma razón: los
## tests dicen que el C++ coincide con el TypeScript, pero no dicen si lo que
## sale son criaturas o manchas. Eso solo se contesta mirando muchas juntas.
##
## La grilla de arriba muestra variedad entre genomas; la de abajo, el mismo
## genoma a lo largo de sus etapas y sus siete formas evolutivas. La segunda es
## la que responde si la crianza se nota: si las siete se ven iguales, el sesgo
## por forma no está haciendo nada.

extends SceneTree

const LADO := 32
const ESCALA := 3
const CELDA := LADO * ESCALA + 6

const COLUMNAS := 12
const FILAS_AZAR := 5

const FONDO := Color("#0a0e0a")
const SEPARADOR := Color("#3d5c46")

const ETAPAS := ["bebe", "juvenil", "adulto"]
const FORMAS := ["indefinida", "petreo", "vaporoso", "coloso", "guardian", "errante", "oraculo"]

## Fijo, para que dos corridas den la misma hoja y un diff visual signifique algo.
const SEED_MUESTRA := "A3F0-91C4-77BE-2D08"


func _init() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó. Compilá con `scons` en gdext/ primero.")
		quit(1)
		return

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")

	var filas_total := FILAS_AZAR + 1 + 1 + 1  # azar + separación + etapas + formas
	var ancho := COLUMNAS * CELDA
	var alto := filas_total * CELDA

	var hoja := Image.create_empty(ancho, alto, false, Image.FORMAT_RGBA8)
	hoja.fill(FONDO)

	# --- criaturas al azar, para ver variedad ---
	var puestas := 0
	for fila in range(FILAS_AZAR):
		for col in range(COLUMNAS):
			var seed: String = core.seed_al_azar()
			var img: Image = core.sprite(seed, "adulto", FORMAS[puestas % FORMAS.size()], false)
			_pegar(hoja, img, col, fila)
			puestas += 1

	# --- una franja para separar ---
	var y_franja := FILAS_AZAR * CELDA + CELDA / 2
	for x in range(ancho):
		hoja.set_pixel(x, y_franja, SEPARADOR)

	# --- el mismo genoma a lo largo de sus etapas ---
	for i in range(ETAPAS.size()):
		var img: Image = core.sprite(SEED_MUESTRA, ETAPAS[i], "indefinida", false)
		_pegar(hoja, img, i, FILAS_AZAR + 1)

	# --- y a lo largo de sus siete formas ---
	for i in range(FORMAS.size()):
		var img: Image = core.sprite(SEED_MUESTRA, "adulto", FORMAS[i], false)
		_pegar(hoja, img, i, FILAS_AZAR + 2)

	# --- y parpadeando, que es lo único que cambia la cara sin tocar el cuerpo ---
	_pegar(hoja, core.sprite(SEED_MUESTRA, "adulto", "indefinida", true), 8, FILAS_AZAR + 2)

	var destino := "res://hoja_de_contacto.png"
	var error := hoja.save_png(destino)
	if error != OK:
		print("No se pudo guardar: error %d" % error)
		quit(1)
		return

	print("Hoja escrita en %s" % ProjectSettings.globalize_path(destino))
	print("%d criaturas al azar + %d etapas + %d formas" % [puestas, ETAPAS.size(), FORMAS.size()])
	quit(0)


## Copia un sprite a la grilla, ampliado con vecino más cercano.
##
## Se amplía a mano y no con Image.resize() porque resize interpola: a 32×32 eso
## convierte el pixel art en una mancha borrosa. Acá cada píxel se repite tal
## cual, que es lo único que tiene sentido para esto.
func _pegar(hoja: Image, sprite: Image, col: int, fila: int) -> void:
	if sprite == null:
		return
	var x0 := col * CELDA + 3
	var y0 := fila * CELDA + 3
	for y in range(LADO):
		for x in range(LADO):
			var c := sprite.get_pixel(x, y)
			if c.a == 0.0:
				continue
			for dy in range(ESCALA):
				for dx in range(ESCALA):
					hoja.set_pixel(x0 + x * ESCALA + dx, y0 + y * ESCALA + dy, c)
