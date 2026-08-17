## CajaDialogo.gd — la caja de texto de abajo, con su marco y su tipeo.
##
## Se le encolan frases y las va diciendo: escribe letra por letra, espera que le
## den Enter, pasa a la siguiente. Es el recurso más viejo del género y sigue
## siendo el mejor: convierte un mensaje en un momento, y le da al jugador el
## control de cuánto tarda en leerlo.
##
## ---
##
## POR QUÉ NO ES UN LABEL CON FONDO.
##
## Un Label suelto sobre el mapa dice lo mismo, y eso es exactamente el problema:
## dice lo mismo TODO el tiempo. Un cartel que siempre está no se lee, se vuelve
## parte del decorado. Una caja que aparece, escribe y se va convierte al texto
## en un evento — y de paso frena al jugador, que es lo que hace que un diálogo
## se sienta como una conversación y no como un cartel de ruta.
##
## ---
##
## EL CORTE DE LÍNEA SE CUENTA, NO SE MIDE.
##
## La tipografía del juego es de ancho fijo: seis píxeles por carácter, todos.
## Eso convierte "¿entra este texto?" en una cuenta de caracteres en vez de una
## medición contra el servidor de texto. Es exacto, es testeable sin abrir una
## ventana, y es la razón por la que `partir()` es una función estática que se
## puede probar sola.
##
## ---
##
## LA CAJA NO SE ROBA EL TECLADO.
##
## Expone `abierta()` y `avanzar()`, y quien la usa decide cuándo llamarlas. Es a
## propósito: si atajara Enter por su cuenta, la pantalla que la contiene tendría
## que adivinar si la tecla le llegó o no, y ese es el tipo de duda que después
## aparece como "a veces caminaba mientras hablaba".

extends Control

## Se vació la cola: no queda nada por decir.
signal termino

## Cuántos renglones entran en la caja. Tres es lo que usaban las consolas de la
## época y hay una razón: con dos, cualquier frase se parte en muchas páginas;
## con cuatro, la caja se come el mapa.
const RENGLONES := 3

## Caracteres por segundo. A 30 se lee mientras aparece; más rápido y el efecto
## no se nota, más lento y da ganas de saltearlo.
const VELOCIDAD := 30.0

const FONDO := Color("#0a0e0a")
const BORDE := Color("#3d5c46")
const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")

const MARGEN := 5

var _etiqueta: Label = null
var _flecha: Label = null

## Cada frase encolada, ya partida en páginas de hasta RENGLONES renglones.
var _cola: Array = []
var _pagina: Array = []
var _reveladas := 0.0

var _tiempo_flecha := 0.0


func _ready() -> void:
	var alto: int = Partida.tam_fuente
	custom_minimum_size = Vector2(0, RENGLONES * (alto + 2) + MARGEN * 2)

	_etiqueta = Label.new()
	_etiqueta.position = Vector2(MARGEN + 2, MARGEN)
	_etiqueta.add_theme_color_override("font_color", TEXTO)
	_etiqueta.add_theme_font_size_override("font_size", alto)
	_etiqueta.add_theme_constant_override("line_spacing", 2)
	add_child(_etiqueta)

	# El triangulito de "seguí". Vive en su propio Label para poder parpadear sin
	# tocar el texto, que si no habría que reescribirlo en cada cuadro.
	_flecha = Label.new()
	_flecha.text = "▼"
	_flecha.add_theme_color_override("font_color", FOSFORO)
	_flecha.add_theme_font_size_override("font_size", alto)
	add_child(_flecha)

	visible = false
	set_process(true)


# ---------------------------------------------------------------------------
# Lo que se le pide desde afuera
# ---------------------------------------------------------------------------

## Encola una frase. Si la caja estaba cerrada, se abre.
func decir(texto: String) -> void:
	if texto.strip_edges() == "":
		return
	for pagina in _paginar(texto, _columnas()):
		_cola.append(pagina)
	if not visible:
		visible = true
		_siguiente()


## Tira todo lo encolado y muestra esto. Para cuando lo nuevo reemplaza a lo
## viejo en vez de sumarse: el cartel de una zona no tiene que esperar a que
## termine el de la anterior.
func decir_ya(texto: String) -> void:
	_cola.clear()
	_pagina = []
	_reveladas = 0.0
	visible = false
	decir(texto)


## Enter. Si todavía está escribiendo, completa la página; si ya terminó, pasa a
## la siguiente. Es el comportamiento que todo el mundo espera y no hace falta
## explicarlo en ningún lado.
func avanzar() -> void:
	if not visible:
		return
	if _reveladas < _largo_pagina():
		_reveladas = _largo_pagina()
		_etiqueta.visible_characters = -1
		return
	_siguiente()


func abierta() -> bool:
	return visible


func cerrar() -> void:
	_cola.clear()
	_pagina = []
	_reveladas = 0.0
	visible = false


# ---------------------------------------------------------------------------
# El tipeo
# ---------------------------------------------------------------------------

func _process(delta: float) -> void:
	if not visible:
		return

	if _reveladas < _largo_pagina():
		_reveladas = minf(_reveladas + VELOCIDAD * delta, _largo_pagina())
		_etiqueta.visible_characters = int(_reveladas)
		_flecha.visible = false
		return

	# Terminó de escribir: aparece el triangulito.
	_tiempo_flecha += delta
	_flecha.visible = fmod(_tiempo_flecha, 1.0) < 0.6


func _siguiente() -> void:
	if _cola.is_empty():
		visible = false
		termino.emit()
		return

	_pagina = _cola.pop_front()
	_reveladas = 0.0
	_etiqueta.text = "\n".join(_pagina)
	_etiqueta.visible_characters = 0
	_tiempo_flecha = 0.0


func _largo_pagina() -> float:
	return float(_etiqueta.text.length())


# ---------------------------------------------------------------------------
# Dibujo del marco
# ---------------------------------------------------------------------------

## Marco de dos líneas, como el de las consolas de la época.
##
## Se dibuja y no se arma con un StyleBox porque son cuatro llamadas y porque
## así el marco es del mismo material que todo lo demás en este proyecto: código
## que produce píxeles, no un recurso configurado a mano.
func _draw() -> void:
	var r := Rect2(Vector2.ZERO, size)
	draw_rect(r, FONDO, true)
	draw_rect(r, BORDE, false, 1.0)
	draw_rect(Rect2(r.position + Vector2(2, 2), r.size - Vector2(4, 4)), FOSFORO, false, 1.0)


func _notification(que: int) -> void:
	if que == NOTIFICATION_RESIZED:
		queue_redraw()
		if _flecha != null:
			_flecha.position = Vector2(
				size.x - MARGEN - Partida.tam_fuente, size.y - MARGEN - Partida.tam_fuente
			)


## Cuántos caracteres entran a lo ancho.
##
## Se descuentan los márgenes de los dos lados más los dos píxeles del marco
## interno. Si diera de menos el texto se cortaría; si diera de más, se saldría
## de la caja — y como la fuente es monoespaciada, esto es una cuenta exacta y no
## una estimación.
func _columnas() -> int:
	var util: float = size.x - (MARGEN + 2) * 2
	return maxi(1, int(util / 6.0))


# ---------------------------------------------------------------------------
# El corte de línea, que es lo único con lógica de verdad acá
# ---------------------------------------------------------------------------

## Parte un texto en renglones de a lo sumo `columnas` caracteres, sin cortar
## palabras al medio.
##
## Es estática y pública para poder probarla sin instanciar nada ni abrir una
## ventana. El corte de línea es de esas cosas que parecen triviales hasta que
## una palabra larga se come el renglón entero o una frase termina con un espacio
## colgando en el borde.
static func partir(texto: String, columnas: int) -> Array:
	var renglones: Array = []
	if columnas < 1:
		return renglones

	var actual := ""
	for palabra in texto.split(" ", false):
		# Una palabra más larga que el renglón no tiene salvación: se parte. Pasa
		# con un seed pegado sin espacios, por ejemplo.
		while palabra.length() > columnas:
			if actual != "":
				renglones.append(actual)
				actual = ""
			renglones.append(palabra.substr(0, columnas))
			palabra = palabra.substr(columnas)

		if actual == "":
			actual = palabra
		elif actual.length() + 1 + palabra.length() <= columnas:
			actual += " " + palabra
		else:
			renglones.append(actual)
			actual = palabra

	if actual != "":
		renglones.append(actual)
	return renglones


## Y las agrupa en páginas de a RENGLONES.
static func _paginar(texto: String, columnas: int) -> Array:
	var paginas: Array = []
	var renglones := partir(texto, columnas)
	var i := 0
	while i < renglones.size():
		paginas.append(renglones.slice(i, mini(i + RENGLONES, renglones.size())))
		i += RENGLONES
	return paginas
