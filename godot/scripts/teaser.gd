## teaser.gd — la coreografía del teaser para historias.
##
##   pwsh tools/armar_teaser.ps1
##
## Juega la intro del juego como un jugador nuevo —créditos, título, partida
## nueva, la intro, cuidarla, el pueblo, el vecino, salir al mundo— para grabarla
## con `--write-movie`. Los subtítulos no van acá: los pone el armado, encima.
##
## ---
##
## ANOTA CUÁNDO EMPIEZA CADA TRAMO.
##
## Escribe `godot/teaser/tiempos.json` con el segundo exacto en que arranca cada
## paso. El armado lee ese archivo para decidir cuándo cambia cada subtítulo. Si
## mañana la intro tiene una página más, el subtítulo de "cuidala" se corre solo
## y no queda encima de la pantalla equivocada.
##
## El tiempo se cuenta en el reloj del JUEGO, que con `--fixed-fps 30` avanza
## exactamente un treintavo de segundo por cuadro: es el mismo reloj que el del
## video, cuadro por cuadro.
##
## ---
##
## LE CEDE SU LUGAR A LA PANTALLA QUE GRABA.
##
## Igual que el arnés del recorrido: los botones cambian de pantalla con
## `change_scene_to_file`, que reemplaza la escena actual. El teaser cuelga cada
## pantalla de la raíz, la nombra escena actual, y se queda al costado.

extends Node

const Historia = preload("res://scripts/Historia.gd")
const CajaDialogo = preload("res://scripts/CajaDialogo.gd")

const RUTA := "user://teaser.json"
const RUTA_MUNDO := "user://teaser_mundo.json"
const SALIDA := "res://teaser/tiempos.json"

var _t := 0.0
var _tramos := {}


func _ready() -> void:
	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	# Los seams ANTES de todo: un video no vale la partida de nadie.
	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://teaser.rota.json"
	Partida.ruta_mundo = RUTA_MUNDO
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"
	# Se graba desde el editor, que es debug: sin esto el video anunciaría la
	# tecla del tester.
	Partida.version_de_prueba = false

	set_process(true)
	await _correr()

	_tramos["fin"] = _t
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path("res://teaser"))
	var f := FileAccess.open(SALIDA, FileAccess.WRITE)
	f.store_string(JSON.stringify(_tramos, "  "))
	f.close()

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))
	print("\nteaser: %.1f s · tramos %s" % [_t, str(_tramos)])
	get_tree().quit(0)


func _process(delta: float) -> void:
	_t += delta


func _correr() -> void:
	await get_tree().process_frame

	# ---- 0. Créditos y título ----------------------------------------------
	_tramo("titulo")
	var inicio: Node = load("res://scenes/Inicio.tscn").instantiate()
	get_tree().root.add_child(inicio)
	get_tree().current_scene = inicio
	await _segundos(2.4)          # el primer cartel, entero
	await _tecla(KEY_ENTER)       # se saltea el segundo
	await _segundos(2.2)          # el título, con sus tres criaturas

	# ---- 1. Partida nueva: la intro ----------------------------------------
	_tramo("semilla")
	var nueva := _buscar_boton(get_tree().current_scene, "Nueva partida")
	nueva.grab_focus()
	await _segundos(0.3)
	await _tecla(KEY_ENTER)
	await _pantalla("PetView")
	# La primera página entera, y un respiro para leerla: es la línea de la
	# señora, la que cuenta la historia. Se calcula del texto porque con un
	# tiempo fijo el video la cortaba a la mitad en cuanto la página creció.
	await _segundos(Historia.INTRO[0].length() / CajaDialogo.VELOCIDAD + 1.2)
	await _tecla(KEY_ENTER)       # pasa a la segunda
	await _tecla(KEY_ENTER)       # y la muestra entera: la de la semilla
	await _segundos(2.6)

	# ---- 2. Cuidarla -------------------------------------------------------
	_tramo("cuidala")
	await _tecla(KEY_ESCAPE)      # se saltea el resto de la intro
	await _segundos(0.8)
	await _tecla(KEY_ENTER)       # le da de comer: el foco está en la baya
	await _segundos(2.2)
	if _caja_abierta():
		await _tecla(KEY_ESCAPE)
	await _segundos(0.6)
	var jugar := _buscar_boton(get_tree().current_scene, "Jugar")
	jugar.grab_focus()
	await _segundos(0.3)
	await _tecla(KEY_ENTER)
	await _segundos(1.4)

	# ---- 3. Salir al pueblo ------------------------------------------------
	_tramo("pueblo")
	var pueblo := _buscar_boton(get_tree().current_scene, "Pueblo")
	pueblo.grab_focus()
	await _segundos(0.3)
	await _tecla(KEY_ENTER)
	await _pantalla("Mundo")
	await _segundos(1.8)
	while _caja_abierta():
		await _tecla(KEY_ENTER)
		await _segundos(0.2)
	await _caminar_hasta("move_left", Historia.VECINO_NOMBRE, 3.0)
	await _tecla(KEY_ENTER)
	# La primera página de Doña Cuenta, entera: con un número fijo el video la
	# cortaba a mitad de palabra en cuanto la frase creció.
	await _segundos(Historia.VECINO[0].length() / CajaDialogo.VELOCIDAD + 1.0)

	# ---- 4. El mundo: un círculo de piedras --------------------------------
	#
	# Un CORTE, como en cualquier tráiler. El círculo más cercano al pueblo que
	# se alcanza caminando está a unos cuarenta pasos —trece segundos de video—,
	# así que el teaser salta a los últimos pasos del camino de verdad: el que
	# encuentra una búsqueda en anchura por celdas libres. Nada que el jugador no
	# pueda hacer; solo se saltea la caminata.
	_tramo("mundo")
	while _caja_abierta():
		await _tecla(KEY_ESCAPE)
		await _segundos(0.1)
	var mundo := get_tree().current_scene
	var ruta := _ruta_al_hito(mundo, 90)
	if ruta.size() >= 2:
		var desde: int = maxi(0, ruta.size() - 7)
		var arranque: Vector2i = ruta[desde]
		mundo._criatura.position = Vector2((arranque.x + 0.5) * 16, (arranque.y + 0.5) * 16)
		Partida.donde = mundo._criatura.position
		mundo._actualizar_chunks()
		mundo._ubicar_camara(true)
		await _segundos(0.8)
		await _seguir(mundo, ruta.slice(desde + 1))
		await _segundos(1.2)      # el círculo, con su semilla titilando y su cartel
		await _tecla(KEY_ENTER)   # la junta
		await _segundos(2.6)
	else:
		print("  OJO: no hubo un círculo a mano; sigue la caminata")
		await _caminar("move_right", 6.0)

	# ---- 5. El cierre ------------------------------------------------------
	_tramo("cierre")
	await _segundos(3.2)


# ---------------------------------------------------------------------------

func _tramo(nombre: String) -> void:
	_tramos[nombre] = _t


func _tecla(codigo: Key) -> void:
	for apretada in [true, false]:
		var ev := InputEventKey.new()
		ev.keycode = codigo
		ev.physical_keycode = codigo
		ev.pressed = apretada
		Input.parse_input_event(ev)
		await get_tree().process_frame


func _caminar(accion: String, segundos: float) -> void:
	Input.action_press(accion)
	await _segundos(segundos)
	Input.action_release(accion)


func _caminar_hasta(accion: String, nombre: String, tope: float) -> void:
	Input.action_press(accion)
	var limite := _t + tope
	while _t < limite:
		await get_tree().process_frame
		var m := get_tree().current_scene
		if m != null and "_cerca" in m and m._cerca.get("nombre", "") == nombre:
			break
	Input.action_release(accion)
	await _segundos(0.3)


func _celda(mundo: Node) -> Vector2i:
	var p: Vector2 = mundo._criatura.position
	return Vector2i(floori(p.x / 16.0), floori(p.y / 16.0))


## El camino más corto, celda por celda, hasta el círculo de piedras más cercano.
##
## En anchura por las celdas que no son sólidas: un camino "en L" no alcanza,
## porque la criatura choca con una caja de diez píxeles y un árbol en la fila de
## al lado la frena. Siguiendo centros de celdas libres vecinas, nunca roza nada.
func _ruta_al_hito(mundo: Node, maximo: int) -> Array:
	var semilla: String = Partida.semilla_mundo
	var lado: int = Partida.core.lado_de_chunk()
	var inicio := _celda(mundo)
	var hitos := {}
	var centro := Vector2i(floori(float(inicio.x) / lado), floori(float(inicio.y) / lado))
	for cy in range(centro.y - 2, centro.y + 3):
		for cx in range(centro.x - 2, centro.x + 3):
			for i in Partida.core.mundo_hitos_chunk(semilla, cx, cy):
				var x: int = i % lado
				@warning_ignore("integer_division")
				var y: int = i / lado
				hitos[Vector2i(cx * lado + x, cy * lado + y)] = true

	var previo := {inicio: inicio}
	var pasos := {inicio: 0}
	var cola: Array[Vector2i] = [inicio]
	while not cola.is_empty():
		var c: Vector2i = cola.pop_front()
		if hitos.has(c):
			var ruta: Array = [c]
			while ruta[0] != inicio:
				ruta.push_front(previo[ruta[0]])
			return ruta
		if pasos[c] >= maximo:
			continue
		for d in [Vector2i.RIGHT, Vector2i.LEFT, Vector2i.DOWN, Vector2i.UP]:
			var n: Vector2i = c + d
			if pasos.has(n) or mundo._solido_en(n):
				continue
			pasos[n] = pasos[c] + 1
			previo[n] = c
			cola.append(n)
	return []


## Camina de centro en centro por una ruta de celdas vecinas.
func _seguir(mundo: Node, ruta: Array) -> void:
	for celda in ruta:
		var meta := Vector2((celda.x + 0.5) * 16, (celda.y + 0.5) * 16)
		var quieto := 0
		while true:
			var p: Vector2 = mundo._criatura.position
			var falta := meta - p
			if absf(falta.x) < 1.6 and absf(falta.y) < 1.6:
				break
			var accion: String
			if absf(falta.x) >= 1.6:
				accion = "move_right" if falta.x > 0 else "move_left"
			else:
				accion = "move_down" if falta.y > 0 else "move_up"
			Input.action_press(accion)
			await get_tree().process_frame
			Input.action_release(accion)
			quieto = quieto + 1 if mundo._criatura.position == p else 0
			if quieto > 15:
				print("  OJO: la ruta se trabó en ", celda)
				return


func _pantalla(nombre: String) -> void:
	for i in 120:
		await get_tree().process_frame
		var actual := get_tree().current_scene
		if actual != null and actual.name == nombre:
			await get_tree().process_frame
			return
	print("  OJO: no se llegó a %s" % nombre)


func _caja_abierta() -> bool:
	var actual := get_tree().current_scene
	if actual == null or not ("_caja" in actual) or actual._caja == null:
		return false
	return actual._caja.abierta()


func _buscar_boton(nodo: Node, texto: String) -> Button:
	if nodo is Button and nodo.text.begins_with(texto):
		return nodo
	for hijo in nodo.get_children():
		var b := _buscar_boton(hijo, texto)
		if b != null:
			return b
	return null


## Espera en el reloj del juego, que con `--fixed-fps` es el reloj del video.
func _segundos(s: float) -> void:
	var hasta := _t + s
	while _t < hasta:
		await get_tree().process_frame
