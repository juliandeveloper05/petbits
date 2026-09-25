## Historia.gd — todo el texto narrativo del juego, en un solo lugar.
##
## La intro, la vecina, y lo que dice cada objetivo. Nada más: la lógica de
## cuándo se cumple un objetivo vive en `Partida.gd`, y cómo se muestra, en cada
## pantalla. Acá solo hay palabras, para poder cambiarlas sin tocar código.
##
## ---
##
## LA SEÑORA QUE CONTABA ESTRELLAS.
##
## Había una señora que no podía dormir y contaba estrellas. Una noche contó la
## última y siguió con los números, que no se terminan nunca. Los enterraba como
## semillas, y en cada uno dormía una criatura. Ponía piedras en ronda para
## acordarse de dónde, y se olvidaba igual: esos son los círculos de piedras de
## afuera del pueblo. Dicen que sigue contando, y por eso el mundo no se termina.
##
## Lo cuenta la intro y lo corrige Doña Cuenta, la vecina de la plaza, que
## también salió de un número y ni se acuerda de cuál. Al jugador le toca
## acordarse por la señora: encontrar lo que enterró y hacerlo nacer. Y lo que
## el número no dice —cuánto te va a querer— es el Vínculo, y lo escribe él.
##
## Salió de cuatro borradores desde ángulos distintos, tres jueces y tres
## lecturas de un jugador nuevo. Dos reglas que no hay que romper al tocarlo:
##
## - Cada página le deja al jugador algo concreto: un botón, un lugar, una tecla.
##   El tester no tiene a nadie que le explique nada.
## - No promete nada que el juego no haga. La señora nunca aparece; el mundo NO
##   es igual para todos (sale de la primera criatura), así que nunca se dice que
##   un círculo guarda lo mismo para cualquiera.
##
## Ganchos que quedan abiertos para después, sin prometer nada hoy:
## - ¿Quién era la señora? Rastros suyos muy lejos en el mundo infinito.
## - El número de Doña Cuenta es fijo (C0FE-1DEA-5EED-B10C, en PuebloMapa.gd):
##   una línea nueva de ella el día que el jugador lo descubra.
## - Otros vecinos con otras versiones del cuento: ¿estrellas u ovejas?
## - Un círculo que no guarde una semilla sino otra cosa que la señora dejó.
## - Si hay cielo nocturno: estrellas que se cuentan.

extends RefCounted


## Lo que se lee al empezar una partida nueva, página por página.
const INTRO := [
	"Cuentan que había una señora que no podía dormir, y en vez de ovejas contaba estrellas. Una noche contó la última... y seguía despierta.",
	"Siguió con los números, que no se terminan nunca, y los iba enterrando como semillas. En cada uno dormía una criatura, como esta chiquita. ¿Ves arriba donde dice Semilla? Ese código es su número.",
	"Esta pantalla es su ficha. De ese número sale casi todo: su color, su linaje, sus mañas. El mismo número da siempre la misma criatura, a vos o a cualquiera.",
	"Ahora es tuya. Lo que el número no dice es cuánto te va a querer: eso es el Vínculo, y lo escribís vos, con cada comida, cada juego, cada mimo.",
	"Y su tiempo corre aunque cierres el juego: le da hambre, duerme y crece, como todo el mundo. Arriba de todo, la línea con → te dice qué hacer. Ahora, dale de comer: Baya, Raíz o Larva.",
]


## La vecina de la plaza. El nombre es también el cartel que aparece al
## acercarse ("Doña Cuenta — Enter para hablar"): PuebloMapa.gd lo lee de acá.
const VECINO_NOMBRE := "Doña Cuenta"

const VECINO := [
	"¡Una nueva! Hola, chiquita. Y hola a vos: me dicen Doña Cuenta. Al patio la podés mandar cuando quieras; al bosque y a las ruinas, cuando esté más grande.",
	"Y no la esperes en la puerta, que tarda lo que tarda. Vos andá a hacer otra cosa: vuelve sola, aunque cierres el juego. Como los gatos.",
	"¿Lo de la señora te lo contaron? Seguro mal. Las ovejas ya las había contado todas, por eso siguió con las estrellas. Y donde enterraba un número, ponía piedras en ronda para acordarse.",
	"Después se olvidaba igual, pobre, y las semillas quedaron ahí, en círculos de piedras afuera del pueblo. Llevalas a la incubadora del criadero: alguien se tiene que acordar.",
	"¿Y la señora? Dicen que sigue contando, y por eso el mundo no se termina. El pueblo sí: se termina en el borde. Yo también salí de un número, ni me acuerdo cuál. Andá, que la chiquita se aburre.",
]


## Lo que dice cada objetivo mientras está pendiente.
##
## El orden lo fija `Partida.OBJETIVOS`; acá solo el texto. Cada uno le dice al
## jugador QUÉ hacer y DÓNDE, porque "explorá" no le dice nada a alguien que
## acaba de abrir el juego. Hasta 74 caracteres: con el "→ " adelante son las 76
## letras que entran en la línea de arriba de la ficha sin cortarse (lo verifica
## VerificarDialogo). Mejor bastante menos: se lee de reojo.
const OBJETIVOS := {
	"comer": "Dale de comer: elegí Baya, Raíz o Larva.",
	"pueblo": "Salí a caminar: botón Pueblo. En la plaza te cuentan algo.",
	"vecino": "Hablá con Doña Cuenta en la plaza: acercate y apretá Enter.",
	"expedicion": "Mandala al patio: su cartel en el pueblo, o botón El patio.",
	"semilla": "Afuera del pueblo, pisá un círculo de piedras y apretá Enter.",
	"incubar": "Volvé al pueblo (0, 0) y usá la incubadora del criadero.",
	"cruza": "Cuando sean adultas, cruzalas en los pedestales del criadero.",
}

## Lo que se dice al cumplir cada uno. Un "¡bien!" genérico no le dice nada a
## nadie; esto le cuenta qué acaba de destrabar y a dónde ir ahora.
const CUMPLIDO := {
	"comer": "Comió, y con ganas. Ojo: lo que come decide en qué se convierte. Ahora sacala a pasear: botón Pueblo.",
	"pueblo": "Esto es el pueblo. Caminá con las flechas; con Esc volvés a la ficha. Acá nomás, a tu izquierda, hay alguien que cuenta cuentos: acercate y apretá Enter.",
	"vecino": "Si te olvidás de algo, volvé a hablarle: lo cuenta siempre igual. Ahora mandala al patio: acercate al cartel de arriba a la izquierda y apretá Enter. O Esc, y en la ficha, botón El patio.",
	"expedicion": "Allá fue. Vuelve sola en un rato, con algo de comer; si la ves medio transparente, es que anda lejos. Mientras, salí más allá del borde del pueblo: afuera hay círculos de piedras con semillas.",
	"semilla": "Uno de los números que la señora enterró y olvidó: adentro duerme otra criatura. Volvé al pueblo, hacia el 0, 0 de los números de abajo, y llevá la semilla al criadero, abajo a la izquierda.",
	"incubar": "¡Nació! La señora se olvidó de ella; vos no. Cambiá entre las dos con < y > en la ficha. Para cruzarlas en los pedestales tienen que ser adultas y quererte: eso lleva unos días.",
	"cruza": "Una tercera, mezcla de las dos. A esta no la enterró la señora: la hicieron ustedes. Así se va llenando el codex: la biblioteca, arriba de la plaza.",
}

## Cuando ya no queda ninguno.
const TODO_HECHO := "La señora sigue contando. Vos buscá sus semillas en los círculos."

## La tecla de prueba, solo en builds de debug.
const AYUDA_PRUEBA := "F8: pasan 6 horas"
