## demo.gd
##
## Treinta segundos de juego, para grabar.
##
##   godot --path godot res://scenes/Demo.tscn \
##         --write-movie godot/demo.avi --fixed-fps 30 --disable-vsync
##
## ---
##
## POR QUÉ ES UN ARNÉS Y NO ALGUIEN JUGANDO.
##
## `--write-movie` fuerza `--fixed-fps`: el tiempo del juego avanza a pasos fijos
## sin importar cuánto tarde de verdad cada cuadro. Así la grabación dura
## exactamente los cuadros que se le piden, sale igual en cualquier máquina, y no
## hay que capturar la pantalla con un programa aparte ni recortar después.
##
## Lo que se pierde es la improvisación. Lo que se gana es que el mismo comando
## produce el mismo video, y que cuando el mundo cambie —como cambió hoy, cuando
## el mineral pasó a existir cerca del pueblo— se regenera y ya.
##
## ---
##
## SE CAMINA DE VERDAD.
##
## Con `Input.action_press`, no moviendo el sprite a mano. Dos motivos: uno, que
## lo que se quiere mostrar es el juego y no una animación del juego; y dos, que
## esto ejercita el único camino que ningún arnés tocaba —la rama de `_process`
## que camina y todo `_unhandled_input`—, que es justamente por dónde se coló el
## bug de `Partida.donde`.
##
## ---
##
## Y SE CHOREOGRAFÍA POR EVENTOS, NO POR TIEMPO.
##
## Caminar N segundos ya falló una vez: 3,4 s a 46 px/s son 156 px y del ingreso
## a los pedestales hay 128, así que la criatura se pasaba de largo y la caja de
## diálogo no salía. Acá se camina HASTA llegar, con un tope por las dudas.

extends Node

const Historia = preload("res://scripts/Historia.gd")

const RUTA := "user://demo.json"
const RUTA_MUNDO := "user://demo_mundo.json"

## A 30 fps. El total sale de sumar los tramos; se informa al final.
const FPS := 30.0

var _cuadros := 0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	# Los seams van ANTES de iniciar(), siempre. Un video no vale la partida de
	# nadie.
	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://demo.rota.json"
	Partida.ruta_mundo = RUTA_MUNDO
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"

	if not Partida.iniciar():
		print("No arrancó la partida.")
		get_tree().quit(1)
		return

	print("\nPetBits — treinta segundos\n")
	await _correr()

	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	print("\n%d cuadros = %.1f s a %d fps" % [_cuadros, _cuadros / FPS, int(FPS)])
	get_tree().quit(0)


func _correr() -> void:
	await get_tree().process_frame

	var mundo: Node2D = load("res://scenes/Mundo.tscn").instantiate()
	get_tree().root.add_child(mundo)
	await _esperar(2)

	# ---- 1. La plaza -------------------------------------------------------
	#
	# Quieta un momento antes de moverse. Un video que arranca con la cámara ya
	# en movimiento no deja leer nada.
	await _esperar(45)  # 1,5 s

	# ---- 2. El vecino ------------------------------------------------------
	#
	# Está en el tile (-3, 1) y la criatura arranca en el (0, 0): tres al oeste y
	# uno al sur. Es la única conversación de varias páginas del juego.
	await _caminar_hasta(mundo, "move_left", Historia.VECINO_NOMBRE, 4.0)
	await _esperar(10)
	mundo._usar()
	await _esperar(85)      # que tipee la primera página
	mundo._usar()           # pasar de página
	await _esperar(75)
	mundo._usar()
	await _esperar(20)
	if mundo._caja.abierta():
		mundo._caja.cerrar()
	await _esperar(10)

	# ---- 3. Salir al mundo -------------------------------------------------
	#
	# Al este, cruzando el hueco del borde de árboles. El cartel de abajo va
	# cambiando de bioma solo.
	await _caminar(mundo, "move_right", 7.0)

	# ---- 4. Campo abierto y recolectar -------------------------------------
	await _caminar(mundo, "move_down", 2.0)
	await _esperar(15)
	mundo._usar()           # levanta lo que haya en el suelo, si hay
	await _esperar(60)

	# ---- 5. El criadero ----------------------------------------------------
	#
	# Corte de plano: caminar de vuelta serían siete segundos de nada. Es lenguaje
	# de trailer, no una mentira sobre el juego.
	mundo._criatura.position = Vector2(-11.5 * 16, 5.5 * 16)
	mundo._mirar_alrededor()
	mundo._ubicar_camara(true)
	await _esperar(45)
	mundo._usar()           # entra, con el fundido
	await _esperar(45)

	# ---- 6. Adentro, los pedestales ----------------------------------------
	await _caminar_hasta(mundo, "move_up", "Los pedestales", 6.0)
	await _esperar(10)
	mundo._usar()
	await _esperar(110)

	# ---- 7. Quieta, para cerrar --------------------------------------------
	await _esperar(40)

	mundo.queue_free()
	await get_tree().process_frame


## Espera N cuadros. Con `--fixed-fps` esto es tiempo exacto.
func _esperar(cuadros: int) -> void:
	for i in cuadros:
		await get_tree().process_frame
		_cuadros += 1


## Camina apretando la tecla, N segundos de tiempo de juego.
func _caminar(mundo: Node2D, accion: String, segundos: float) -> void:
	Input.action_press(accion)
	for i in int(segundos * FPS):
		await get_tree().process_frame
		_cuadros += 1
	Input.action_release(accion)


## Camina hasta quedar al lado de un punto, con un tope por las dudas.
func _caminar_hasta(mundo: Node2D, accion: String, nombre: String, tope: float) -> void:
	Input.action_press(accion)
	var restantes := int(tope * FPS)
	while restantes > 0:
		await get_tree().process_frame
		_cuadros += 1
		restantes -= 1
		if mundo._cerca.get("nombre", "") == nombre:
			break
	Input.action_release(accion)
	if mundo._cerca.get("nombre", "") != nombre:
		print("  OJO: no se llegó a %s" % nombre)
