## CodexMapa.gd — el interior del codex.
##
## Tres estanterías, una por categoría. Pararse frente a una y apretar Enter
## cuenta lo que llevás descubierto de esa categoría.
##
## ---
##
## POR QUÉ TRES ESTANTES Y NO UNA PANTALLA CON TRES PESTAÑAS.
##
## Porque el codex es una colección, y una colección se recorre. Tres pestañas
## muestran la misma información en menos pasos y por eso mismo pesan menos: al
## caminar de un estante a otro pasás por el medio de la sala, y ese medio es
## donde se nota que faltan cosas.
##
## El total incluye a propósito las dos rarezas legendarias que son casi
## inalcanzables —Pangrama es una en 880.000—. Un codex que se completa del todo
## deja de dar motivo para volver.

extends RefCounted

const TILE := 16

## Las mismas medidas que el pueblo, y no por simetría: 30 x 17 tiles de 16
## píxeles son 480 x 272, que es el viewport del juego.
##
## La primera versión de estos interiores medía 20 x 13. Se veía bien en el PNG
## del render —donde cada mapa se compone solo— y en el juego dejaba un tercio de
## la pantalla en negro a la derecha. Ningún test lo dijo: todos preguntaban por
## la grilla, y la grilla estaba perfecta.
## Un interior tiene tamaño: no es el mundo, es una sala.
const INFINITO := false

const ANCHO := 30
const ALTO := 17

enum T { PASTO, CAMINO, AGUA, PIEDRA, ARBOL, PASTO_ALTO, ARENA, MUSGO, PISO, PARED, ALFOMBRA, PEDESTAL }

const ENTRADA := Vector2(15.5 * TILE, 14.5 * TILE)

## Los tres estantes. `categoria` es la clave que devuelve `PetBitsCore.codex()`,
## así que agregar una cuarta categoría del lado del C++ no obliga a tocar el
## despacho: solo esta lista.
const PUNTOS := [
	{"x": 6, "y": 5, "nombre": "Linajes", "tipo": "estante", "categoria": "linajes"},
	{"x": 15, "y": 5, "nombre": "Formas", "tipo": "estante", "categoria": "formas"},
	{"x": 24, "y": 5, "nombre": "Rarezas", "tipo": "estante", "categoria": "rarezas"},
	{"x": 15, "y": 15, "nombre": "La puerta", "tipo": "puerta", "mapa": "pueblo"},
]


static func generar() -> Array:
	var mapa := []
	for y in ALTO:
		var fila := []
		for x in ANCHO:
			fila.append(T.PISO)
		mapa.append(fila)

	for x in ANCHO:
		mapa[0][x] = T.PARED
		mapa[ALTO - 1][x] = T.PARED
	for y in ALTO:
		mapa[y][0] = T.PARED
		mapa[y][ANCHO - 1] = T.PARED

	# Las estanterías: bloques sólidos contra la pared del fondo. Se quedan en la
	# fila 4 y el punto de lectura va en la 5, o sea que te parás DELANTE y no
	# encima — que es como se mira un estante.
	for punto in PUNTOS:
		if punto["tipo"] != "estante":
			continue
		for dx in [-2, -1, 0, 1, 2]:
			mapa[4][punto["x"] + dx] = T.PEDESTAL
			mapa[3][punto["x"] + dx] = T.PEDESTAL
		mapa[2][punto["x"]] = T.MUSGO

	# Un pasillo de alfombra que une los tres y baja hasta la puerta.
	for x in range(3, ANCHO - 3):
		mapa[6][x] = T.ALFOMBRA
	for y in range(6, ALTO - 1):
		mapa[y][15] = T.ALFOMBRA
	mapa[ALTO - 1][15] = T.ALFOMBRA

	return mapa
