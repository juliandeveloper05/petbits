## Historia.gd — todo el texto narrativo del juego, en un solo lugar.
##
## La intro, el vecino, y lo que dice cada objetivo. Nada más: la lógica de
## cuándo se cumple un objetivo vive en `Partida.gd`, y cómo se muestra, en cada
## pantalla. Acá solo hay palabras, para poder cambiarlas sin tocar código.
##
## ---
##
## ESTO ES UN BORRADOR DE ONBOARDING, NO LA HISTORIA.
##
## Julian tiene una historia en mente que todavía no llegó. Mientras tanto, estos
## textos explican el juego con lo que el juego ya es —una criatura que sale de un
## número, un pueblo, un mundo que no se termina— en la misma voz que el resto:
## rioplatense, cálida, la criatura en femenino. Cuando llegue la historia, se
## reemplaza este archivo y nada más.

extends RefCounted


## Lo que se lee al empezar una partida nueva, página por página.
const INTRO := [
	"¿Ves ese código de arriba? Es una semilla: un número de sesenta y cuatro bits. Hay más semillas posibles que estrellas.",
	"Y de ese número salió ella. Sus colores, su forma, su manera de ser: todo está escrito ahí. Quien plante la misma semilla, tiene la misma criatura.",
	"Ahora es tuya. Come, juega, duerme, crece, y se acuerda de cómo la tratás. El tiempo pasa aunque cierres el juego.",
	"Arriba de todo vas a ver siempre qué hacer. Empezá por darle de comer.",
]


## El vecino de la plaza.
const VECINO_NOMBRE := "Alguien del pueblo"

const VECINO := [
	"¡Ah, una nueva! Hola, chiquita. Y hola a vos también, que la estás cuidando.",
	"Mirá: al patio la podés mandar cuando quieras, no le cuesta nada. Al bosque y a las ruinas, solo cuando esté más grande.",
	"Y tardan lo que tardan. Andá a hacer otra cosa mientras — está bien que el juego siga sin vos.",
	"¿Viste los círculos de piedra, afuera? A veces alguien deja una semilla ahí. Llevala al criadero y vas a ver.",
	"¿Y el pueblo? El pueblo se termina en el borde, pero el mundo no. Seguí caminando.",
]


## Lo que dice cada objetivo mientras está pendiente.
##
## El orden lo fija `Partida.OBJETIVOS`; acá solo el texto. Cada uno le dice al
## jugador QUÉ hacer y DÓNDE, porque "explorá" no le dice nada a alguien que
## acaba de abrir el juego.
const OBJETIVOS := {
	"comer": "Dale de comer: elegí Baya, Raíz o Larva.",
	"pueblo": "Salí a caminar: botón Pueblo.",
	"vecino": "Hablá con el vecino de la plaza (Enter).",
	"expedicion": "Mandala al patio a buscar comida.",
	"semilla": "Buscá una semilla: hay círculos de piedra afuera del pueblo.",
	"incubar": "Llevá la semilla al criadero y usá la incubadora.",
	"cruza": "Cuando las dos sean adultas, cruzalas en los pedestales.",
}

## Lo que se dice al cumplir cada uno. Un "¡bien!" genérico no le dice nada a
## nadie; esto le cuenta qué acaba de destrabar.
const CUMPLIDO := {
	"comer": "Comió. Cada comida la cría distinto: lo que come decide en qué se convierte.",
	"pueblo": "Esto es el pueblo. Caminá con las flechas; Esc vuelve a la ficha.",
	"vecino": "Ya conocés al pueblo. Afuera hay mucho más.",
	"expedicion": "Salió. Vuelve sola, aunque cierres el juego.",
	"semilla": "Una semilla. Adentro hay otra criatura, distinta a todas.",
	"incubar": "¡Nació! Ahora son dos. Cambiá entre ellas con < y > en la ficha.",
	"cruza": "Nació una tercera, mezcla de las dos. Así se llena el codex.",
}

## Cuando ya no queda ninguno.
const TODO_HECHO := "Ya sabés todo. Cada semilla es una criatura distinta: salí a buscarlas."

## La tecla de prueba, solo en builds de debug.
const AYUDA_PRUEBA := "F8: pasan 6 horas"
