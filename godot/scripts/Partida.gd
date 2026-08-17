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


## Prepara la partida. Idempotente: llamarla dos veces no hace nada la segunda.
##
## Devuelve false si la GDExtension no cargó, que es el único modo de falla que
## las pantallas necesitan distinguir — sin C++ no hay juego que mostrar.
func iniciar() -> bool:
	if _iniciada:
		return core != null
	_iniciada = true

	if not ClassDB.class_exists("PetBitsCore"):
		return false

	core = ClassDB.instantiate("PetBitsCore")

	# La tipografía va antes que nada: se instala como fuente por defecto del
	# motor, así que cualquier pantalla que se arme después ya nace con ella.
	# Hacerlo al revés dejaría el primer cuadro dibujado con la fuente de Godot.
	fuente = Tipografia.instalar(core)
	tam_fuente = int(core.fuente_metricas()["alto"])

	_cargar_o_nacer()

	_reloj = Timer.new()
	_reloj.wait_time = INTERVALO_CONSULTA
	_reloj.timeout.connect(_consultar)
	_reloj.autostart = true
	add_child(_reloj)
	return true


func hay_juego() -> bool:
	return core != null


# ---------------------------------------------------------------------------
# Guardado
# ---------------------------------------------------------------------------

func _cargar_o_nacer() -> void:
	if not FileAccess.file_exists(ruta_save):
		_nacer_nueva()
		return

	var archivo := FileAccess.open(ruta_save, FileAccess.READ)
	if archivo == null:
		anotar("No se pudo abrir la partida guardada. Empezamos de nuevo.", "aviso")
		_nacer_nueva()
		return

	var texto := archivo.get_as_text()
	archivo.close()

	var r: Dictionary = core.cargar(texto)
	if not r["ok"]:
		anotar("La partida guardada no se pudo leer: %s" % r["mensaje"], "aviso")
		anotar("Se guardó una copia en %s por las dudas." % ruta_cuarentena, "tenue")
		DirAccess.rename_absolute(
			ProjectSettings.globalize_path(ruta_save),
			ProjectSettings.globalize_path(ruta_cuarentena)
		)
		_nacer_nueva()
		return

	# El tiempo corrió mientras el juego estaba cerrado. Esta es la llamada que
	# hace que la criatura haya vivido en serio durante la ausencia.
	var sim: Dictionary = core.simular(ahora_ms())
	anotar("Volviste.", "bien")
	if sim.get("ticks", 0) > 0:
		anotar_eventos(sim)


func _nacer_nueva() -> void:
	core.nacer(core.seed_al_azar(), ahora_ms(), _tz_min())
	anotar("Nació recién.", "tenue")
	guardar()


func guardar() -> void:
	if core == null:
		return
	var texto: String = core.guardar(ahora_ms())
	if texto == "":
		return
	var archivo := FileAccess.open(ruta_save, FileAccess.WRITE)
	if archivo == null:
		return
	archivo.store_string(texto)
	archivo.close()


## Guardar al cerrar la ventana.
##
## Sin esto se perdería lo hecho desde el último tick guardado. El juego escribe
## el archivo entero cada vez —son un par de kilobytes— así que no hay estado
## parcial posible: o está el archivo viejo o está el nuevo.
##
## Vive en el autoload y no en la pantalla porque la pantalla puede no ser la que
## estaba abierta: si cerrás la ventana estando en el mapa, la partida se guarda
## igual.
func _notification(que: int) -> void:
	if que == NOTIFICATION_WM_CLOSE_REQUEST or que == NOTIFICATION_PREDELETE:
		guardar()


# ---------------------------------------------------------------------------
# Tiempo
# ---------------------------------------------------------------------------

## El reloj del sistema en milisegundos.
##
## Godot lo da en segundos y como float. Se redondea acá para que al C++ le
## llegue un entero: la simulación cuenta ticks de un minuto y una parte decimal
## en los milisegundos no aporta nada y sí puede correr una frontera.
func ahora_ms() -> int:
	return int(Time.get_unix_time_from_system()) * 1000


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
