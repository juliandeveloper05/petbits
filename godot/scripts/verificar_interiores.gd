## verificar_interiores.gd
##
## Comprueba que el criadero y el codex sean lugares donde pasa algo.
##
##   godot --headless --path godot res://scenes/VerificarInteriores.tscn
##
## ---
##
## LO QUE ESTO CONTESTA.
##
## Que las puertas lleven a algún lado, que las paredes frenen, que salir te deje
## en la puerta por la que entraste y no en el medio de la plaza, y —lo que de
## verdad importa— que cruzar dos criaturas produzca una tercera que quede
## escrita en el archivo.
##
## La cruza es la parte más fácil de dejar a medias sin notarlo. Se puede armar
## la sala entera, con sus pedestales y su alfombra, y que apretar Enter no haga
## nada más que mostrar un cartel: se ve idéntica a la versión que funciona.
##
## Por eso el test incuba una segunda criatura primero. Sin dos adultas no hay
## cruza posible, y un test que solo comprueba el mensaje de "hace falta otra
## criatura" estaría midiendo el camino que NO importa.
##
## Corre contra un save aparte. Un test que juegue con tu partida no es un test.

extends Node

const RUTA := "user://verificacion_interiores.json"
const Mapas = preload("res://scripts/Mapas.gd")

var _fallas := 0


func _ready() -> void:
	if not ClassDB.class_exists("PetBitsCore"):
		print("La GDExtension no cargó.")
		get_tree().quit(1)
		return

	DirAccess.remove_absolute(ProjectSettings.globalize_path(RUTA))
	Partida.ruta_save = RUTA
	Partida.ruta_cuarentena = "user://verificacion_interiores.rota.json"

	print("\nPetBits — el criadero y el codex\n")

	if not Partida.iniciar():
		print("No arrancó la partida.")
		get_tree().quit(1)
		return

	await _correr()

	DirAccess.remove_absolute(ProjectSettings.globalize_path(RUTA))

	if _fallas == 0:
		print("\nTodo bien: los dos interiores se caminan y la cruza funciona.")
	else:
		print("\n%d falla(s)." % _fallas)
	get_tree().quit(_fallas)


func _correr() -> void:
	await get_tree().process_frame

	var mundo: Node2D = load("res://scenes/Mundo.tscn").instantiate()
	get_tree().root.add_child(mundo)
	await get_tree().process_frame
	await get_tree().process_frame

	_afirmar(mundo._mapa_id == "pueblo", "se arranca en el pueblo")

	# ---- Los tres mapas se generan ----------------------------------------
	#
	# Antes de caminar nada: una grilla con una fila corta o un índice de tile
	# fuera de rango no revienta, dibuja cualquier cosa.
	for id in Mapas.todos():
		var def = Mapas.script_de(id)
		var grilla: Array = def.generar()
		_afirmar(grilla.size() == def.ALTO, "%s tiene %d filas" % [id, def.ALTO])

		var bien := true
		for fila in grilla:
			if fila.size() != def.ANCHO:
				bien = false
			for tile in fila:
				if tile < 0 or tile >= Partida.core.cantidad_tiles():
					bien = false
		_afirmar(bien, "%s: todas las filas completas y con tiles que existen" % id)

		# La entrada tiene que caer sobre algo que se camine. Es el error más
		# fácil de cometer al mover una puerta: aparecés adentro de una pared y
		# no te podés mover para ningún lado.
		var celda := Vector2i(int(def.ENTRADA.x / 16), int(def.ENTRADA.y / 16))
		_afirmar(
			not Partida.core.tile_solido(grilla[celda.y][celda.x]),
			"%s: la entrada no cae sobre un tile sólido" % id
		)

		# Y todo punto interactivo tiene que ser alcanzable: al menos uno de sus
		# vecinos se camina.
		for punto in def.PUNTOS:
			var alcanzable := false
			var vecinos: Array[Vector2i] = [
				Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0)
			]
			for d in vecinos:
				var v: Vector2i = Vector2i(punto["x"], punto["y"]) + d
				if v.x < 0 or v.y < 0 or v.x >= def.ANCHO or v.y >= def.ALTO:
					continue
				if not Partida.core.tile_solido(grilla[v.y][v.x]):
					alcanzable = true
			_afirmar(alcanzable, "%s: se puede llegar a %s" % [id, punto["nombre"]])

	# ---- Entrar al criadero -----------------------------------------------
	var puerta := _punto_de(mundo, "puerta", "criadero")
	_afirmar(not puerta.is_empty(), "el pueblo tiene la puerta del criadero")

	mundo._criatura.position = Vector2((puerta["x"] + 0.5) * 16, (puerta["y"] + 0.5) * 16)
	mundo._mirar_alrededor()
	_afirmar(mundo._cerca.get("mapa", "") == "criadero", "pararse en la puerta la reconoce")

	mundo._usar()
	# El fundido es asincrónico: hay que dejarlo terminar antes de mirar.
	await _esperar_fundido()

	_afirmar(mundo._mapa_id == "criadero", "la puerta lleva al criadero")
	_afirmar(Partida.mapa == "criadero", "y la partida se acuerda de dónde estás")
	_afirmar(Partida.venir_de == "pueblo", "y de dónde venías")

	# Las paredes frenan y el piso se camina.
	_afirmar(mundo._choca(Vector2(8, 8)), "la pared del criadero frena")
	_afirmar(not mundo._choca(mundo._criatura.position), "la entrada del criadero se camina")

	# ---- Cruzar con una sola criatura -------------------------------------
	var pedestales := _punto_de(mundo, "cruzar", "")
	mundo._criatura.position = Vector2((pedestales["x"] + 0.5) * 16, (pedestales["y"] + 0.5) * 16)
	mundo._mirar_alrededor()
	_afirmar(mundo._cerca.get("tipo", "") == "cruzar", "los pedestales se reconocen")

	mundo._usar()
	_afirmar(mundo._caja.abierta(), "con una sola criatura contesta algo")

	var antes: int = Partida.core.criaturas(Partida.ahora_ms()).size()
	_afirmar(antes == 1, "y NO nació nadie (había %d)" % antes)
	mundo._caja.cerrar()

	# ---- Incubar una segunda y cruzar de verdad ----------------------------
	#
	# Las dos tienen que ser adultas, sanas y con vínculo, que es lo que pide la
	# regla — y se llega ahí criándolas de verdad, ver `_criar`.
	var r: Dictionary = Partida.core.incubar("FEDC-BA98-7654-3210", Partida.ahora_ms(), -180)
	_afirmar(r["ok"], "se puede incubar una semilla: %s" % r["mensaje"])
	_afirmar(
		Partida.core.criaturas(Partida.ahora_ms()).size() == 2,
		"ahora hay dos criaturas"
	)

	_volver_adultas()

	var listas: Array = []
	var porques: Array = []
	for c in Partida.core.criaturas(Partida.ahora_ms()):
		if c["puede_cruzar"]:
			listas.append(c)
		else:
			porques.append("%s: %s" % [c["seed"], c["motivo"]])
	_afirmar(
		listas.size() == 2,
		"las dos pueden cruzar (%d pudieron; %s)" % [listas.size(), " | ".join(porques)]
	)

	mundo._usar()
	var despues: Array = Partida.core.criaturas(Partida.ahora_ms())
	_afirmar(despues.size() == 3, "nació una tercera (hay %d)" % despues.size())
	_afirmar(mundo._caja.abierta(), "y lo cuenta por la caja de diálogo")

	# La cría es la activa, y sus padres quedaron en descanso.
	var activa := ""
	for c in despues:
		if c["activa"]:
			activa = c["id"]
	_afirmar(activa != "", "hay una criatura activa")

	var en_descanso := 0
	for c in despues:
		if not c["puede_cruzar"] and c["motivo"].begins_with("Necesita descansar"):
			en_descanso += 1
	_afirmar(en_descanso == 2, "los dos padres quedaron en descanso (%d)" % en_descanso)

	# ---- Y quedó escrito ---------------------------------------------------
	mundo._caja.cerrar()
	Partida.guardar()
	var f := FileAccess.open(RUTA, FileAccess.READ)
	_afirmar(f != null, "el save existe")
	if f != null:
		var otro: RefCounted = ClassDB.instantiate("PetBitsCore")
		var carga: Dictionary = otro.cargar(f.get_as_text())
		f.close()
		_afirmar(carga["ok"], "el save con tres criaturas se vuelve a leer")
		if carga["ok"]:
			_afirmar(
				otro.criaturas(Partida.ahora_ms()).size() == 3,
				"las tres criaturas sobrevivieron el archivo"
			)

	# ---- Salir: se vuelve a la puerta, no a la plaza ------------------------
	var salida := _punto_de(mundo, "puerta", "pueblo")
	mundo._criatura.position = Vector2((salida["x"] + 0.5) * 16, (salida["y"] + 0.5) * 16)
	mundo._mirar_alrededor()
	mundo._usar()
	await _esperar_fundido()

	_afirmar(mundo._mapa_id == "pueblo", "la puerta del criadero devuelve al pueblo")
	var cerca_de_la_puerta := (
		absi(int(mundo._criatura.position.x / 16) - puerta["x"]) <= 1
		and absi(int(mundo._criatura.position.y / 16) - puerta["y"]) <= 2
	)
	_afirmar(cerca_de_la_puerta, "salís por donde entraste, no en el medio de la plaza")

	# ---- El codex ----------------------------------------------------------
	var puerta_codex := _punto_de(mundo, "puerta", "codex")
	mundo._criatura.position = Vector2((puerta_codex["x"] + 0.5) * 16, (puerta_codex["y"] + 0.5) * 16)
	mundo._mirar_alrededor()
	mundo._usar()
	await _esperar_fundido()

	_afirmar(mundo._mapa_id == "codex", "la otra puerta lleva al codex")

	for categoria in ["linajes", "formas", "rarezas"]:
		var estante := _punto_de(mundo, "estante", categoria)
		_afirmar(not estante.is_empty(), "hay estante de %s" % categoria)
		if estante.is_empty():
			continue
		mundo._criatura.position = Vector2((estante["x"] + 0.5) * 16, (estante["y"] + 0.5) * 16)
		mundo._mirar_alrededor()
		mundo._usar()
		_afirmar(mundo._caja.abierta(), "el estante de %s cuenta algo" % categoria)
		mundo._caja.cerrar()

	mundo.queue_free()
	await get_tree().process_frame


## El primer punto de ese tipo, filtrando además por mapa/categoría si se pide.
func _punto_de(mundo: Node2D, tipo: String, detalle: String) -> Dictionary:
	for punto in mundo._def.PUNTOS:
		if punto["tipo"] != tipo:
			continue
		if detalle == "":
			return punto
		if punto.get("mapa", "") == detalle or punto.get("categoria", "") == detalle:
			return punto
	return {}


## Cría a las dos criaturas hasta que puedan cruzar.
##
## Las reglas piden adulta (cuatro días de ticks activos), salud por encima de 50
## y vínculo por encima de 20 — y el vínculo topa en 12 por día, así que hacen
## falta dos días de atención como mínimo. Eso es deliberado del lado del diseño:
## ata la cruza al bucle de cuidado en vez de convertirla en un atajo.
##
## Se cría de verdad en vez de escribirle el estado a mano. La tentación era
## agregarle al C++ un método que pusiera la criatura en adulta y listo, y eso
## habría sido un backdoor en el código de producción para que pasara un test:
## el día que la regla de elegibilidad cambie, el atajo seguiría funcionando y el
## test seguiría en verde sin probar nada.
##
## Además hay que acariciar antes de que pasen 48 horas sin atención, o entra en
## letargo y ahí tampoco puede cruzar. O sea que este bucle es, literalmente, la
## partida mínima que hace falta jugar para llegar al criadero.
func _criar(id: String) -> void:
	Partida.core.activar(id)

	# Se avanza hacia el FUTURO desde el instante en que nació.
	#
	# La primera versión de esto arrancaba siete días en el pasado y no avanzaba
	# un solo tick: la criatura acababa de nacer, así que su reloj ya estaba en
	# "ahora" y `simular` nunca va para atrás. El test decía "todavía no terminó
	# de crecer" y parecía un problema de la regla de elegibilidad.
	#
	# Que el reloj del juego quede adelantado no importa acá: es un save de
	# prueba que se borra al terminar, y la simulación aguanta el reloj corrido
	# hacia atrás sin perder nada — tiene su propio test de paridad.
	var ahora: int = Partida.ahora_ms()
	const DIA := 24 * 60 * 60 * 1000

	# Seis días alcanzan para llegar a adulta —son cuatro— y de paso el vínculo
	# topa en 12 por día, así que con dos ya pasa de 20.
	#
	# Pero no alcanza con acariciarla. La primera versión de este bucle solo
	# acariciaba y la criatura llegaba adulta, con vínculo 28... y salud CERO:
	# seis días sin comer. La simulación estaba haciendo bien su trabajo y el
	# test estaba jugando mal.
	#
	# Así que hay que jugar la partida entera, expediciones incluidas. La
	# despensa inicial no da para seis días, y el patio es justamente el destino
	# que no pide etapa ni cuesta energía porque si para conseguir comida hiciera
	# falta comida el jugador quedaría trabado. Este bucle es, sin quererlo, la
	# prueba de que esa economía cierra.
	const MEDIA_HORA := 30 * 60 * 1000
	for dia in range(7):
		var t: int = ahora + dia * DIA

		# A buscar comida, y a recibirla media hora después.
		Partida.core.simular(t)
		Partida.core.recibir(t)
		Partida.core.enviar("patio", t)
		Partida.core.simular(t + MEDIA_HORA)
		Partida.core.recibir(t + MEDIA_HORA)

		_dar_de_comer(t + MEDIA_HORA)
		Partida.core.acariciar(t + MEDIA_HORA)

		# Y de nuevo a media jornada, para no dejar 48 horas de hueco: ahí es
		# cuando entra en letargo, y en letargo tampoco puede cruzar.
		Partida.core.simular(t + DIA / 2)
		_dar_de_comer(t + DIA / 2)
		Partida.core.acariciar(t + DIA / 2)

	# Se imprime cómo quedó porque es información, no ruido: si mañana alguien
	# cambia el balance y la criatura llega a los seis días con salud 40, este
	# renglón lo dice antes de que el test falle por "no está lo bastante sana".
	var e: Dictionary = Partida.core.estado()
	print("    criada: %s · %s · vínculo %.0f · salud %.0f" % [
		e["seed"], e["etapa"], e["stats"]["vinculo"], e["stats"]["salud"]
	])


## Le da lo primero que haya en la despensa.
##
## Cuál da igual: lo que hace falta acá es que coma, no empujarla hacia una rama
## evolutiva. Que la dieta decida la forma adulta es otra mecánica y tiene sus
## propios tests.
func _dar_de_comer(cuando: int) -> void:
	for alimento in Partida.core.alimentos():
		if int(alimento["cantidad"]) > 0:
			Partida.core.alimentar(alimento["id"], cuando)
			return


func _volver_adultas() -> void:
	for c in Partida.core.criaturas(Partida.ahora_ms()):
		_criar(c["id"])


func _esperar_fundido() -> void:
	# El fundido dura FUNDIDO segundos de ida y otros tantos de vuelta, y va por
	# un Tween: hay que dejar correr cuadros de verdad, no solo uno.
	for i in 30:
		await get_tree().process_frame


func _afirmar(condicion: bool, que: String) -> void:
	if condicion:
		print("  ok   %s" % que)
	else:
		print("  FALLA %s" % que)
		_fallas += 1
