## medir_layout.gd
##
## Mide si PetView entra en el viewport, en vez de mirarlo a ojo.
##
##   godot --headless --path godot --script res://scripts/medir_layout.gd
##
## Existe porque la primera versión de esa pantalla apilaba todo en una columna
## y pedía unos 400 píxeles de alto sobre un viewport de 270: Salud y Vínculo
## quedaban abajo del borde. Godot no avisa de eso —simplemente recorta— así que
## solo se ve mirando la ventana, o midiendo.

extends SceneTree


func _init() -> void:
	var vp := Vector2(
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height")
	)
	print("\nViewport del proyecto: %d x %d" % [vp.x, vp.y])

	var escena: PackedScene = load("res://scenes/PetView.tscn")
	var raiz: Control = escena.instantiate()
	root.add_child(raiz)

	# No se le fija el tamaño a mano: la escena tiene anclas al rectángulo
	# completo, así que Godot se lo asigna solo y hacerlo por fuera solo saca un
	# warning. Lo que se mide es el mínimo que PIDE el contenido, que no depende
	# del tamaño que tenga puesto.

	# Dos cuadros para que los contenedores resuelvan su tamaño.
	await process_frame
	await process_frame

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

	quit(fallas)
