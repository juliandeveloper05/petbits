## Mapas.gd — qué mapas hay y dónde está cada uno.
##
## Un registro y nada más. Existe para que `Mundo` no tenga una cadena de `if`
## con los nombres adentro: agregar un mapa es agregar una línea acá y un archivo
## de datos al lado, sin tocar la escena que los dibuja.
##
## Los scripts se traen con `preload` y no con `load` a propósito. `preload` se
## resuelve al compilar, así que un nombre mal escrito acá es un error al abrir
## el proyecto y no un `null` a mitad de una puerta.

extends RefCounted

const PUEBLO = preload("res://scripts/PuebloMapa.gd")
const CRIADERO = preload("res://scripts/CriaderoMapa.gd")
const CODEX = preload("res://scripts/CodexMapa.gd")

## Dónde empieza el juego. El pueblo es el único mapa con salida a PetView.
const INICIAL := "pueblo"


## El script del mapa, o el del pueblo si el id no existe.
##
## No revienta ante un id desconocido: un mapa que falta tiene que dejarte en el
## pueblo, no dejarte sin juego. Puede pasar al cargar una partida guardada por
## una versión más nueva.
static func script_de(id: String):
	match id:
		"pueblo": return PUEBLO
		"criadero": return CRIADERO
		"codex": return CODEX
	return PUEBLO


## Todos los ids, para que las herramientas puedan recorrerlos sin repetir la
## lista. La usa el render de PNG para componer los tres mapas de una.
static func todos() -> Array:
	return ["pueblo", "criadero", "codex"]
