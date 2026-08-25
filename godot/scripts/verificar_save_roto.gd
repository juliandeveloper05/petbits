## verificar_save_roto.gd
##
## Comprueba que un save que no se pudo leer NUNCA se pise.
##
##   godot --headless --path godot res://scenes/VerificarSaveRoto.tscn
##
## ---
##
## POR QUÉ ESTO TIENE ARNÉS PROPIO.
##
## Lo que se prueba es `Partida.iniciar()`, y `iniciar()` es idempotente: la
## segunda llamada devuelve sin hacer nada. Así que solo se puede ejercer el
## camino de carga UNA vez por proceso, y este caso necesita ese camino entero —
## no alcanza con llamar a los helpers de adentro.
##
## ---
##
## QUÉ SE ESTÁ CUIDANDO.
##
## `_cargar_o_nacer()` tenía dos salidas de error y solo una apartaba el archivo.
## Si `FileAccess.open(..., READ)` devolvía null —el save existe pero está
## lockeado por otra copia del juego, o el antivirus lo está mirando en el
## arranque— se llamaba derecho a `_nacer_nueva()`, que termina en `guardar()`,
## que abre con WRITE. Y WRITE trunca.
##
## O sea: una partida de meses se perdía por un lock de cinco minutos, y sin
## dejar ningún `partida.rota.json`. La otra rama —la del JSON ilegible— sí
## renombraba, pero ignoraba si el renombrado había fallado y seguía igual.
##
## El caso que se monta acá es el segundo, porque es el que se puede montar sin
## pedirle al sistema operativo que trabe un archivo: se apunta la cuarentena a
## un directorio que no existe, con lo cual `rename_absolute` falla, y se mira
## qué le pasa al archivo original.
##
## La afirmación que importa es la última: después de todo esto, el archivo tiene
## que estar **byte por byte** como estaba.

extends Node

const RUTA := "user://verificacion_save_roto.json"
const CUARENTENA_IMPOSIBLE := "user://no_existe_este_directorio/apartado.json"
const RUTA_MUNDO := "user://verificacion_save_roto_mundo.json"

## Un save sintáctica y semánticamente inválido, para que `core.cargar()` falle.
const ROTO := '{"version":5,"criaturas":[{"esto":'

var _fallas := 0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	print("\nPetBits — un save que no se puede leer no se pisa\n")

	_limpiar()

	# El save "de antes", el que hay que no perder.
	var f := FileAccess.open(RUTA, FileAccess.WRITE)
	f.store_string(ROTO)
	f.close()

	# Los seams van ANTES de iniciar(), siempre. Puestos después no tienen
	# efecto y el test escribe el save de verdad del que lo corre — ya pasó.
	Partida.guardar_al_salir = false
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = CUARENTENA_IMPOSIBLE
	Partida.ruta_mundo = RUTA_MUNDO
	Partida.semilla_inicial = "A3F0-91C4-77BE-2D08"

	var arranco: bool = Partida.iniciar()

	# 1. El juego arranca igual. Que no se pueda leer la partida vieja no puede
	#    dejar a nadie sin juego.
	_afirmar(arranco, "el juego arranca igual")
	_afirmar(Partida.core != null, "y hay un núcleo")

	# 2. Esta sesión NO guarda. Es lo que impide el destrozo.
	_afirmar(Partida.guardar_bloqueado, "la sesión queda con el guardado bloqueado")

	# 3. Nada quedó en la cuarentena, porque no se pudo escribir ahí.
	_afirmar(
		not FileAccess.file_exists(CUARENTENA_IMPOSIBLE),
		"no hay cuarentena (el renombrado falló, que es el caso que se monta)"
	)

	# 4. Y lo que de verdad importa: el archivo original sigue ahí, igual.
	_afirmar(FileAccess.file_exists(RUTA), "el save de antes sigue existiendo")
	_afirmar(_leer(RUTA) == ROTO, "y está byte por byte como estaba")

	# 5. Guardar a mano tampoco lo pisa. `_nacer_nueva()` ya llamó a `guardar()`
	#    una vez; esto comprueba que la protección no era solo de ese primer
	#    llamado sino de todos.
	Partida.guardar()
	_afirmar(_leer(RUTA) == ROTO, "y guardar() explícito tampoco lo pisa")

	# 6. La otra mitad del trato: con una cuarentena que SÍ se puede escribir, el
	#    archivo se aparta de verdad. Sin esto, un `_empezar_sin_pisar()` que
	#    nunca renombrara nada pasaría las cinco afirmaciones de arriba.
	_probar_que_la_cuarentena_funciona()

	_limpiar()

	if _fallas == 0:
		print("\nTodo bien: un save ilegible no se pierde.")
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


## Con una cuarentena escribible, el save ilegible se aparta en vez de perderse.
func _probar_que_la_cuarentena_funciona() -> void:
	var origen := "user://verificacion_save_roto_2.json"
	var destino := "user://verificacion_save_roto_2.rota.json"
	for r in [origen, destino]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))

	var f := FileAccess.open(origen, FileAccess.WRITE)
	f.store_string(ROTO)
	f.close()

	Partida.ruta_save = origen
	Partida.ruta_cuarentena = destino
	Partida.guardar_bloqueado = false

	var ok: bool = Partida._apartar_el_ilegible()

	_afirmar(ok, "con cuarentena escribible, el apartado funciona")
	_afirmar(not FileAccess.file_exists(origen), "el original se movió")
	_afirmar(FileAccess.file_exists(destino), "y está en la cuarentena")
	_afirmar(_leer(destino) == ROTO, "con su contenido intacto")
	_afirmar(not Partida.guardar_bloqueado, "y el guardado NO queda bloqueado")

	for r in [origen, destino]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))


func _leer(ruta: String) -> String:
	var f := FileAccess.open(ruta, FileAccess.READ)
	if f == null:
		return ""
	var t := f.get_as_text()
	f.close()
	return t


func _limpiar() -> void:
	for r in [RUTA, RUTA_MUNDO, CUARENTENA_IMPOSIBLE]:
		DirAccess.remove_absolute(ProjectSettings.globalize_path(r))


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		print("  ok   %s" % que)
	else:
		print("  FALLA %s" % que)
		_fallas += 1
