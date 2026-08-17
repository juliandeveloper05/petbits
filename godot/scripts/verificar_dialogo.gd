## verificar_dialogo.gd
##
## Comprueba el corte de línea de la caja de diálogo.
##
##   godot --headless --path godot res://scenes/VerificarDialogo.tscn
##
## ---
##
## POR QUÉ ES LO ÚNICO QUE SE TESTEA DE LA CAJA.
##
## Del resto —el marco, el tipeo, el triangulito que parpadea— no hay nada que
## comprobar que no sea mirarlo. Pero el corte de línea sí tiene una respuesta
## correcta, y es de esas cosas que parecen triviales hasta que una palabra larga
## se come el renglón entero, o una frase de treinta y siete caracteres entra
## justo y la de treinta y ocho desaparece por el borde.
##
## Que la tipografía sea de ancho fijo hace esto exacto: "¿entra?" es contar
## caracteres, no medirlos. Por eso `partir()` es estática y se prueba sola, sin
## instanciar la caja ni abrir una ventana.
##
## Las frases de prueba incluyen las que el juego dice de verdad, más los casos
## que rompen: una palabra más larga que el renglón, un texto de un solo carácter
## y espacios de más en el medio.

extends Node

const CajaDialogo = preload("res://scripts/CajaDialogo.gd")

var _fallas := 0


func _ready() -> void:
	print("\nPetBits — el corte de línea de la caja de diálogo\n")

	var frases := [
		"Nébula está de expedición y vuelve en hora y media.",
		"El bosque. Hay cosas raras entre los árboles, pero está lejos.",
		"No te queda de eso. Mandala a buscar.",
		"¿Volviste? Mientras no estabas se le bajó el ánimo y durmió un rato.",
		"Las ruinas. Cuatro horas de ida y vuelta, y no siempre trae algo.",
		"A3F0-91C4-77BE-2D08",
		"a",
		"palabra  con   espacios    de    mas",
		"unapalabramuchomaslargaqueelrenglonenteroynoentraniahipalos",
	]

	# Varios anchos: el de la caja de verdad y algunos apretados, que es donde
	# los cortes de línea se rompen.
	for columnas in [8, 12, 37, 76]:
		for frase in frases:
			_probar(frase, columnas)

	# El caso degenerado: cero columnas no tiene que colgarse ni devolver basura.
	_afirmar(CajaDialogo.partir("hola mundo", 0).is_empty(), "con cero columnas no devuelve nada")
	_afirmar(CajaDialogo.partir("", 40).is_empty(), "un texto vacío no produce renglones")

	if _fallas == 0:
		print("\nTodo bien: %d frases cortadas sin perder ni una palabra." % (frases.size() * 4))
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


func _probar(frase: String, columnas: int) -> void:
	var renglones: Array = CajaDialogo.partir(frase, columnas)
	var ctx := '%d col · "%s"' % [columnas, frase.substr(0, 24)]

	# 1. Nada se sale del ancho. Es la razón de ser de la función.
	var mas_ancho := 0
	for r in renglones:
		mas_ancho = maxi(mas_ancho, r.length())
	_afirmar(mas_ancho <= columnas, "%s — el renglón más ancho mide %d" % [ctx, mas_ancho])

	# 2. No se pierde ni se inventa nada.
	#
	# Esta es la que importa de verdad. Un corte de línea que descarta la última
	# palabra pasa el test del ancho con las mejores notas: los renglones que
	# quedan entran todos.
	#
	# Se compara SIN espacios. La primera versión de este test juntaba los
	# renglones con un espacio y comparaba contra el original — y falló siete
	# veces, todas legítimas: cuando una palabra no entra en el renglón se parte
	# a la fuerza, y ahí rearmar con espacios inventa uno que nunca estuvo. El
	# invariante que vale para todos los anchos es que la secuencia de caracteres
	# que no son espacio sea la misma.
	var original := " ".join(frase.split(" ", false))
	var rearmado := "".join(renglones)
	_afirmar(
		rearmado.replace(" ", "") == original.replace(" ", ""),
		"%s — se perdió o se inventó texto (%d vs %d caracteres)" % [
			ctx, rearmado.replace(" ", "").length(), original.replace(" ", "").length()
		]
	)

	# 2b. Y cuando ninguna palabra necesita partirse, el corte tiene que ser
	# exacto: juntar los renglones con un espacio devuelve el original tal cual.
	# Sin esto, el chequeo de arriba dejaría pasar un corte que se come todos los
	# espacios y pega las palabras.
	var hay_palabra_larga := false
	for palabra in frase.split(" ", false):
		if palabra.length() > columnas:
			hay_palabra_larga = true
	if not hay_palabra_larga:
		_afirmar(" ".join(renglones) == original, "%s — el texto rearmado no coincide" % ctx)

	# 3. Ningún renglón arranca o termina con espacio.
	var limpio := true
	for r in renglones:
		if r != r.strip_edges():
			limpio = false
	_afirmar(limpio, "%s — hay renglones con espacios colgando" % ctx)

	# 4. Y ninguno está vacío: un renglón en blanco en el medio de una frase es
	# un salto de página que nadie pidió.
	var sin_vacios := true
	for r in renglones:
		if r == "":
			sin_vacios = false
	_afirmar(sin_vacios, "%s — hay renglones vacíos" % ctx)


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		return  # los éxitos no se imprimen: son ciento cuarenta y cuatro
	print("  FALLA %s" % que)
	_fallas += 1
