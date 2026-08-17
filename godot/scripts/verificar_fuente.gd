## verificar_fuente.gd
##
## Comprueba que Godot encuentre todos los glifos de la tipografía.
##
##   godot --headless --path godot res://scenes/VerificarFuente.tscn
##
## ---
##
## LO QUE ESTO CONTESTA Y LOS TESTS DEL C++ NO.
##
## Del lado del C++ ya está comprobado que la fuente está completa: hay un glifo
## por cada carácter que el juego imprime. Pero eso solo dice que el atlas los
## tiene dibujados. Entre el atlas y la pantalla hay una traducción entera —los
## recortes, los avances, el mapeo de carácter a glifo del servidor de texto— y
## cualquier eslabón de esa cadena puede perder una letra sin que nadie avise.
##
## El truco para medirlo sin poder mirar: la fuente es de ancho fijo, seis
## píxeles por carácter. Entonces cualquier texto de N caracteres tiene que medir
## exactamente N × 6. Si a Godot le falta un glifo, sustituye por otra cosa —o
## por nada— y la cuenta no da. Una regla, no una impresión.
##
## Y hay un control negativo: un carácter que la fuente NO tiene DEBE romper esa
## cuenta. Sin esa comprobación, un `get_string_size` que devolviera cualquier
## cosa consistente pasaría los otros veinte tests sin haber medido nada.

extends Node

var _fallas := 0


func _ready() -> void:
	if not Partida.iniciar():
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	print("\nPetBits — la tipografía llega entera hasta Godot\n")

	var m: Dictionary = Partida.core.fuente_metricas()
	var avance: int = m["avance"]
	var tam: int = m["alto"]
	var fuente: FontFile = Partida.fuente

	_afirmar(fuente != null, "la fuente se construyó")
	if fuente == null:
		get_tree().quit(1)
		return

	_afirmar(ThemeDB.fallback_font == fuente, "quedó como fuente por defecto del motor")
	_afirmar(ThemeDB.fallback_font_size == tam, "y con su tamaño nativo")

	print("caja %dx%d · avance %d · ascenso %d · descenso %d · %d glifos" % [
		m["ancho"], tam, avance, m["ascenso"], m["descenso"],
		Partida.core.fuente_glifos().size()
	])

	# -- las medidas de la caja ---------------------------------------------
	_afirmar(int(fuente.get_height(tam)) == tam, "el alto de línea es el de la caja")
	_afirmar(int(fuente.get_ascent(tam)) == int(m["ascenso"]), "el ascenso coincide")
	_afirmar(int(fuente.get_descent(tam)) == int(m["descenso"]), "el descenso coincide")

	# -- cada carácter mide lo que tiene que medir --------------------------
	var muestras := [
		"PetBits",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
		"abcdefghijklmnopqrstuvwxyz",
		"0123456789",
		" !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
		"áéíóúüñ",
		"ÁÉÍÓÚÜÑÂ",
		"¿¡·—→◆",
		"Nébula está de expedición",
		"Musgo · Plácido — vuelve en 15 min",
		"¿Volviste? No te queda de eso.",
	]

	for texto in muestras:
		var ancho: float = fuente.get_string_size(texto, HORIZONTAL_ALIGNMENT_LEFT, -1, tam).x
		var esperado: int = texto.length() * avance
		_afirmar(
			int(ancho) == esperado,
			'"%s" mide %d y tenía que medir %d' % [texto.substr(0, 28), int(ancho), esperado]
		)

	# -- carácter por carácter, todo el juego de glifos ---------------------
	# Lo de arriba pasa aunque un glifo esté mal si otro compensa. Esto no.
	var faltantes := ""
	for g in Partida.core.fuente_glifos():
		var ch := char(g["codigo"])
		if int(fuente.get_string_size(ch, HORIZONTAL_ALIGNMENT_LEFT, -1, tam).x) != avance:
			faltantes += ch
	_afirmar(faltantes == "", "todos los glifos miden un avance; fallan: %s" % faltantes)

	# -- el control negativo -------------------------------------------------
	# Un carácter que la fuente no tiene TIENE que romper la cuenta. Si esto
	# pasara, querría decir que get_string_size devuelve el ancho esperado
	# midiendo o no, y todo lo de arriba no probaría nada.
	var ajeno := "漢"
	var ancho_ajeno: float = fuente.get_string_size(ajeno, HORIZONTAL_ALIGNMENT_LEFT, -1, tam).x
	_afirmar(
		int(ancho_ajeno) != avance,
		"un carácter ajeno a la fuente no mide un avance (midió %d)" % int(ancho_ajeno)
	)

	if _fallas == 0:
		print("\nTodo bien: Godot encuentra los %d glifos." % Partida.core.fuente_glifos().size())
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		print("  ok   %s" % que)
	else:
		print("  FALLA %s" % que)
		_fallas += 1
