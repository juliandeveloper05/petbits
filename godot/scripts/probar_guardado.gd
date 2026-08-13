## probar_guardado.gd
##
## Comprueba que la partida sobrevive el cierre del juego.
##
##   godot --headless --path godot --script res://scripts/probar_guardado.gd
##
## ---
##
## Los tests de C++ verifican el formato. Esto verifica el circuito completo, que
## es otra cosa: escribir el archivo de verdad, crear un núcleo NUEVO como si el
## juego se hubiera cerrado y vuelto a abrir, leerlo de vuelta, y que la criatura
## sea la misma.
##
## Un save que se serializa bien pero que el juego nunca escribe, o que escribe
## en un lugar que no vuelve a leer, pasaría todos los tests del formato igual.

extends SceneTree

const RUTA := "user://prueba_guardado.json"
const INICIO_MS := 1786406400000
const TICK_MS := 60000

var _fallas := 0


func _init() -> void:
	print("\nPetBits — la partida sobrevive al cierre")

	if not ClassDB.class_exists("PetBitsCore"):
		print("FALLA: la GDExtension no cargó.")
		quit(1)
		return

	# --- sesión 1: nace, vive un día, come y se guarda ---
	var uno: RefCounted = ClassDB.instantiate("PetBitsCore")
	uno.nacer("A3F0-91C4-77BE-2D08", INICIO_MS, -180)
	uno.simular(INICIO_MS + 1441 * TICK_MS)
	uno.alimentar("larva", INICIO_MS + 1441 * TICK_MS)
	uno.acariciar(INICIO_MS + 1441 * TICK_MS)

	var antes: Dictionary = uno.estado()
	var texto: String = uno.guardar(INICIO_MS + 1441 * TICK_MS)

	var f := FileAccess.open(RUTA, FileAccess.WRITE)
	f.store_string(texto)
	f.close()
	print("  guardado: %d bytes" % texto.length())

	# --- sesión 2: un núcleo nuevo, como si el juego se hubiera reabierto ---
	var dos: RefCounted = ClassDB.instantiate("PetBitsCore")
	var leido := FileAccess.open(RUTA, FileAccess.READ)
	var r: Dictionary = dos.cargar(leido.get_as_text())
	leido.close()

	if not r["ok"]:
		print("  FALLA: no se pudo cargar — %s" % r["mensaje"])
		quit(1)
		return

	var despues: Dictionary = dos.estado()

	_igual(despues["seed"], antes["seed"], "seed")
	_igual(despues["id"], antes["id"], "id")
	_igual(despues["etapa"], antes["etapa"], "etapa")
	_igual(despues["forma"], antes["forma"], "forma")
	_igual(despues["ticks_vividos"], antes["ticks_vividos"], "ticks_vividos")
	_igual(despues["ticks_activos"], antes["ticks_activos"], "ticks_activos")
	_igual(despues["letargico"], antes["letargico"], "letargico")

	# Los stats con todos sus decimales. Si el guardado los redondeara, acá se
	# vería: son valores acumulados a lo largo de 1441 ticks.
	var a: Dictionary = antes["stats"]
	var b: Dictionary = despues["stats"]
	for clave in ["energia", "animo", "salud", "vinculo"]:
		_igual(b[clave], a[clave], "stats.%s" % clave)

	print("  %s · %s · salud %.13f" % [despues["etapa"], despues["forma"], b["salud"]])

	# El sprite tiene que ser el mismo bicho, no uno parecido.
	var sa: Image = uno.sprite_actual(false)
	var sb: Image = dos.sprite_actual(false)
	_igual(sb.get_data() == sa.get_data(), true, "el sprite es idéntico")

	# --- y un archivo roto no puede tumbar el arranque ---
	var tres: RefCounted = ClassDB.instantiate("PetBitsCore")
	var roto: Dictionary = tres.cargar("{\"version\":5,\"criaturas\":[")
	_igual(roto["ok"], false, "un save cortado se rechaza")
	_igual(roto["mensaje"] != "", true, "el rechazo explica el motivo")
	print("  save cortado: %s" % roto["mensaje"])

	DirAccess.remove_absolute(ProjectSettings.globalize_path(RUTA))

	if _fallas == 0:
		print("\nLa partida sobrevive: mismo bicho, mismos números.")
		quit(0)
	else:
		print("\n%d fallas." % _fallas)
		quit(1)


func _igual(obtenido: Variant, esperado: Variant, que: String) -> void:
	if obtenido == esperado:
		return
	_fallas += 1
	print("  FALLA %s: dio %s, se esperaba %s" % [que, str(obtenido), str(esperado)])
