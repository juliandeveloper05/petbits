## PuebloMapa.gd — la forma del pueblo, sin nada más.
##
## Las medidas, las zonas, y la función que arma la grilla. Todo estático: no se
## instancia, no tiene estado, no sabe de sprites ni de partidas.
##
## ---
##
## POR QUÉ ESTÁ SEPARADO DE `Mundo.gd`.
##
## Al principio el mapa vivía adentro de la escena, y la herramienta que lo
## dibuja en PNG instanciaba el `Node2D` entero solo para leerle la grilla. Eso
## traía dos problemas: el nodo nunca entraba al árbol y había que liberarlo a
## mano, y —el que lo rompió de verdad— `Mundo.gd` pasó a depender del autoload
## `Partida`, que no existe cuando Godot arranca con `--script`. El script no
## compilaba, `load()` devolvía null, y el renderizador se colgaba sin decir por
## qué.
##
## Los datos y la escena que los muestra son dos cosas distintas. Separarlas hace
## que la herramienta pueda leer el pueblo sin arrastrar medio juego detrás.

extends RefCounted

const TILE := 16
const ANCHO := 30
const ALTO := 17

## Los tipos de tile, en el mismo orden que el enum del C++.
##
## El orden es un contrato: la grilla guarda índices, así que reordenar acá los
## rompe todos. Si se agrega un tile, va al final.
enum T { PASTO, CAMINO, AGUA, PIEDRA, ARBOL, PASTO_ALTO, ARENA, MUSGO, PISO, PARED, ALFOMBRA, PEDESTAL }

## Dónde arranca la criatura, en píxeles: el centro de la plaza.
const ENTRADA := Vector2(15.5 * TILE, 8.5 * TILE)

## Los puntos con los que se puede interactuar, y de qué tipo es cada uno.
##
## Son de dos clases y la diferencia es la decisión de diseño que ordena toda
## la fase:
##
##   - Las TRES ENTRADAS de expedición —patio, bosque, ruinas— no llevan a
##     ningún lado. Son destinos que tardan quince minutos, hora y media o cuatro
##     horas de tiempo real, con el juego cerrado. Si caminaras hasta el bosque
##     en tres segundos esa espera dejaría de existir, y la espera es media
##     mecánica. Llegás hasta la puerta y desde ahí la mandás.
##
##   - Las DOS PUERTAS —criadero y codex— sí abren a un mapa propio, y por el
##     motivo inverso: no hay espera que preservar. Eran un menú, y un menú se
##     puede reemplazar entero por un lugar sin perder nada.
##
## `tipo` es el mismo vocabulario en los tres mapas, así que `Mundo` despacha
## una sola vez y no necesita saber en cuál está.
const PUNTOS := [
	{"x": 4, "y": 3, "nombre": "El patio", "tipo": "expedicion", "destino": "patio"},
	{"x": 25, "y": 3, "nombre": "El bosque", "tipo": "expedicion", "destino": "bosque"},
	{"x": 25, "y": 13, "nombre": "Las ruinas", "tipo": "expedicion", "destino": "ruinas"},
	{"x": 4, "y": 13, "nombre": "El criadero", "tipo": "puerta", "mapa": "criadero"},
	{"x": 15, "y": 2, "nombre": "El codex", "tipo": "puerta", "mapa": "codex"},

	# El vecino de la plaza. Su seed es fijo: es siempre la misma criatura, y eso
	# importa más de lo que parece. Un NPC con genoma al azar cambiaría de cara
	# cada vez que abrís el juego, y dejaría de ser alguien para volver a ser una
	# textura que habla.
	{
		"x": 12, "y": 9, "nombre": "Alguien del pueblo", "tipo": "npc",
		"seed": "C0FE-1DEA-5EED-B10C",
		"dice": [
			"Ah, vos sos la que cuida a esa. Se nota.",
			"Mirá: al patio la podés mandar cuando quieras, no le cuesta nada."
			+ " Al bosque y a las ruinas, solo cuando esté lista.",
			"Y tardan lo que tardan. Andá a hacer otra cosa mientras — está bien"
			+ " que el juego siga sin vos.",
		],
	},
]


## La grilla del pueblo: ALTO filas de ANCHO índices de tile.
##
## A mano y no procedural: es el lugar donde el jugador arranca y tiene que
## leerse como un sitio pensado, no como ruido. Los caminos van de la plaza
## central a cada zona, que es lo que enseña sin decir nada que esas son las
## salidas.
static func generar() -> Array:
	var mapa := []
	for y in ALTO:
		var fila := []
		for x in ANCHO:
			fila.append(T.PASTO)
		mapa.append(fila)

	# Un borde de árboles: encierra el pueblo sin necesidad de paredes
	# invisibles, que son la manera fea de resolver lo mismo.
	for x in ANCHO:
		mapa[0][x] = T.ARBOL
		mapa[ALTO - 1][x] = T.ARBOL
	for y in ALTO:
		mapa[y][0] = T.ARBOL
		mapa[y][ANCHO - 1] = T.ARBOL

	# La plaza, en el centro.
	for y in range(7, 11):
		for x in range(12, 19):
			mapa[y][x] = T.CAMINO

	# Un caminito a cada zona.
	for punto in PUNTOS:
		if punto["tipo"] != "npc":
			_trazar_camino(mapa, 15, 8, punto["x"], punto["y"])

	# Un estanque, para que no sea todo verde y camino.
	for y in range(11, 15):
		for x in range(19, 24):
			mapa[y][x] = T.AGUA
	for x in range(19, 24):
		mapa[10][x] = T.ARENA

	# Pasto alto en dos manchones. Es decorativo hoy; cuando haya criaturas
	# salvajes, es donde aparecerán.
	for p in [[7, 4], [8, 4], [7, 5], [21, 4], [22, 4], [21, 5], [22, 5]]:
		mapa[p[1]][p[0]] = T.PASTO_ALTO

	# Y los puntos, marcados en piedra. El NPC no: es alguien parado ahí, no una
	# entrada, y ponerle una piedra debajo lo convertiría en decorado.
	for punto in PUNTOS:
		if punto["tipo"] == "npc":
			continue
		mapa[punto["y"]][punto["x"]] = T.PIEDRA

	return mapa


## Un camino en L entre dos puntos. Primero horizontal, después vertical.
static func _trazar_camino(mapa: Array, x0: int, y0: int, x1: int, y1: int) -> void:
	for x in range(min(x0, x1), max(x0, x1) + 1):
		if _dentro(x, y0):
			mapa[y0][x] = T.CAMINO
	for y in range(min(y0, y1), max(y0, y1) + 1):
		if _dentro(x1, y):
			mapa[y][x1] = T.CAMINO


## El borde de árboles no se pisa: es la pared del pueblo.
static func _dentro(x: int, y: int) -> bool:
	return x > 0 and y > 0 and x < ANCHO - 1 and y < ALTO - 1
