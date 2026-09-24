## capturar_recorrido.gd
##
## Recorre el juego como lo haría el tester, CON VENTANA, y saca una captura en
## cada paso: créditos, título, partida nueva, la intro, la ficha, el pueblo...
##
##   godot --path godot res://scenes/CapturarRecorrido.tscn
##   → godot/recorrido_*.png
##
## ---
##
## EL ARNÉS LE CEDE SU LUGAR A LA PANTALLA QUE PRUEBA.
##
## El menú del título cambia de pantalla con `change_scene_to_file`, que reemplaza
## la escena actual. Si el arnés fuera la escena actual, el primer botón lo
## borraría a él y el recorrido terminaría ahí. Así que cuelga la pantalla de la
## raíz, la nombra escena actual, y se queda al costado mirando: cuando el menú
## cambia de pantalla, se reemplaza la pantalla y el arnés sigue vivo.
##
## ---
##
## SE APRIETAN TECLAS DE VERDAD.
##
## Con `Input.parse_input_event`, que es lo mismo que le llega a Godot cuando
## alguien toca el teclado. Llamar a los métodos de las pantallas mediría otra
## cosa: por ahí se coló que Esc no hacía nada en todo el juego.

extends Node

const RUTA := "user://recorrido.json"
const RUTA_MUNDO := "user://recorrido_mundo.json"

var _n := 0


func _ready() -> void:
	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://recorrido.rota.json"
	Partida.ruta_mundo = RUTA_MUNDO
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"

	print("\nPetBits — el recorrido del tester\n")
	await _correr()

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))
	print("\n%d capturas" % _n)
	get_tree().quit(0)


func _correr() -> void:
	await get_tree().process_frame

	# --- El título, cediéndole el lugar de escena actual ---
	var inicio: Node = load("res://scenes/Inicio.tscn").instantiate()
	get_tree().root.add_child(inicio)
	get_tree().current_scene = inicio

	await _segundos(1.2)
	await _captura("creditos", "el primer cartel")

	await _tecla(KEY_ENTER)
	await _segundos(0.6)
	await _captura("titulo", "el menú, después de saltear los créditos")

	# --- Nueva partida ---
	var boton := _buscar_boton(get_tree().current_scene, "Nueva partida")
	if boton == null:
		print("  OJO: no está el botón Nueva partida")
		return
	boton.grab_focus()
	await _cuadros(2)
	await _tecla(KEY_ENTER)
	await _pantalla("PetView")
	await _segundos(1.8)
	await _captura("intro", "la intro de una partida nueva, tipeando")

	# Pasar todas las páginas.
	for i in 12:
		if not _caja_abierta():
			break
		await _tecla(KEY_ENTER)
		await _segundos(0.15)
		await _tecla(KEY_ENTER)
		await _segundos(0.15)
	await _segundos(0.4)
	await _captura("ficha", "la ficha, con el primer objetivo")

	# --- Darle de comer, con el teclado ---
	await _tecla(KEY_ENTER)
	await _segundos(1.4)
	await _captura("comio", "el objetivo cumplido")
	if _caja_abierta():
		await _tecla(KEY_ESCAPE)
		await _segundos(0.3)
	await _captura("siguiente", "el objetivo siguiente")

	# --- Al pueblo, con el botón ---
	var pueblo := _buscar_boton(get_tree().current_scene, "Pueblo")
	pueblo.grab_focus()
	await _cuadros(2)
	await _tecla(KEY_ENTER)
	await _pantalla("Mundo")
	await _segundos(1.6)
	await _captura("pueblo", "el pueblo: objetivo arriba, controles abajo")
	while _caja_abierta():
		await _tecla(KEY_ENTER)
		await _segundos(0.2)
	await _segundos(0.3)
	await _captura("pueblo_ayuda", "la ayuda de controles, visible")

	# --- Caminar hasta el vecino, con las flechas, y hablarle ---
	await _caminar_hasta("move_left", "Alguien del pueblo", 3.0)
	await _captura("vecino_cerca", "parada al lado del vecino")
	await _tecla(KEY_ENTER)
	await _segundos(2.2)
	await _captura("vecino", "el vecino hablando")
	while _caja_abierta():
		await _tecla(KEY_ENTER)
		await _segundos(0.15)
	await _segundos(1.2)
	await _captura("despues_vecino", "el objetivo, después de hablar")
	while _caja_abierta():
		await _tecla(KEY_ENTER)
		await _segundos(0.15)

	# --- Esc vuelve a la ficha, y otra vez Esc al título ---
	await _tecla(KEY_ESCAPE)
	await _pantalla("PetView")
	await _segundos(0.4)
	await _tecla(KEY_ESCAPE)
	await _pantalla("Inicio")
	await _segundos(0.5)
	await _captura("volver_titulo", "Esc, Esc: el título, sin créditos")


# ---------------------------------------------------------------------------

func _tecla(codigo: Key) -> void:
	for apretada in [true, false]:
		var ev := InputEventKey.new()
		ev.keycode = codigo
		ev.physical_keycode = codigo
		ev.pressed = apretada
		Input.parse_input_event(ev)
		await get_tree().process_frame


## Camina apretando la acción hasta quedar al lado de un punto del mapa.
func _caminar_hasta(accion: String, nombre: String, tope: float) -> void:
	Input.action_press(accion)
	var hasta := Time.get_ticks_msec() + int(tope * 1000)
	while Time.get_ticks_msec() < hasta:
		await get_tree().process_frame
		var m := get_tree().current_scene
		if m != null and "_cerca" in m and m._cerca.get("nombre", "") == nombre:
			break
	Input.action_release(accion)
	await _cuadros(4)


func _pantalla(nombre: String) -> void:
	for i in 120:
		await get_tree().process_frame
		var actual := get_tree().current_scene
		if actual != null and actual.name == nombre:
			await _cuadros(3)
			return
	print("  OJO: no se llegó a %s" % nombre)


func _caja_abierta() -> bool:
	var actual := get_tree().current_scene
	if actual == null or not ("_caja" in actual) or actual._caja == null:
		return false
	return actual._caja.abierta()


func _buscar_boton(nodo: Node, texto: String) -> Button:
	if nodo is Button and nodo.text == texto:
		return nodo
	for hijo in nodo.get_children():
		var b := _buscar_boton(hijo, texto)
		if b != null:
			return b
	return null


func _segundos(s: float) -> void:
	await get_tree().create_timer(s).timeout


func _cuadros(n: int) -> void:
	for i in n:
		await get_tree().process_frame


func _captura(nombre: String, que: String) -> void:
	await RenderingServer.frame_post_draw
	_n += 1
	var ruta := "res://recorrido_%02d_%s.png" % [_n, nombre]
	get_viewport().get_texture().get_image().save_png(ruta)
	var escena := get_tree().current_scene
	print("  %02d %-16s %s  [%s]" % [_n, nombre, que, escena.name if escena else "?"])
