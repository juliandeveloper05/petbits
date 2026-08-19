## CriaderoMapa.gd — el interior del criadero.
##
## Las medidas, los puntos y la función que arma la grilla. Todo estático: no se
## instancia, no tiene estado, no sabe de sprites ni de partidas. Igual que
## [PuebloMapa.gd] — ver ahí el porqué de esa separación.
##
## ---
##
## POR QUÉ EL CRIADERO ES UN LUGAR Y NO UN MENÚ.
##
## Cruzar dos criaturas es, mecánicamente, elegir dos de una lista. Eso es un
## menú, y como menú funcionaría igual de bien. Pero el criadero es de los pocos
## momentos del juego donde algo NACE, y una lista desplegable no le hace lugar a
## eso: caminar hasta acá, pararte entre los dos pedestales y apretar un botón le
## da al momento el peso que tiene.
##
## Es la misma decisión que ordena la Fase 3 entera, aplicada al revés que en las
## expediciones. Allá el mapa reemplaza al menú y NO a la espera, porque la
## espera es la mecánica. Acá no hay espera que preservar: el menú es todo lo que
## había, y el lugar lo reemplaza entero.

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

## Los tipos de tile, en el mismo orden que el enum del C++.
##
## Se repite en cada mapa a propósito y no se comparte: si un mapa quisiera
## importarlo de otro, el que lo define pasaría a ser especial sin motivo. El
## contrato real está del lado del C++, y los tests lo comprueban ahí.
enum T { PASTO, CAMINO, AGUA, PIEDRA, ARBOL, PASTO_ALTO, ARENA, MUSGO, PISO, PARED, ALFOMBRA, PEDESTAL }

## Dónde aparecés al entrar: justo adentro de la puerta.
const ENTRADA := Vector2(15.5 * TILE, 14.5 * TILE)

## Los dos pedestales. Se paran las criaturas ahí; vos te parás en el medio.
const PEDESTAL_IZQ := Vector2i(13, 5)
const PEDESTAL_DER := Vector2i(17, 5)

## Los puntos con los que se puede interactuar.
##
## `tipo` decide qué hace Enter encima de cada uno. Es el mismo vocabulario en
## los tres mapas —"puerta", "expedicion", "cruzar", "estante"— así que `Mundo`
## despacha una sola vez y no sabe en cuál está.
const PUNTOS := [
	{"x": 15, "y": 6, "nombre": "Los pedestales", "tipo": "cruzar"},
	{"x": 15, "y": 15, "nombre": "La puerta", "tipo": "puerta", "mapa": "pueblo"},
]


static func generar() -> Array:
	var mapa := []
	for y in ALTO:
		var fila := []
		for x in ANCHO:
			fila.append(T.PISO)
		mapa.append(fila)

	# Las paredes. El borde entero, salvo el hueco de la puerta abajo.
	for x in ANCHO:
		mapa[0][x] = T.PARED
		mapa[ALTO - 1][x] = T.PARED
	for y in ALTO:
		mapa[y][0] = T.PARED
		mapa[y][ANCHO - 1] = T.PARED

	# La alfombra: de la puerta a los pedestales. No es decoración — es lo que
	# enseña adónde ir sin un cartel que lo diga.
	for y in range(5, ALTO - 1):
		mapa[y][15] = T.ALFOMBRA
	for x in range(12, 19):
		mapa[5][x] = T.ALFOMBRA

	# Dos macetas de musgo en las esquinas del fondo, para que la sala no sea
	# cuatro paredes y una alfombra.
	for p in [[2, 2], [3, 2], [2, 3], [26, 2], [27, 2], [27, 3]]:
		mapa[p[1]][p[0]] = T.MUSGO

	# Y los dos pedestales, flanqueando el final de la alfombra.
	mapa[PEDESTAL_IZQ.y][PEDESTAL_IZQ.x] = T.PEDESTAL
	mapa[PEDESTAL_DER.y][PEDESTAL_DER.x] = T.PEDESTAL

	# La puerta: un hueco en la pared de abajo. Se camina, y pararse ahí es lo
	# que dispara la salida.
	mapa[ALTO - 1][15] = T.ALFOMBRA

	return mapa
