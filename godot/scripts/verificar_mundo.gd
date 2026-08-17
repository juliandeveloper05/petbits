## verificar_mundo.gd
##
## Comprueba que el mapa y PetView sean el mismo juego.
##
##   godot --headless --path godot res://scenes/VerificarMundo.tscn
##
## Devuelve 0 si todo pasa, y la cantidad de fallas si no.
##
## ---
##
## POR QUÉ HACE FALTA UN TEST PARA ESTO.
##
## Que las dos pantallas abran y no revienten no prueba nada de lo que importa.
## La primera versión del mapa abría perfecto y mostraba una criatura al azar:
## caminabas, la mandabas al bosque, y la partida que estabas jugando no se
## enteraba. Eso es exactamente el tipo de bug que una captura de pantalla no
## muestra — se ve igual de bien estando roto.
##
## Así que lo que se comprueba es la continuidad: que el seed sea el mismo de un
## lado y del otro, que mandarla desde el mapa quede escrito en el archivo, y que
## al volver a PetView siga de expedición. Es la misma pregunta que el chequeo de
## interoperabilidad con la web, un piso más abajo.
##
## Corre contra un save aparte. Un test que juegue con tu partida no es un test.
##
## Es una escena y no un `--script` porque con `--script` el script reemplaza el
## main loop y los autoloads no se instancian: `Partida` no existiría. Además, un
## test del juego conviene que arranque como arranca el juego.

extends Node

const RUTA := "user://verificacion_mundo.json"

var _fallas := 0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	DirAccess.remove_absolute(ProjectSettings.globalize_path(RUTA))
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://verificacion_mundo.rota.json"

	print("\nPetBits — el mapa y la criatura son la misma partida\n")

	if not Partida.iniciar():
		print("No arrancó la partida.")
		get_tree().quit(1)
		return

	await _correr()

	DirAccess.remove_absolute(ProjectSettings.globalize_path(RUTA))

	if _fallas == 0:
		print("\nTodo bien: las dos pantallas juegan la misma partida.")
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


func _correr() -> void:
	# Un cuadro antes de tocar nada. Durante `_ready()` la raíz todavía está
	# armando sus hijos y `add_child` falla — pero falla con un error en consola y
	# devolviendo, no con una excepción, así que el test seguía corriendo contra
	# una pantalla que nunca había entrado al árbol y daba todo verde igual.
	await get_tree().process_frame

	# ---- PetView ----------------------------------------------------------
	var petview: Control = load("res://scenes/PetView.tscn").instantiate()
	get_tree().root.add_child(petview)
	await get_tree().process_frame
	await get_tree().process_frame

	_afirmar(petview.is_inside_tree(), "PetView entró al árbol")
	var seed_petview: String = Partida.core.estado()["seed"]
	print("PetView muestra:  %s" % seed_petview)
	_afirmar(seed_petview != "", "PetView tiene una criatura")

	petview.queue_free()
	await get_tree().process_frame

	# ---- El mapa ----------------------------------------------------------
	var mundo: Node2D = load("res://scenes/Mundo.tscn").instantiate()
	get_tree().root.add_child(mundo)
	await get_tree().process_frame
	await get_tree().process_frame

	_afirmar(mundo.is_inside_tree(), "el pueblo entró al árbol")
	var seed_mapa: String = Partida.core.estado()["seed"]
	print("El pueblo muestra: %s" % seed_mapa)
	_afirmar(seed_mapa == seed_petview, "es la MISMA criatura en las dos pantallas")
	_afirmar(mundo._criatura != null, "hay algo que caminar")

	# ---- Los árboles frenan ------------------------------------------------
	# El borde es la única pared del pueblo. Si no frenara, se podría salir del
	# mapa caminando y el resto de este test daría igual.
	_afirmar(mundo._choca(Vector2(8, 8)), "el borde de árboles frena")
	_afirmar(not mundo._choca(Vector2(15.5 * 16, 8.5 * 16)), "la plaza se camina")

	# ---- Caminar hasta el patio -------------------------------------------
	var patio: Dictionary = preload("res://scripts/PuebloMapa.gd").ZONAS[0]
	mundo._criatura.position = Vector2((patio["x"] + 0.5) * 16, (patio["y"] + 0.5) * 16)
	mundo._mirar_alrededor()
	_afirmar(mundo._zona_cerca.get("destino", "") == "patio", "pararse en la entrada la reconoce")

	# ---- Mandarla desde ahí ------------------------------------------------
	_afirmar(Partida.core.falta_para_volver(Partida.ahora_ms()) == 0, "antes de mandarla, está en casa")
	mundo._enviar()
	var falta: int = Partida.core.falta_para_volver(Partida.ahora_ms())
	print("Mandada al patio: vuelve en %d min" % ceili(falta / 60000.0))
	_afirmar(falta > 0, "mandarla desde el mapa la saca de verdad")

	# ---- Y quedó escrito ---------------------------------------------------
	# Esto es lo que separa "la interfaz dice que la mandé" de "la mandé". El
	# archivo se lee con un core nuevo, que no sabe nada de lo que pasó recién.
	var archivo := FileAccess.open(RUTA, FileAccess.READ)
	_afirmar(archivo != null, "el save existe en disco")
	if archivo != null:
		var otro: RefCounted = ClassDB.instantiate("PetBitsCore")
		var r: Dictionary = otro.cargar(archivo.get_as_text())
		archivo.close()
		_afirmar(r["ok"], "el save se vuelve a leer")
		if r["ok"]:
			_afirmar(otro.estado()["seed"] == seed_petview, "el save es de la misma criatura")
			_afirmar(
				otro.falta_para_volver(Partida.ahora_ms()) > 0,
				"la expedición quedó guardada, no solo en pantalla"
			)

	mundo.queue_free()
	await get_tree().process_frame

	# ---- Volver a PetView --------------------------------------------------
	var vuelta: Control = load("res://scenes/PetView.tscn").instantiate()
	get_tree().root.add_child(vuelta)
	await get_tree().process_frame
	await get_tree().process_frame

	_afirmar(Partida.core.estado()["seed"] == seed_petview, "al volver sigue siendo la misma")
	_afirmar(
		Partida.core.falta_para_volver(Partida.ahora_ms()) > 0,
		"al volver, sigue de expedición"
	)
	_afirmar(Partida.bitacora.size() > 0, "la bitácora sobrevivió al cambio de pantalla")

	vuelta.queue_free()
	await get_tree().process_frame


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		print("  ok   %s" % que)
	else:
		print("  FALLA %s" % que)
		_fallas += 1
