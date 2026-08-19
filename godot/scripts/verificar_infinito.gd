## verificar_infinito.gd
##
## Comprueba que el mundo infinito sea un lugar y no un fondo de pantalla.
##
##   godot --headless --path godot res://scenes/VerificarInfinito.tscn
##
## ---
##
## LO QUE ESTO CONTESTA Y LOS TESTS DEL C++ NO.
##
## Del lado del núcleo ya está comprobado que el terreno cierra sus costuras, que
## es determinista, que ningún bioma se come el mundo y que se puede caminar de
## punta a punta. Todo eso es sobre la FUNCIÓN que genera tiles.
##
## Acá se prueba la otra mitad: que el juego use esa función bien. Que salir del
## pueblo no te tire al vacío, que los chunks se carguen y se liberen mientras
## caminás, que levantar algo del suelo llegue a la despensa, y que el archivo del
## nativo se acuerde de dónde estabas SIN meter un campo en el save compartido.
##
## Esa última es la que más importa y la más fácil de romper sin notarlo: agregar
## "x" e "y" a `partida.json` funcionaría perfecto durante meses, y rompería la
## promesa de que la web y el nativo escriben exactamente lo mismo.

extends Node

const RUTA := "user://verificacion_infinito.json"
const RUTA_MUNDO := "user://verificacion_infinito_mundo.json"

var _fallas := 0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))
	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://verificacion_infinito.rota.json"
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"

	# ANTES de iniciar(), no después. La primera versión de este test lo fijaba
	# más abajo, y para entonces `iniciar()` ya había leído el archivo del mundo
	# de VERDAD: los tests se contaminaban entre sí a través de él y este pasaba o
	# fallaba según cuál se hubiera corrido antes. Es la misma razón por la que la
	# ruta del save se fija acá arriba.
	Partida.ruta_mundo = RUTA_MUNDO

	print("\nPetBits — el mundo infinito, desde el juego\n")

	if not Partida.iniciar():
		print("No arrancó la partida.")
		get_tree().quit(1)
		return

	await _correr()

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	if _fallas == 0:
		print("\nTodo bien: el mundo se camina, da cosas y se acuerda de vos.")
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


func _correr() -> void:
	await get_tree().process_frame

	var mundo: Node2D = load("res://scenes/Mundo.tscn").instantiate()
	get_tree().root.add_child(mundo)
	await get_tree().process_frame
	await get_tree().process_frame

	var semilla: String = Partida.semilla_mundo
	_afirmar(semilla != "", "el mundo tiene semilla: %s" % semilla)

	# La semilla del mundo es el genoma de la PRIMERA criatura, no de la activa.
	# Si fuera la activa, incubar algo cambiaría el mundo entero debajo de tus
	# pies — y eso es exactamente lo que se prueba abajo.
	var primera: String = Partida.core.criaturas(Partida.ahora_ms())[0]["seed"]
	_afirmar(semilla == primera, "el mundo sale de la primera criatura")

	# ---- Los chunks se cargan y se liberan --------------------------------
	var cargados_al_empezar: int = mundo._chunks.size()
	_afirmar(cargados_al_empezar == 9, "arranca con el anillo de 3x3 (%d)" % cargados_al_empezar)

	# Se camina bien lejos, saltando de a un chunk. Sin liberar los viejos, el
	# tilemap crecería sin techo: una caminata larga dejaría decenas de miles de
	# celdas puestas que nadie va a mirar nunca más.
	var lado: int = Partida.core.lado_de_chunk()
	for i in range(1, 9):
		mundo._criatura.position = Vector2(i * lado * 16, 0)
		mundo._actualizar_chunks()
	_afirmar(
		mundo._chunks.size() == 9,
		"después de caminar ocho chunks sigue habiendo nueve (%d)" % mundo._chunks.size()
	)

	# Y volviendo, el mundo es el mismo: es la propiedad que el generador
	# garantiza, vista desde el juego.
	var lejos := Vector2i(4 * lado, 0)
	var antes: int = Partida.core.mundo_tile(semilla, lejos.x, lejos.y)
	mundo._criatura.position = Vector2.ZERO
	mundo._actualizar_chunks()
	var despues: int = Partida.core.mundo_tile(semilla, lejos.x, lejos.y)
	_afirmar(antes == despues, "el mundo no cambia por haberse ido y vuelto")

	# ---- Hay algo que levantar ---------------------------------------------
	#
	# Se busca en espiral desde el pueblo hasta encontrar cada tipo de hallazgo.
	# Si alguno no apareciera en un radio grande, sería una mecánica que existe
	# en el código y no en el juego.
	var encontrados := {}
	var primer_hito := Vector2i(0, 0)
	for radio in range(20, 200, 10):
		for a in range(0, 360, 7):
			var c := Vector2i(
				int(radio * cos(deg_to_rad(a))), int(radio * sin(deg_to_rad(a)))
			)
			var h: Dictionary = Partida.core.mundo_hallazgo(semilla, c.x, c.y)
			if h["tipo"] == "nada":
				continue
			if not encontrados.has(h["tipo"]):
				encontrados[h["tipo"]] = c
			if h["tipo"] == "hito" and primer_hito == Vector2i(0, 0):
				primer_hito = c

	for tipo in ["forraje", "veta", "hito"]:
		_afirmar(encontrados.has(tipo), "hay %s en el mundo cerca del pueblo" % tipo)

	# ---- Levantar forraje llega a la despensa -------------------------------
	if encontrados.has("forraje"):
		var c: Vector2i = encontrados["forraje"]
		var antes_baya: int = _cuanto("baya")
		mundo._criatura.position = Vector2((c.x + 0.5) * 16, (c.y + 0.5) * 16)
		mundo._recolectar()
		_afirmar(_cuanto("baya") == antes_baya + 1, "juntar pasto alto suma a la despensa")
		_afirmar(Partida.ya_recolectado(c), "y queda anotado que ahí ya no hay")

		# Dos veces no da dos: lo levantado se recuerda.
		var tras_una: int = _cuanto("baya")
		mundo._recolectar()
		_afirmar(_cuanto("baya") == tras_una, "no se puede levantar dos veces lo mismo")

	# ---- Un hito deja una semilla ------------------------------------------
	if primer_hito != Vector2i(0, 0):
		var antes_semillas: int = Partida.core.semillas().size()
		mundo._criatura.position = Vector2(
			(primer_hito.x + 0.5) * 16, (primer_hito.y + 0.5) * 16
		)
		mundo._recolectar()
		_afirmar(
			Partida.core.semillas().size() == antes_semillas + 1,
			"un hito deja una semilla para incubar"
		)

	# ---- El archivo del mundo ----------------------------------------------
	mundo._criatura.position = Vector2(1234, -567)
	Partida.donde = mundo._criatura.position
	Partida.guardar_mundo()
	Partida.guardar()

	_afirmar(FileAccess.file_exists(RUTA_MUNDO), "se escribió el archivo del mundo")

	# Y lo que de verdad importa: el save COMPARTIDO no tiene ni una palabra de
	# esto. Si alguna vez alguien agrega "x" e "y" ahí, funcionaría perfecto y
	# rompería la promesa de que los dos programas escriben lo mismo.
	var f := FileAccess.open(RUTA, FileAccess.READ)
	_afirmar(f != null, "el save compartido existe")
	if f != null:
		var texto := f.get_as_text()
		f.close()
		var datos = JSON.parse_string(texto)
		_afirmar(typeof(datos) == TYPE_DICTIONARY, "el save compartido es JSON")
		if typeof(datos) == TYPE_DICTIONARY:
			var esperadas := ["version", "guardadoMs", "criaturas", "activaId",
				"codex", "inventario", "semillas"]
			var sobran: Array = []
			for clave in datos.keys():
				if not esperadas.has(clave):
					sobran.append(clave)
			_afirmar(
				sobran.is_empty(),
				"el save compartido no tiene campos del nativo (sobran: %s)" % str(sobran)
			)

	mundo.queue_free()
	await get_tree().process_frame


func _cuanto(id: String) -> int:
	for a in Partida.core.alimentos():
		if a["id"] == id:
			return int(a["cantidad"])
	return 0


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		print("  ok   %s" % que)
	else:
		print("  FALLA %s" % que)
		_fallas += 1
