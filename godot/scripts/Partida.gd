## Partida.gd — el juego, una sola vez, para todas las pantallas.
##
## Es un autoload. Guarda el `PetBitsCore`, el archivo de guardado, el reloj de
## la simulación y la bitácora, y las escenas se cuelgan de él.
##
## ---
##
## POR QUÉ EXISTE.
##
## Antes cada escena creaba su propio `PetBitsCore`. Con una sola pantalla eso no
## se notaba, pero al aparecer el mapa el problema quedó a la vista: el pueblo
## mostraba una criatura al azar, no la tuya, y mandarla de expedición desde ahí
## no habría tenido ningún efecto sobre la partida que estabas jugando. Dos cores
## son dos juegos distintos corriendo al mismo tiempo.
##
## El core vive acá, entonces, y `change_scene_to_file` deja de perder cosas: al
## cambiar de pantalla se destruyen los nodos, no la partida.
##
## ---
##
## NO SE INICIA SOLO.
##
## `iniciar()` es explícito y lo llama la escena que necesita jugar. Podría
## hacerse en `_ready()` y ahorrarse la llamada, pero entonces las herramientas
## de línea de comandos —el render del mapa, la hoja de contacto, el medidor de
## layout— cargarían tu partida al arrancar, y si no existiera crearían una. Una
## herramienta que compone un PNG no tiene por qué tocar tu save.
##
## ---
##
## LOS TONOS VIAJAN COMO NOMBRE, NO COMO COLOR.
##
## `nota` emite "aviso" o "bien", no un `Color`. El modelo no sabe de qué color
## es la consola: eso lo decide cada pantalla, y el mapa pinta sus carteles
## distinto que el registro de PetView.

extends Node

const Tipografia = preload("res://scripts/Tipografia.gd")

## Algo pasó en el mundo y las pantallas tienen que redibujarse.
signal cambio

## El tiempo avanzó de verdad (al menos un tick). Trae lo que devolvió `simular`.
signal avanzo(sim: Dictionary)

## Volvió de una expedición. Trae lo que devolvió `recibir`.
signal volvio(r: Dictionary)

## Una línea para la bitácora. `tono` ∈ normal · tenue · bien · aviso · alerta · raro
signal nota(texto: String, tono: String)

## Se cumplió el objetivo que estaba a la vista. Trae su id; el texto lo pone
## cada pantalla desde `Historia.gd`.
signal objetivo_cumplido(id: String)

## Dónde vive la partida.
##
## `user://` es la carpeta de datos del usuario que resuelve Godot en cada
## sistema operativo. Nunca `res://`: eso es el proyecto, y en un juego exportado
## viene dentro del paquete y es de solo lectura.
const RUTA_SAVE := "user://partida.json"

## Nombre del archivo cuando un save no se puede leer.
##
## El save roto NO se borra: se corre de lugar. Alguien puede haber perdido meses
## de partida por un corte de luz a mitad de una escritura, y un archivo que no
## carga hoy puede ser recuperable a mano mañana. Borrarlo es una decisión que le
## toca al dueño, no al programa.
const RUTA_CUARENTENA := "user://partida.rota.json"

## Dónde vive lo que es SOLO del nativo.
##
## Un archivo aparte, y no un campo más adentro de `partida.json`. Ese archivo es
## el formato de la web y la web no tiene mundo: agregarle un campo propio
## rompería la promesa de que los dos programas escriben exactamente lo mismo, y
## la validación contra su esquema real empezaría a fallar.
##
## Acá adentro va dónde estabas y qué ya levantaste del suelo. Si falta, aparecés
## en el pueblo y el mundo está entero — que es exactamente lo que pasa la
## primera vez.
const RUTA_MUNDO := "user://mundo.json"

## Un tick del juego es un minuto real. Preguntar más seguido no cambia nada
## —`simular` solo avanza en ticks enteros— pero mantiene la pantalla al día.
const INTERVALO_CONSULTA := 1.0

## Cuántas líneas de bitácora se recuerdan al cambiar de pantalla.
##
## La bitácora se guarda acá y no en PetView justamente por esto: si viviera en
## la pantalla, ir al pueblo y volver la borraría, y con ella el "mientras no
## estabas" que es medio motivo de abrir el juego.
const MAXIMO_BITACORA := 120

## Adónde se guarda de verdad.
##
## Son variables y no las constantes de arriba por las herramientas de línea de
## comandos: `medir_layout` y `verificar_mundo` instancian las pantallas reales,
## y sin poder desviar la ruta cada corrida jugaría con la partida del usuario
## —le haría nacer una criatura, o le mandaría la suya de expedición—. Un medidor
## de layout no tiene por qué tocarte el save.
##
## Se cambian ANTES de `iniciar()`; después no tienen efecto.
var ruta_save := RUTA_SAVE
var ruta_cuarentena := RUTA_CUARENTENA
var ruta_mundo := RUTA_MUNDO

## Si se guarda automáticamente al cerrar.
##
## Los tests lo apagan, y no es capricho: borran sus archivos al terminar y el
## manejador de cierre volvía a escribirlos justo después, así que la carpeta de
## datos del usuario se llenaba de saves de prueba que nadie borraba nunca.
var guardar_al_salir := true

## Si esta sesión tiene PROHIBIDO guardar.
##
## Se prende cuando había un save que no se pudo leer y tampoco se pudo apartar.
## En ese caso no sabemos qué tiene adentro, no lo pudimos poner a salvo, y
## escribirle encima sería destruir la partida de alguien — puede ser el archivo
## lockeado por otra copia del juego, o el antivirus mirándolo en el arranque,
## que son dos cosas que se arreglan solas en cinco minutos.
##
## Se juega igual. Lo que no pasa es que se escriba.
var guardar_bloqueado := false

## Con qué genoma nace la criatura si no hay partida guardada.
##
## Vacío significa al azar, que es lo que corresponde jugando. Los tests lo fijan
## y por el mismo motivo que fijan la ruta del save: una criatura distinta en cada
## corrida es un sujeto distinto en cada corrida.
##
## Esto no es una comodidad, es lo que separa un test de un sorteo. El de los
## interiores cría durante seis días simulados hasta que la criatura pueda cruzar,
## y el metabolismo —que sale del genoma— decide cuánta hambre pasa en el camino.
## Con seed al azar pasaba tres veces de cada cuatro, y esa cuarta no significaba
## nada.
var semilla_inicial := ""

## En qué mapa estás, y de cuál venías.
##
## NO se guardan en el archivo, y eso es deliberado: el formato del save es el de
## la web, y la web no tiene mapas. Agregar un campo propio rompería la promesa
## de que los dos programas escriben lo mismo, y a cambio ahorraría reaparecer en
## la plaza al abrir el juego — que es donde uno espera aparecer igual.
##
## `venir_de` es lo que hace que salir del criadero te deje en su puerta y no en
## el medio del pueblo.
var mapa := "pueblo"
var venir_de := ""

## Dónde quedó parada la criatura, en píxeles del mundo.
var donde := Vector2.ZERO

## Las coordenadas que ya se recolectaron, como "x,y".
##
## Se guarda lo LEVANTADO y no lo que queda, porque lo que queda es infinito. Un
## mundo sin techo obliga a invertir la pregunta: no "qué hay", que lo contesta el
## generador, sino "qué sacaste", que es finito y cabe en un archivo.
var recolectado := {}

## Cuántas coordenadas se recuerdan.
##
## Con el tope, lo más viejo se olvida y el pasto vuelve a crecer. No es una
## limitación disfrazada de mecánica: un jugador que camina mil tiles no se
## acuerda de qué mata pisó hace tres días, y el archivo tampoco tiene por qué.
const MAXIMO_RECOLECTADO := 4000

## La semilla del mundo: el genoma de la primera criatura de la colección.
##
## Se cachea porque se pregunta muchísimo —cada comprobación de colisión la
## necesita— y porque no cambia mientras dure la partida. Se refresca al cargar y
## al nacer, que son los dos momentos en que la colección puede cambiar de
## primera criatura.
var semilla_mundo := ""

var core: RefCounted = null

## La tipografía del juego, armada desde el atlas que genera el C++.
##
## Queda a mano para las pantallas que quieran pedirle medidas —cuánto mide un
## texto antes de dibujarlo— pero no hace falta asignarla a nada: `instalar()` la
## deja como fuente por defecto del motor.
var fuente: FontFile = null

## El alto de la caja de la fuente, que es su tamaño nativo.
##
## Las pantallas lo usan en vez de números sueltos. Pedirle 9 o 12 a una bitmap
## font de 11 la escala, y a este tamaño escalar es romperla.
var tam_fuente := 11

## Todo lo anotado desde que arrancó el programa: [{ texto, tono }].
var bitacora: Array = []

var _iniciada := false
var _reloj: Timer = null

## Si esta sesión CARGÓ una partida de verdad.
##
## No es lo mismo que tener un core. La pantalla de título llama a `preparar()`
## para tener la tipografía, y eso crea el core sin cargar nada: si el guard de
## `guardar_mundo()` mirara solo el core, salir desde el menú escribiría un mundo
## vacío encima del tuyo — el mismo agujero que ya se cerró una vez para las
## herramientas de línea de comandos, abierto de nuevo por otra puerta.
var _cargada := false

## Si esta partida arrancó con una criatura recién nacida.
var partida_nueva := false

## Cuánto se adelantó el reloj con la tecla de prueba, en milisegundos.
##
## Se GUARDA, y eso es lo importante. Si no se guardara, al reabrir el juego el
## reloj volvería seis horas para atrás por cada F8: la criatura tendría su
## último tick en el futuro y se quedaría congelada hasta que el tiempo real la
## alcanzara. Vive en `mundo.json`, que es del nativo: `partida.json` no gana
## ningún campo.
var desfase_ms := 0

## Seis horas: lo que avanza cada F8.
const ADELANTO_PRUEBA_MS := 6 * 60 * 60 * 1000

## Los objetivos del tutorial, en orden. El texto de cada uno está en
## `Historia.gd`; acá solo importa el orden y cuándo se cumplen.
const OBJETIVOS := ["comer", "pueblo", "vecino", "expedicion", "semilla", "incubar", "cruza"]

## Cuáles ya se cumplieron, como conjunto. También vive en `mundo.json`.
var objetivos_hechos := {}


## Prepara la partida. Idempotente: llamarla dos veces no hace nada la segunda.
##
## Devuelve false si la GDExtension no cargó, que es el único modo de falla que
## las pantallas necesitan distinguir — sin C++ no hay juego que mostrar.
func iniciar() -> bool:
	if _iniciada:
		return core != null
	_iniciada = true

	if not preparar():
		return false

	_cargada = true

	# El archivo del mundo va PRIMERO, porque trae el desfase del reloj de
	# prueba. Si la partida se cargara antes, `simular()` vería la hora real, que
	# para una partida adelantada con F8 está en el pasado: "el reloj fue para
	# atrás", y la criatura congelada.
	_cargar_mundo()
	_cargar_o_nacer()
	semilla_mundo = core.semilla_del_mundo()

	_reloj = Timer.new()
	_reloj.wait_time = INTERVALO_CONSULTA
	_reloj.timeout.connect(_consultar)
	_reloj.autostart = true
	add_child(_reloj)
	return true


## Solo el núcleo y la tipografía, sin cargar ninguna partida.
##
## Es lo que necesita la pantalla de título: dibujar con la fuente del juego y
## saber si hay una partida guardada, ANTES de decidir si se continúa o se
## empieza otra. Cargar la partida ahí obligaría a descargarla si el jugador
## elige "Nueva partida".
func preparar() -> bool:
	if core != null:
		return true
	if not ClassDB.class_exists("PetBitsCore"):
		return false

	core = ClassDB.instantiate("PetBitsCore")

	# La tipografía va antes que nada: se instala como fuente por defecto del
	# motor, así que cualquier pantalla que se arme después ya nace con ella.
	# Hacerlo al revés dejaría el primer cuadro dibujado con la fuente de Godot.
	fuente = Tipografia.instalar(core)
	tam_fuente = int(core.fuente_metricas()["alto"])
	return true


func hay_juego() -> bool:
	return core != null


## ¿Hay una partida guardada para continuar?
func hay_partida_guardada() -> bool:
	return FileAccess.file_exists(ruta_save)


## Aparta la partida actual para empezar otra. NO BORRA NADA.
##
## Es la misma regla que la cuarentena: la partida de alguien no la borra el
## programa. Se le agrega la fecha al nombre para que apartar dos veces no pise
## la primera. Devuelve false si no se pudo apartar el save — y en ese caso NO
## hay que empezar otra partida, porque escribiría encima.
##
## Se llama ANTES de `iniciar()`.
func archivar_partida() -> bool:
	var sello := Time.get_datetime_string_from_system(false, true)
	sello = sello.replace(":", "-").replace(" ", "_")
	var bien := true
	for par in [[ruta_save, "partida"], [ruta_mundo, "mundo"]]:
		var origen: String = par[0]
		if not FileAccess.file_exists(origen):
			continue
		var destino := origen.get_base_dir().path_join("%s.anterior-%s.json" % [par[1], sello])
		var err := DirAccess.rename_absolute(
			ProjectSettings.globalize_path(origen), ProjectSettings.globalize_path(destino)
		)
		if err != OK and par[1] == "partida":
			bien = false
	return bien


# ---------------------------------------------------------------------------
# Guardado
# ---------------------------------------------------------------------------

func _cargar_o_nacer() -> void:
	# Si falta el save pero quedó el temporal, es que el juego se cortó justo
	# entre borrar el viejo y mover el nuevo — la única ventana que deja
	# `_escribir_entero()`. El temporal está completo: se recupera.
	var temporal := ruta_save + ".tmp"
	if not FileAccess.file_exists(ruta_save) and FileAccess.file_exists(temporal):
		DirAccess.rename_absolute(
			ProjectSettings.globalize_path(temporal), ProjectSettings.globalize_path(ruta_save)
		)

	if not FileAccess.file_exists(ruta_save):
		_nacer_nueva()
		return

	var archivo := FileAccess.open(ruta_save, FileAccess.READ)
	if archivo == null:
		# El archivo ESTÁ —`file_exists` dijo que sí— y no se pudo abrir. No
		# sabemos qué tiene adentro, así que lo último que hay que hacer es
		# escribirle encima.
		#
		# Esta rama llamaba derecho a `_nacer_nueva()`, que termina en `guardar()`,
		# que abre el archivo con WRITE — y WRITE trunca. Una partida de meses se
		# perdía por un lock de cinco minutos, y sin dejar ningún `.rota.json`,
		# que es justo lo que la constante de más arriba promete que no pasa.
		anotar("No se pudo abrir la partida guardada.", "aviso")
		_empezar_sin_pisar()
		return

	var texto := archivo.get_as_text()
	archivo.close()

	var r: Dictionary = core.cargar(texto)
	if not r["ok"]:
		# Acá sí se apartaba el save — pero se ignoraba si el renombrado había
		# funcionado, y si fallaba se seguía a `_nacer_nueva()` igual. El mismo
		# agujero que la rama de arriba, un paso más tarde.
		anotar("La partida guardada no se pudo leer: %s" % r["mensaje"], "aviso")
		_empezar_sin_pisar()
		return

	# El tiempo corrió mientras el juego estaba cerrado. Esta es la llamada que
	# hace que la criatura haya vivido en serio durante la ausencia.
	var sim: Dictionary = core.simular(ahora_ms())
	anotar("Volviste.", "bien")
	if sim.get("ticks", 0) > 0:
		anotar_eventos(sim)


## Empieza de nuevo SIN pisar el save que no se pudo leer.
##
## El orden importa y es el único que no pierde datos:
##
##   1. Se intenta apartar el archivo. Si sale bien, el original está a salvo con
##      otro nombre y recién ahí se puede nacer de nuevo y guardar encima.
##   2. Si NO sale bien, se juega igual pero esta sesión no guarda nada. Un juego
##      que arranca y no guarda es una molestia; un juego que arranca borrando la
##      partida de alguien es otra cosa.
##
## Nacer primero y apartar después no sirve: `_nacer_nueva()` termina en
## `guardar()`, y para cuando el renombrado falle el archivo ya no está.
func _empezar_sin_pisar() -> void:
	if _apartar_el_ilegible():
		_nacer_nueva()
		return

	guardar_bloqueado = true
	anotar("No se pudo apartar la partida vieja, así que esta sesión no se guarda.", "aviso")
	anotar("Cerrá lo que esté usando %s y volvé a abrir el juego." % ruta_save, "tenue")
	_nacer_nueva()


## Corre el save ilegible a la cuarentena. Dice si lo logró.
##
## Va aparte de `_empezar_sin_pisar()` para que se pueda probar sola: el camino
## entero pasa por `iniciar()`, que es idempotente, así que un solo proceso puede
## ejercerlo una sola vez. El caso "la cuarentena SÍ se pudo escribir" queda
## afuera de ese único intento y necesita esta puerta.
func _apartar_el_ilegible() -> bool:
	var err := DirAccess.rename_absolute(
		ProjectSettings.globalize_path(ruta_save),
		ProjectSettings.globalize_path(ruta_cuarentena)
	)
	if err != OK:
		return false
	anotar("Se guardó una copia en %s por las dudas." % ruta_cuarentena, "tenue")
	return true


func _nacer_nueva() -> void:
	partida_nueva = true
	var seed: String = semilla_inicial if semilla_inicial != "" else core.seed_al_azar()
	core.nacer(seed, ahora_ms(), _tz_min())
	anotar("Nació recién.", "tenue")
	guardar()


## Lee el archivo del nativo. Si no está o está roto, se arranca de cero.
##
## No hay cuarentena para este: perder dónde estabas parado es una molestia, no
## una pérdida. Con el save compartido es al revés, y por eso ese sí se renombra.
func _cargar_mundo() -> void:
	if not FileAccess.file_exists(ruta_mundo):
		return

	var archivo := FileAccess.open(ruta_mundo, FileAccess.READ)
	if archivo == null:
		return
	var texto := archivo.get_as_text()
	archivo.close()

	var datos = JSON.parse_string(texto)
	if typeof(datos) != TYPE_DICTIONARY:
		anotar("El archivo del mundo no se pudo leer. Volvés al pueblo.", "aviso")
		return

	mapa = datos.get("mapa", "pueblo")
	donde = Vector2(float(datos.get("x", 0.0)), float(datos.get("y", 0.0)))
	venir_de = datos.get("venir_de", "")

	recolectado = {}
	for clave in datos.get("recolectado", []):
		recolectado[clave] = true

	desfase_ms = int(datos.get("desfase_ms", 0))
	objetivos_hechos = {}
	for id in datos.get("objetivos", []):
		objetivos_hechos[id] = true


func guardar_mundo() -> void:
	# Si la partida no arrancó, no hay nada que guardar y sí hay algo que perder.
	#
	# `Partida` es un autoload: se instancia con SOLO ABRIR una escena, la
	# mencione o no. Cinco escenas del proyecto no llaman nunca a `iniciar()`
	# —MapaAPng, RegionAPng, MedirLayout, VerificarDialogo y Arranque— y todas
	# terminan pasando por `_notification`, que llama acá.
	#
	# Sin este guard, regenerar el PNG del mapa te borraba dónde estabas parado y
	# todo lo que hubieras levantado del suelo: `recolectado` vale {} y `donde`
	# vale cero, así que se escribía un mundo vacío encima del tuyo. `guardar()`
	# ya tiene el mismo guard; acá faltaba.
	if not _cargada:
		return

	var claves: Array = recolectado.keys()
	# Si se pasó del tope, se tiran las más viejas. `keys()` conserva el orden de
	# inserción en GDScript, así que las primeras son las primeras que se
	# levantaron.
	if claves.size() > MAXIMO_RECOLECTADO:
		claves = claves.slice(claves.size() - MAXIMO_RECOLECTADO)

	_escribir_entero(ruta_mundo, JSON.stringify({
		"mapa": mapa,
		"x": donde.x,
		"y": donde.y,
		"venir_de": venir_de,
		"recolectado": claves,
		"desfase_ms": desfase_ms,
		"objetivos": objetivos_hechos.keys(),
	}))


## ¿Ya se levantó lo que había en esa celda?
func ya_recolectado(celda: Vector2i) -> bool:
	return recolectado.has("%d,%d" % [celda.x, celda.y])


func marcar_recolectado(celda: Vector2i) -> void:
	recolectado["%d,%d" % [celda.x, celda.y]] = true


func guardar() -> void:
	if not _cargada:
		return
	# La única escritura sobre el save compartido de todo el programa. Si esta
	# sesión arrancó sin poder leer ni apartar el archivo de antes, acá es donde
	# se frena.
	if guardar_bloqueado:
		return
	var texto: String = core.guardar(ahora_ms())
	if texto == "":
		return
	_escribir_entero(ruta_save, texto)


## Escribe un archivo entero sin dejarlo nunca a medias.
##
## Se escribe a un temporal y recién cuando está completo se pone en el lugar
## del bueno. Abrir el archivo de verdad con WRITE lo trunca a cero en el acto:
## un corte de luz entre ese momento y el `close()` dejaba la partida vacía.
## Con el temporal, en el peor caso queda el archivo viejo — o, si el corte cae
## justo entre borrar el viejo y mover el nuevo, queda el `.tmp`, que
## `_cargar_o_nacer()` sabe recuperar.
func _escribir_entero(ruta: String, texto: String) -> void:
	# LA RED: sin ventana, jamás los archivos de verdad.
	#
	# El juego nunca se juega headless. Todo lo que corre headless es una
	# herramienta o un test, y cada uno se supone que desvía las rutas antes de
	# `iniciar()`. Ya fallaron tres veces de tres formas distintas: un test que
	# desviaba después de iniciar, `verificar_fuente` que no desviaba nada, y
	# `medir_layout`, que desviaba el save pero no el mundo y escribió el
	# `mundo.json` de verdad en cada corrida de la suite durante semanas.
	#
	# Revisar cada herramienta no alcanzó. Esto hace que la próxima que se olvide
	# no pueda romper nada: si no hay ventana y la ruta es la real, no se escribe.
	if DisplayServer.get_name() == "headless" and ruta in [RUTA_SAVE, RUTA_MUNDO]:
		push_warning("Partida: sin ventana no se escribe %s. ¿Faltó desviar la ruta?" % ruta)
		return

	var temporal := ruta + ".tmp"
	var archivo := FileAccess.open(temporal, FileAccess.WRITE)
	if archivo == null:
		return
	archivo.store_string(texto)
	archivo.close()
	DirAccess.rename_absolute(
		ProjectSettings.globalize_path(temporal), ProjectSettings.globalize_path(ruta)
	)


## Guardar al cerrar la ventana.
##
## Sin esto se perdería lo hecho desde el último tick guardado.
##
## (Acá decía que "no hay estado parcial posible: o está el archivo viejo o está
## el nuevo". No era cierto: `guardar()` abría el archivo con WRITE, que lo
## trunca antes de escribir. Ahora sí lo es — ver `_escribir_entero()`.)
##
## Vive en el autoload y no en la pantalla porque la pantalla puede no ser la que
## estaba abierta: si cerrás la ventana estando en el mapa, la partida se guarda
## igual.
func _notification(que: int) -> void:
	if que == NOTIFICATION_WM_CLOSE_REQUEST or que == NOTIFICATION_PREDELETE:
		if not guardar_al_salir:
			return
		guardar()
		guardar_mundo()


# ---------------------------------------------------------------------------
# Tiempo
# ---------------------------------------------------------------------------

## El reloj del sistema en milisegundos.
##
## Godot lo da en segundos y como float. Se redondea acá para que al C++ le
## llegue un entero: la simulación cuenta ticks de un minuto y una parte decimal
## en los milisegundos no aporta nada y sí puede correr una frontera.
func ahora_ms() -> int:
	return int(Time.get_unix_time_from_system()) * 1000 + desfase_ms


## La tecla de prueba: F8 adelanta seis horas.
##
## Solo en builds de debug, que es lo que se le pasa al tester. En un export de
## release `OS.is_debug_build()` es falso y la tecla no existe.
##
## `_input` y no `_unhandled_input`: tiene que andar en cualquier pantalla,
## aunque la pantalla se quede con las teclas que le interesan.
func _input(evento: InputEvent) -> void:
	if not OS.is_debug_build() or not _cargada:
		return
	if evento.is_action_pressed("tester_adelantar"):
		get_viewport().set_input_as_handled()
		adelantar(ADELANTO_PRUEBA_MS)


## Pasa el tiempo como si el juego hubiera estado cerrado.
##
## No es un atajo de la simulación: es exactamente lo que pasa al volver después
## de seis horas. `simular()` corre los mismos ticks, en el mismo orden, con las
## mismas reglas.
func adelantar(ms: int) -> void:
	desfase_ms += ms
	var r: Dictionary = core.simular(ahora_ms())
	anotar("→ Pasaron %d horas (tecla de prueba)." % int(ms / 3600000), "tenue")
	_revisar_regreso()
	if r.get("ticks", 0) > 0:
		anotar_eventos(r)
		avanzo.emit(r)
	guardar()
	guardar_mundo()
	cambio.emit()


## Minutos de desfasaje horario respecto de UTC.
##
## Se lee UNA vez, al nacer, y después vive dentro del estado de la criatura. Si
## se leyera en cada tick, mudarse de zona horaria movería la hora local de ticks
## ya procesados y rompería el invariante de que simular por pedazos da lo mismo
## que de corrido.
func _tz_min() -> int:
	return int(Time.get_time_zone_from_system().get("bias", 0))


func _consultar() -> void:
	if core == null:
		return

	var r: Dictionary = core.simular(ahora_ms())
	_revisar_regreso()

	if r.get("ticks", 0) > 0:
		anotar_eventos(r)
		avanzo.emit(r)
		# Se guarda solo cuando el tiempo avanzó de verdad. El reloj consulta una
		# vez por segundo y un tick dura un minuto: escribir en cada consulta
		# serían sesenta escrituras al pedo por cada una que sirve.
		guardar()

	cambio.emit()


## ¿Volvió de la expedición?
##
## Se pregunta en cada consulta y no con un temporizador propio: el juego puede
## haber estado cerrado durante toda la salida, así que "volvió" no es un evento
## que ocurra mientras mirás, es una condición que se comprueba.
func _revisar_regreso() -> void:
	var r: Dictionary = core.recibir(ahora_ms())
	if not r.get("volvio", false):
		return

	anotar("Volvió %s. %s" % [r["destino"], r["mensaje"]], "bien")
	if r["semilla"] != "":
		anotar("Encontró una semilla: %s" % r["semilla"], "raro")
	volvio.emit(r)
	guardar()


# ---------------------------------------------------------------------------
# Acciones
# ---------------------------------------------------------------------------

## Se pone al día ANTES de actuar, igual que `catchUp()` en la web.
##
## Si no, la acción se aplicaría sobre un estado viejo y el tiempo transcurrido
## se descontaría después, pisándola. Con el reloj consultando cada segundo casi
## nunca hay ticks pendientes, pero "casi nunca" no es una garantía.
##
## Devuelve { ok, mensaje } tal como lo dio el C++, para que quien llamó pueda
## mostrarlo donde corresponda: el registro en PetView, el cartel en el mapa.
func actuar(accion: Callable) -> Dictionary:
	if core == null:
		return {"ok": false, "mensaje": "No hay juego."}

	core.simular(ahora_ms())
	var r: Dictionary = accion.call()

	if r.get("ok", false):
		guardar()
	cambio.emit()
	return r


# ---------------------------------------------------------------------------
# Objetivos
# ---------------------------------------------------------------------------

## El primer objetivo sin cumplir, o "" si ya están todos.
##
## Dos se deducen del estado en vez de marcarse, porque se pueden cumplir por más
## de un camino: una semilla se consigue caminando o de una expedición, y una
## segunda criatura se tiene incubando o cruzando. Marcarlos en un solo lugar
## dejaría al otro camino con el objetivo trabado.
func objetivo_actual() -> String:
	for id in OBJETIVOS:
		if not _cumplido(id):
			return id
	return ""


func _cumplido(id: String) -> bool:
	if objetivos_hechos.has(id):
		return true
	if core == null or not _cargada:
		return false
	match id:
		"semilla":
			return core.semillas().size() > 0 or _cantidad_de_criaturas() >= 2
		"incubar":
			return _cantidad_de_criaturas() >= 2
	return false


func _cantidad_de_criaturas() -> int:
	return core.criaturas(ahora_ms()).size()


## Anota que algo pasó. Si era el objetivo a la vista, avisa.
##
## Se puede llamar de más: marcar algo ya marcado no hace nada. Eso deja a quien
## llama despreocuparse de si el objetivo era el actual o no.
func marcar(id: String) -> void:
	var era_el_actual := objetivo_actual() == id
	if objetivos_hechos.has(id):
		return
	objetivos_hechos[id] = true
	guardar_mundo()
	if era_el_actual:
		objetivo_cumplido.emit(id)
	cambio.emit()


# ---------------------------------------------------------------------------
# Bitácora
# ---------------------------------------------------------------------------

func anotar(texto: String, tono: String = "normal") -> void:
	bitacora.append({"texto": texto, "tono": tono})
	if bitacora.size() > MAXIMO_BITACORA:
		bitacora = bitacora.slice(bitacora.size() - MAXIMO_BITACORA)
	nota.emit(texto, tono)


func anotar_eventos(r: Dictionary) -> void:
	for ev in r["eventos"]:
		var tono := "tenue"
		if ev["tipo"] == "evolucion":
			tono = "bien"
		elif ev["tipo"] in ["salud", "letargo"]:
			tono = "alerta"
		anotar(ev["texto"], tono)

	if r.get("omitidos", 0) > 0:
		anotar("(y %d cosas más)" % r["omitidos"], "tenue")
