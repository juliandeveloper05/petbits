## PuebloMapa.gd — los puntos del pueblo. El terreno ya no está acá.
##
## Hasta la Fase 3 este archivo tenía la grilla entera del pueblo, tile por tile.
## Ahora el pueblo es una región del mundo infinito y su terreno lo genera
## `world_gen.cpp`, junto con todo lo demás.
##
## ---
##
## POR QUÉ SE MUDÓ EL TERRENO Y NO LOS PUNTOS.
##
## Si la grilla del pueblo se hubiera quedado de este lado, el generador no
## podría verla: habría que componer el pueblo y el mundo AFUERA, con un caso
## especial en el walker que preguntara "¿estoy en el pueblo?" antes de cada
## tile. Con la grilla adentro del generador, `tileEnMundo` es una función total
## y `Mundo` no necesita saber que el pueblo existe.
##
## Los PUNTOS se quedan porque no son terreno: son con qué se puede interactuar,
## qué dice cada cosa, dónde está parado el vecino. Eso es contenido de juego y
## vive mejor en un archivo que se edita sin recompilar.
##
## ---
##
## LAS COORDENADAS SON DE MUNDO, NO DE PUEBLO.
##
## Antes iban de (0,0) a (29,16), que era el mapa entero. Ahora el pueblo está
## centrado en el origen del mundo, así que van de (-15,-8) a (14,8). El
## rectángulo lo define `world_gen.h` y `PetBitsCore.pueblo_rect()` lo expone,
## para que no haya dos lugares donde esté escrito dónde empieza el pueblo.

extends RefCounted

const TILE := 16

## Este "mapa" no tiene tamaño: es el mundo. `Mundo` lo trata distinto — carga
## chunks y mueve la cámara en vez de volcar una grilla fija.
const INFINITO := true

## Dónde arranca la criatura al empezar una partida: la plaza, en el origen.
const ENTRADA := Vector2(0.5 * TILE, 0.5 * TILE)

## Los puntos con los que se puede interactuar, en coordenadas de MUNDO.
##
## Son de dos clases y la diferencia es la decisión de diseño que ordena la fase:
##
##   - Las TRES ENTRADAS de expedición —patio, bosque, ruinas— no llevan a ningún
##     lado. Son destinos que tardan quince minutos, hora y media o cuatro horas
##     de tiempo real, con el juego cerrado. Ahora que el mundo es infinito y
##     caminable la tentación de convertirlas en lugares es más fuerte, y sería
##     el mismo error: la espera es media mecánica.
##
##   - Las DOS PUERTAS —criadero y codex— sí abren a un mapa propio, porque ahí
##     no hay espera que preservar. Eran un menú.
const PUNTOS := [
	{"x": -11, "y": -5, "nombre": "El patio", "tipo": "expedicion", "destino": "patio"},
	{"x": 10, "y": -5, "nombre": "El bosque", "tipo": "expedicion", "destino": "bosque"},
	{"x": 10, "y": 5, "nombre": "Las ruinas", "tipo": "expedicion", "destino": "ruinas"},
	{"x": -11, "y": 5, "nombre": "El criadero", "tipo": "puerta", "mapa": "criadero"},
	{"x": 0, "y": -6, "nombre": "El codex", "tipo": "puerta", "mapa": "codex"},

	# El vecino de la plaza. Su seed es fijo: es siempre la misma criatura, y eso
	# importa más de lo que parece. Un NPC con genoma al azar cambiaría de cara
	# cada vez que abrís el juego, y dejaría de ser alguien para volver a ser una
	# textura que habla.
	{
		"x": -3, "y": 1, "nombre": "Alguien del pueblo", "tipo": "npc",
		"seed": "C0FE-1DEA-5EED-B10C",
		"dice": [
			"Ah, vos sos la que cuida a esa. Se nota.",
			"Mirá: al patio la podés mandar cuando quieras, no le cuesta nada."
			+ " Al bosque y a las ruinas, solo cuando esté lista.",
			"Y tardan lo que tardan. Andá a hacer otra cosa mientras — está bien"
			+ " que el juego siga sin vos.",
			"¿Y el pueblo? El pueblo se termina en el borde, pero el mundo no."
			+ " Segui caminando y vas a ver.",
		],
	},
]
