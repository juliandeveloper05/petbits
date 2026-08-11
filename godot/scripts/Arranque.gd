## Arranque.gd
##
## Pantalla de diagnóstico. Es la primera escena del proyecto y responde una
## sola pregunta: ¿la GDExtension compiló y cargó?
##
## Existe porque, sin ella, no hay forma de distinguir un `scons` que salió bien
## de uno que dejó el .dll con el nombre equivocado. Godot no avisa gran cosa:
## carga el proyecto igual y las clases C++ simplemente no existen. Con esta
## pantalla el resultado se ve apenas apretás F5.
##
## No depende de la extensión para andar. Si el C++ no está, lo dice y explica
## qué falta — que es justo cuando más se la necesita.

extends Control

const VERDE := Color("#9bbc0f")
const AMBAR := Color("#ffc23d")
const TENUE := Color("#7e937a")

var _core: RefCounted = null


func _ready() -> void:
	var raiz := VBoxContainer.new()
	raiz.set_anchors_preset(Control.PRESET_FULL_RECT)
	raiz.offset_left = 16
	raiz.offset_top = 16
	raiz.offset_right = -16
	raiz.offset_bottom = -16
	raiz.add_theme_constant_override("separation", 6)
	add_child(raiz)

	_titulo(raiz, "PetBits — arranque")
	_linea(raiz, "Godot %s" % Engine.get_version_info().string, TENUE)
	raiz.add_child(HSeparator.new())

	if ClassDB.class_exists("PetBitsCore"):
		_core = ClassDB.instantiate("PetBitsCore")
		_reportar_extension(raiz)
	else:
		_reportar_falta(raiz)


func _reportar_extension(raiz: VBoxContainer) -> void:
	_linea(raiz, "GDExtension cargada.", VERDE)
	_linea(raiz, _core.version(), TENUE)
	raiz.add_child(HSeparator.new())

	# Se decodifica un seed conocido. Es la prueba de que no solo cargó la
	# biblioteca: los algoritmos portados responden y dan lo que tienen que dar.
	var seed := "A3F0-91C4-77BE-2D08"
	var genes: Dictionary = _core.decodificar(seed)

	if genes.is_empty():
		_linea(raiz, "La extensión cargó pero decodificar() devolvió vacío.", AMBAR)
		return

	_linea(raiz, "Semilla de prueba: %s" % genes["seed"], VERDE)
	_linea(raiz, "%s · %s" % [genes["linaje"], genes["temperamento"]], TENUE)
	_linea(raiz, "Afinidad %s · metabolismo %s" % [genes["afinidad"], genes["metabolismo"]], TENUE)

	var rarezas: Array = _core.rarezas(seed)
	if rarezas.is_empty():
		_linea(raiz, "Sin rarezas. Genoma corriente.", TENUE)
	else:
		for r in rarezas:
			_linea(raiz, "◆ %s — %s" % [r["nombre"], r["regla"]], AMBAR)

	raiz.add_child(HSeparator.new())
	_linea(raiz, "Comparalo con petbits.vercel.app: tiene que dar igual.", TENUE)


func _reportar_falta(raiz: VBoxContainer) -> void:
	_linea(raiz, "GDExtension NO cargada.", AMBAR)
	_linea(raiz, "", TENUE)
	_linea(raiz, "El proyecto abre igual, pero el núcleo C++ no está. Falta", TENUE)
	_linea(raiz, "compilarlo. Desde la raíz del repo:", TENUE)
	_linea(raiz, "", TENUE)
	_linea(raiz, "    git submodule update --init --recursive", VERDE)
	_linea(raiz, "    cd gdext && scons", VERDE)
	_linea(raiz, "", TENUE)
	_linea(raiz, "Después cerrá y volvé a abrir el proyecto en Godot: las", TENUE)
	_linea(raiz, "extensiones se cargan al arrancar, no se recargan solas.", TENUE)


func _titulo(padre: Node, texto: String) -> void:
	var etiqueta := Label.new()
	etiqueta.text = texto
	etiqueta.add_theme_font_size_override("font_size", 20)
	etiqueta.add_theme_color_override("font_color", VERDE)
	padre.add_child(etiqueta)


func _linea(padre: Node, texto: String, color: Color) -> void:
	var etiqueta := Label.new()
	etiqueta.text = texto
	etiqueta.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	etiqueta.add_theme_color_override("font_color", color)
	padre.add_child(etiqueta)
