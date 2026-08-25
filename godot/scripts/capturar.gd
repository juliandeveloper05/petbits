## capturar.gd
##
## Corre el juego DE VERDAD —con ventana— y saca capturas mientras camina.
##
##   godot --path godot res://scenes/Capturar.tscn
##   → godot/captura_*.png
##
## ---
##
## POR QUÉ CON VENTANA Y NO HEADLESS.
##
## Godot sin ventana no dibuja: pedirle una captura devuelve negro. Por eso todas
## las herramientas de este proyecto componen sus PNG a mano, tile por tile, y por
## eso hay cosas que ninguna puede contestar — cómo se siente caminar, si el
## fundido molesta, si al cruzar un borde de chunk se nota un tirón.
##
## Con ventana sí dibuja, y entonces `get_viewport().get_texture()` devuelve
## exactamente lo que se ve. Es la diferencia entre verificar que el juego
## funciona y verificar que se juega.
##
## ---
##
## Y MIDE, QUE ES LA MITAD DEL PUNTO.
##
## Cargar un chunk son varios miles de `set_cell` repartidos en seis capas. En el
## plan escribí que la mitigación era pintar solo donde el material aparece, y
## nunca lo cronometré: un tirón de doscientos milisegundos al cruzar un borde no
## lo ve ningún test headless, y se siente en el primer minuto de juego.
##
## Se registra el tiempo de CADA cuadro mientras camina, y se informa el peor.

extends Node

const RUTA := "user://captura.json"
const RUTA_MUNDO := "user://captura_mundo.json"

## Cuánto camina en cada tramo, en segundos.
const CAMINATA := 2.5

var _peor_cuadro := 0.0
var _cuando_el_peor := ""
var _cuadros := 0
var _suma := 0.0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://captura.rota.json"
	Partida.ruta_mundo = RUTA_MUNDO
	# Semilla fija: dos corridas tienen que dar las mismas capturas, o un diff
	# visual no significaría nada.
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"

	print("\nPetBits — el juego corriendo de verdad\n")

	if not Partida.iniciar():
		print("No arrancó la partida.")
		get_tree().quit(1)
		return

	await _correr()

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	print("\n--- cuadros ---")
	print("  %d cuadros, promedio %.1f ms" % [_cuadros, (_suma / maxf(_cuadros, 1)) * 1000.0])
	print("  el peor: %.1f ms (%s)" % [_peor_cuadro * 1000.0, _cuando_el_peor])
	if _peor_cuadro > 0.100:
		print("  OJO: un cuadro de más de 100 ms se ve como un tirón.")

	get_tree().quit(0)


func _correr() -> void:
	await get_tree().process_frame

	var mundo: Node2D = load("res://scenes/Mundo.tscn").instantiate()
	get_tree().root.add_child(mundo)
	await get_tree().process_frame
	await get_tree().process_frame

	await _capturar(mundo, "01_pueblo", "la plaza, al arrancar")

	# ---- Caminar hacia el este, cruzando el borde del pueblo --------------
	#
	# Se usa el sistema de entrada de verdad y no se mueve el sprite a mano: lo
	# que se quiere medir es el juego, y mover el sprite salteando `_process`
	# mediría otra cosa.
	await _caminar(mundo, "move_right", CAMINATA, "saliendo del pueblo al este")
	await _capturar(mundo, "02_saliendo", "cruzando el borde del pueblo")

	await _caminar(mundo, "move_right", CAMINATA * 2.0, "campo abierto")
	await _capturar(mundo, "03_campo", "lejos del pueblo")

	await _caminar(mundo, "move_down", CAMINATA * 2.0, "hacia el sur")
	await _capturar(mundo, "04_sur", "más lejos todavía")

	# ---- El criadero, para ver el fundido y un interior -------------------
	#
	# La cámara va suavizada, así que mover la criatura de prepo y esperar un
	# cuadro fotografía el lugar ANTERIOR: la primera versión de esto sacó una
	# foto del campo abierto con el cartel del criadero encima, y la foto era
	# honesta — la cámara todavía estaba allá. Hay que pedirle que salte.
	mundo._criatura.position = Vector2(-11.5 * 16, 5.5 * 16)
	mundo._mirar_alrededor()
	mundo._ubicar_camara(true)
	await get_tree().process_frame
	await _capturar(mundo, "05_puerta", "parada en la puerta del criadero")

	mundo._usar()
	# El fundido dura FUNDIDO segundos de ida y otros tantos de vuelta.
	for i in 40:
		await get_tree().process_frame
	await _capturar(mundo, "06_criadero", "adentro del criadero")

	# ---- La caja de diálogo -----------------------------------------------
	#
	# Que es lo único que no se puede mirar en un PNG compuesto a mano: el tipeo
	# y el triangulito son cosas del tiempo.
	#
	# Hay que CAMINAR hasta los pedestales. `_usar()` no hace nada si no tiene un
	# punto cerca, y se entra por la puerta, a ocho tiles del mostrador: la
	# primera versión llamaba a `_usar()` ahí nomás y sacaba dos capturas
	# idénticas, que es exactamente el modo de falla que este arnés existe para
	# encontrar — y lo encontró en sí mismo.
	await _caminar_hasta(mundo, "move_up", "Los pedestales")

	mundo._usar()
	for i in 25:
		await get_tree().process_frame
	await _capturar(mundo, "07_dialogo", "la caja a mitad de escribir")

	# Y terminada de escribir, que es cuando aparece el triangulito de "seguí".
	for i in 200:
		await get_tree().process_frame
	await _capturar(mundo, "08_triangulito", "la caja entera, con el triangulito")

	mundo.queue_free()
	await get_tree().process_frame


## Camina hasta quedar al lado de un punto, y frena ahi.
##
## Caminar por tiempo es fragil: 3,4 s a 46 px/s son 156 px, y del ingreso a los
## pedestales hay 128. La criatura se pasaba de largo dos tiles y `_usar()` no
## encontraba nada. Con el tope de seguridad, si el punto no aparece nunca el
## arnes lo dice en vez de colgarse.
func _caminar_hasta(mundo: Node2D, accion: String, nombre: String) -> void:
	Input.action_press(accion)
	var tope := 8.0
	while tope > 0.0:
		var t0 := Time.get_ticks_usec()
		await get_tree().process_frame
		var dt := (Time.get_ticks_usec() - t0) / 1000000.0
		tope -= dt
		_cuadros += 1
		_suma += dt
		if dt > _peor_cuadro:
			_peor_cuadro = dt
			_cuando_el_peor = "caminando hasta %s" % nombre
		if mundo._cerca.get("nombre", "") == nombre:
			break
	Input.action_release(accion)

	if mundo._cerca.get("nombre", "") != nombre:
		print("  OJO: no se llego a %s en 8 s." % nombre)
	else:
		print("  llego a %s" % nombre)


## Camina de verdad, apretando la tecla, y cronometra cada cuadro.
func _caminar(mundo: Node2D, accion: String, segundos: float, que: String) -> void:
	Input.action_press(accion)

	var restante := segundos
	while restante > 0.0:
		var t0 := Time.get_ticks_usec()
		await get_tree().process_frame
		var dt := (Time.get_ticks_usec() - t0) / 1000000.0

		restante -= dt
		_cuadros += 1
		_suma += dt
		if dt > _peor_cuadro:
			_peor_cuadro = dt
			_cuando_el_peor = que

	Input.action_release(accion)

	var celda := Vector2i(
		int(floor(mundo._criatura.position.x / 16)), int(floor(mundo._criatura.position.y / 16))
	)
	print("  %s → quedó en (%d, %d), bioma %s" % [
		que, celda.x, celda.y,
		Partida.core.mundo_bioma(Partida.semilla_mundo, celda.x, celda.y)
	])


## Guarda lo que se está viendo.
##
## `frame_post_draw` es la espera que importa: sin ella se captura el buffer del
## cuadro anterior, y las capturas salen corridas un cuadro respecto de lo que
## dice el mensaje.
func _capturar(mundo: Node2D, nombre: String, que: String) -> void:
	await RenderingServer.frame_post_draw
	var imagen := get_viewport().get_texture().get_image()
	var ruta := "res://captura_%s.png" % nombre
	imagen.save_png(ruta)

	# El estado, al lado del nombre. Una captura que sale bien porque el juego
	# estaba en otro lado que el que dice el mensaje es peor que ninguna.
	var p: Vector2 = mundo._criatura.position
	var cerca := "nada"
	if not mundo._cerca.is_empty():
		cerca = str(mundo._cerca["nombre"])
	print("  %s — %s\n      criatura en (%d, %d) · cerca: %s · caja: %s" % [
		nombre, que,
		int(floor(p.x / 16)), int(floor(p.y / 16)),
		cerca,
		"abierta" if mundo._caja.abierta() else "cerrada",
	])
