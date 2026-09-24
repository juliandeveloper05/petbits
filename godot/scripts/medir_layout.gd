## medir_layout.gd
##
## Mide si PetView entra en el viewport, en vez de mirarlo a ojo.
##
##   godot --headless --path godot res://scenes/MedirLayout.tscn
##
## Existe porque la primera versión de esa pantalla apilaba todo en una columna
## y pedía unos 400 píxeles de alto sobre un viewport de 270: Salud y Vínculo
## quedaban abajo del borde. Godot no avisa de eso —simplemente recorta— así que
## solo se ve mirando la ventana, o midiendo.
##
## ---
##
## POR QUÉ ES UNA ESCENA Y NO UN `--script`.
##
## Lo era, hasta que PetView pasó a colgarse del autoload `Partida`. Cuando se
## arranca Godot con `--script`, ese script REEMPLAZA el main loop y los
## autoloads no llegan a instanciarse: ni siquiera compila, porque `Partida` no
## existe todavía cuando el script se carga.
##
## Como escena principal, en cambio, arranca igual que el juego de verdad — que
## es justamente lo que una herramienta de medición tiene que estar midiendo.

extends Node

const RUTA := "user://medicion.json"
const RUTA_MUNDO := "user://medicion_mundo.json"


func _ready() -> void:
	var vp := Vector2(
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height")
	)
	print("\nViewport del proyecto: %d x %d" % [vp.x, vp.y])

	# La pantalla real necesita una partida real, pero medir no es jugar: la
	# partida de prueba va a un archivo aparte para no despertarle la criatura a
	# quien esté corriendo esto.
	#
	# LOS CUATRO, NO DOS. Acá se desviaba el save y la cuarentena, pero no el
	# archivo del mundo ni el guardado al salir. PetView llama a `iniciar()`, y
	# al cerrar `_notification` escribía `user://mundo.json` — el DE VERDAD, el
	# de quien estuviera corriendo la suite. Pasó en cada corrida durante
	# semanas: la posición y lo levantado del suelo se reseteaban solos. Además,
	# `guardar()` volvía a crear `medicion.json` después de que la línea del
	# final lo borraba, y quedaba tirado en la carpeta.
	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://medicion.rota.json"
	Partida.ruta_mundo = RUTA_MUNDO

	# Un cuadro antes de colgar nada: durante `_ready()` la raíz todavía está
	# armando sus hijos y `add_child` falla con un error en consola en vez de una
	# excepción, así que la medición seguiría —y daría bien— sobre una pantalla
	# que nunca entró al árbol.
	await get_tree().process_frame

	var escena: PackedScene = load("res://scenes/PetView.tscn")
	var raiz: Control = escena.instantiate()
	get_tree().root.add_child(raiz)

	# No se le fija el tamaño a mano: la escena tiene anclas al rectángulo
	# completo, así que Godot se lo asigna solo y hacerlo por fuera solo saca un
	# warning. Lo que se mide es el mínimo que PIDE el contenido, que no depende
	# del tamaño que tenga puesto.

	# Dos cuadros para que los contenedores resuelvan su tamaño.
	await get_tree().process_frame
	await get_tree().process_frame

	var fallas := 0
	for hijo in raiz.get_children():
		if not (hijo is VBoxContainer):
			continue
		var pedido: float = hijo.get_combined_minimum_size().y
		var ancho: float = hijo.get_combined_minimum_size().x
		print("Alto mínimo que pide el contenido: %.0f px" % pedido)
		print("Alto disponible:                   %.0f px" % vp.y)
		print("Ancho mínimo que pide:             %.0f px" % ancho)
		if pedido > vp.y:
			print("\nFALLA: el contenido se pasa por %.0f px. Se va a recortar." % (pedido - vp.y))
			fallas += 1
		elif ancho > vp.x:
			print("\nFALLA: el contenido se pasa a lo ancho por %.0f px." % (ancho - vp.x))
			fallas += 1
		else:
			print("\nEntra, con %.0f px de aire abajo." % (vp.y - pedido))

	# Se borran las CONSTANTES de este archivo, nunca `Partida.ruta_*`. Si alguien
	# le saca el desvío de arriba, `Partida.ruta_save` pasa a ser la partida de
	# verdad y esta línea la borraría — la red de `Partida` frena las escrituras
	# sin ventana, pero un borrado directo no pasa por ella. Así lo verificó el
	# arreglo de este archivo: sacándole el desvío a propósito, esta limpieza
	# borró el mundo.json real.
	for r in [RUTA, RUTA_MUNDO]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))
	get_tree().quit(fallas)
