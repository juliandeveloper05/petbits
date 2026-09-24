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
	await _segundos(3.6)          # la primera página, tipeándose
	await _tecla(KEY_ENTER)
	await _tecla(KEY_ENTER)
	await _segundos(2.6)          # la segunda, a medias

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
	await _caminar_hasta("move_left", "Alguien del pueblo", 3.0)
	await _tecla(KEY_ENTER)
	await _segundos(3.2)          # el vecino, hablando

	# ---- 4. El mundo -------------------------------------------------------
	_tramo("mundo")
	while _caja_abierta():
		await _tecla(KEY_ESCAPE)
		await _segundos(0.1)
	await _caminar("move_right", 9.0)   # por la calle, y unos pasos afuera del pueblo

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
