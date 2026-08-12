## verificar_puente.gd
##
## Comprueba, sin abrir el editor, que Godot carga la GDExtension y que los
## valores llegan intactos hasta GDScript.
##
##   godot --headless --path godot --script res://scripts/verificar_puente.gd
##
## ---
##
## QUÉ VERIFICA ESTO Y QUÉ NO.
##
## Los algoritmos ya están verificados en gdext/tests: 40.416 comprobaciones
## contra lo que devuelve el TypeScript. Repetir eso acá no agregaría nada.
##
## Lo que falta comprobar es el último tramo, que esos tests no tocan: que la
## biblioteca cargue en el motor, que la clase quede registrada, y que los
## valores sobrevivan el viaje C++ → Variant → GDScript. Ahí hay cosas que
## pueden romperse solas —enteros de 64 bits que no entran en el int con signo
## de GDScript, texto con acentos que se convierte mal— y que se ven perfectas
## del lado del C++.
##
## Devuelve 0 si está todo bien y 1 si algo no coincide, así que sirve tal cual
## en un script.

extends SceneTree

## Los esperados salen de gdext/tests/vectores_generados.h, que a su vez sale de
## ejecutar el TypeScript. No están calculados a mano.
const SEED_EJEMPLO := "A3F0-91C4-77BE-2D08"

var _fallas := 0


func _init() -> void:
	print("\nPetBits — puente GDExtension → GDScript")
	print("Godot %s\n" % Engine.get_version_info().string)

	if not ClassDB.class_exists("PetBitsCore"):
		print("FALLA: la clase PetBitsCore no está registrada.")
		print("       La extensión no cargó. Revisá que exista el .dll en godot/bin/")
		print("       y que su nombre coincida con petbits_core.gdextension.")
		quit(1)
		return

	print("La extensión cargó y PetBitsCore está registrada.")

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	print("%s\n" % core.version())

	# El "·" de version() es un byte doble en UTF-8. Si el C++ arma el String
	# con el constructor desde const char* en vez de String::utf8(), cada byte
	# se ensancha por separado y aparece "Â·". Pasó de verdad, así que queda
	# comprobado: es un error silencioso que solo se ve mirando la pantalla.
	_igual(core.version().contains("Â"), false, "version() sin mojibake")

	_probar_genes(core)
	_probar_rarezas(core)
	_probar_acentos(core)
	_probar_seeds_grandes(core)

	if _fallas == 0:
		print("\nPuente OK: los valores llegan intactos hasta GDScript.")
		quit(0)
	else:
		print("\n%d fallas." % _fallas)
		quit(1)


func _igual(obtenido: Variant, esperado: Variant, que: String) -> void:
	if obtenido == esperado:
		return
	_fallas += 1
	print("  FALLA %s: dio %s, se esperaba %s" % [que, str(obtenido), str(esperado)])


func _probar_genes(core: RefCounted) -> void:
	print("decodificar(\"%s\")" % SEED_EJEMPLO)
	var g: Dictionary = core.decodificar(SEED_EJEMPLO)

	if g.is_empty():
		_fallas += 1
		print("  FALLA: devolvió un diccionario vacío")
		return

	_igual(g["seed"], SEED_EJEMPLO, "seed")
	_igual(g["lineage"], 8, "lineage")
	_igual(g["bodyShape"], 0, "bodyShape")
	_igual(g["eyes"], 13, "eyes")
	_igual(g["mouth"], 2, "mouth")
	_igual(g["appendages"], 14, "appendages")
	_igual(g["pattern"], 11, "pattern")
	_igual(g["hue"], 119, "hue")
	_igual(g["paletteMode"], 4, "paletteMode")
	_igual(g["temperament"], 0, "temperament")
	_igual(g["metabolism"], 7, "metabolism")
	_igual(g["affinity"], 0, "affinity")
	_igual(g["proportion"], 9, "proportion")
	_igual(g["mutation"], 163, "mutation")

	var sesgo: Dictionary = g["statBias"]
	_igual(sesgo["vigor"], 0, "statBias.vigor")
	_igual(sesgo["animo"], 0, "statBias.animo")
	_igual(sesgo["ingenio"], 3, "statBias.ingenio")
	_igual(sesgo["vinculo"], 3, "statBias.vinculo")

	# Los nombres viajan como String y llevan acentos: es donde una conversión
	# mal hecha se nota.
	_igual(g["linaje"], "Vapor", "linaje")
	_igual(g["temperamento"], "Plácido", "temperamento")
	_igual(g["afinidad"], "Brasa", "afinidad")
	_igual(g["metabolismo"], "Frenético", "metabolismo")

	print("  %s · %s · %s" % [g["linaje"], g["temperamento"], g["afinidad"]])


func _probar_rarezas(core: RefCounted) -> void:
	# El seed 0 tiene tres rarezas: popcount 0 (Vacío), primer byte igual al
	# último (Uróboros) y los 16 bits altos espejo de los bajos (Espejo).
	print("rarezas(\"0\")")
	var rarezas: Array = core.rarezas("0")
	_igual(rarezas.size(), 3, "cantidad de rarezas")

	var ids := []
	for r in rarezas:
		ids.append(r["id"])
	ids.sort()
	_igual(ids, ["espejo", "uroboros", "vacio"], "ids de rarezas")

	for r in rarezas:
		print("  ◆ %s (%s) — %s" % [r["nombre"], r["tier"], r["regla"]])

	# El seed de ejemplo no tiene ninguna: el caso vacío también importa.
	_igual(core.rarezas(SEED_EJEMPLO).size(), 0, "rarezas del seed de ejemplo")


func _probar_acentos(core: RefCounted) -> void:
	# Escribir un texto como seed lo hashea. Con acentos el hash se calcula
	# sobre unidades UTF-16, no sobre bytes UTF-8, para coincidir con el
	# charCodeAt del TypeScript. Si el String de Godot se convirtiera mal en el
	# camino, este valor cambiaría.
	print("formatear_seed(\"Nébula\")")
	var obtenido: String = core.formatear_seed("Nébula")
	_igual(obtenido, "4347-1D9E-073B-4812", "hash de texto con acento")
	print("  %s" % obtenido)


func _probar_seeds_grandes(core: RefCounted) -> void:
	# Seeds con el bit 63 encendido: son la mitad del espacio posible, y si
	# cruzaran como entero llegarían negativos al int con signo de GDScript. Por
	# eso los seeds viajan como texto.
	print("seeds con el bit más alto encendido")
	_igual(core.formatear_seed("FFFFFFFFFFFFFFFF"), "FFFF-FFFF-FFFF-FFFF", "seed máximo")
	_igual(core.formatear_seed("0x8000000000000000"), "8000-0000-0000-0000", "bit 63")
	_igual(core.formatear_seed("0"), "0000-0000-0000-0000", "seed cero")

	# El prefijo 0x de arriba no es decorativo, y esto lo fija por escrito:
	# "8000000000000000" son todos dígitos, así que se lee como DECIMAL y da otra
	# criatura. El orden de parseSeed pone el hexadecimal sin letras último, para
	# que "1234" signifique mil doscientos treinta y cuatro y no 0x1234.
	#
	# Es una diferencia visible para el jugador —dos textos parecidos, dos bichos
	# distintos— así que conviene que un cambio de comportamiento acá rompa algo.
	_igual(core.formatear_seed("8000000000000000"), "001C-6BF5-2634-0000", "todo dígitos = decimal")
