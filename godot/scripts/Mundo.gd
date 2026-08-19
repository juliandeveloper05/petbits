## Mundo.gd
##
## Un mapa caminable. Cuál, lo dice `Partida.mapa`.
##
## Hay dos clases, y la escena las trata casi igual: los INTERIORES son una
## grilla fija del tamaño de la pantalla, y el MUNDO es infinito. La única
## diferencia real es de dónde salen los tiles y si la cámara se mueve.
##
## ---
##
## ANTES ERA "EL PUEBLO" Y AHORA ES "UN MAPA".
##
## La diferencia parece de nombre y no lo es. Con el pueblo adentro, cada mapa
## nuevo habría sido una escena nueva con su copia de caminar, chocar, hablar y
## dibujar — cuatro cosas que no cambian entre un pueblo y un interior. Lo único
## que cambia de verdad es la grilla y qué hay en ella.
##
## Así que esta escena no sabe en qué mapa está. Le pide a `Mapas` el script del
## que corresponda, arma el tilemap con su grilla y despacha sus puntos por
## `tipo`. Agregar un mapa es agregar un archivo de datos.
##
## ---
##
## LA DECISIÓN DE DISEÑO QUE ORDENA TODO ESTO.
##
## Tres de las zonas del pueblo —el patio, el bosque, las ruinas— ya existen como
## mecánica: son los destinos de expedición, y tardan quince minutos, hora y
## media o cuatro horas de tiempo REAL. Ocurren mientras el juego está cerrado.
##
## Si caminar hasta el bosque tardara tres segundos, esa mecánica moriría: irías,
## agarrarías el botín y volverías, y la espera —que es medio corazón del juego—
## dejaría de existir. Por eso esas tres NO son mapas: son puertas donde la
## mandás.
##
## Las otras dos —el criadero y el codex— sí lo son, y por el motivo inverso: ahí
## no hay espera que preservar. Eran un menú, y un menú se puede reemplazar
## entero por un lugar sin perder nada.
##
## ---
##
## ES LA MISMA CRIATURA QUE EN PetView, NO UNA COPIA.
##
## Todo sale del autoload `Partida`: el core, el guardado, el reloj. Mandarla
## desde acá es exactamente la misma llamada que apretar el botón en la otra
## pantalla, con el mismo estado y el mismo archivo. Si esta escena creara su
## propio `PetBitsCore` —como hizo en su primera versión— el pueblo mostraría una
## criatura al azar y las expediciones no tendrían efecto sobre tu partida.
##
## ---
##
## El tilemap se arma por código, con el atlas que genera el C++. No hay ningún
## PNG dibujado a mano en el proyecto, y esto no iba a ser la excepción.

extends Node2D

const Mapas = preload("res://scripts/Mapas.gd")
const CajaDialogo = preload("res://scripts/CajaDialogo.gd")

const TILE := 16
const VELOCIDAD := 46.0

## Cuánto tarda el fundido al cambiar de mapa, en segundos, de ida.
##
## Corto a propósito. Un fundido largo se siente cinematográfico la primera vez y
## un peaje a partir de la tercera, y una puerta se cruza muchas veces.
const FUNDIDO := 0.14

## La paleta del mapa. Es la misma consola verde fósforo que PetView, pero acá el
## texto va sobre pasto y paredes en vez de sobre negro: los carteles llevan
## contorno oscuro y no fondo.
const TEXTO := Color("#d6e6d0")
const SOMBRA := Color("#0a0e0a")
const FOSFORO := Color("#9bbc0f")
const AVISO := Color("#ffc23d")

## Las capas de terreno, de abajo hacia arriba, más la de objetos al final.
##
## Antes era una sola. Con la grilla dual cada material se dibuja en su propia
## capa, transparente donde no está, y lo que se ve es la pila — que es lo que
## produce los bordes redondeados en vez de escalones de dieciséis píxeles.
var _capas: Array[TileMapLayer] = []
var _capa_objetos: TileMapLayer = null

## La forma del atlas, que la dice el C++.
var _layout := {}
var _criatura: Sprite2D = null
var _cartel: Label = null
var _estado: Label = null
var _caja: Control = null
var _velo: ColorRect = null
var _camara: Camera2D = null

## Los sprites de los NPC del mapa actual. Se rehacen en cada carga.
var _vecinos: Array[Sprite2D] = []

## Los chunks ya volcados en el tilemap, por coordenada de chunk.
##
## Se guardan para no volver a pintarlos. Volcar un chunk son mil veinticuatro
## `set_cell`, y repetir los nueve en cada paso sería un tirón por cuadro.
var _chunks := {}

## En qué chunk estaba la criatura la última vez que se miró.
var _chunk_actual := Vector2i(999999, 999999)

## Si ya se usó la posición guardada. Ver `_volver_donde_corresponde`.
var _restaurada := false

## Cuántos tiles de lado tiene un chunk. Lo dice el C++: tenerlo escrito de los
## dos lados es tenerlo mal de uno.
var _lado_chunk := 32

## El script del mapa en el que estamos, y su grilla ya generada.
var _mapa_id := ""
var _def = null
var _mapa: Array = []

## Sobre qué punto está parada, o {} si está en el medio del campo.
var _cerca: Dictionary = {}

## Cuando la caja está contando algo del criadero, con qué seguir al cerrarse.
var _pendiente := ""


func _ready() -> void:
	if not Partida.iniciar():
		_sin_extension()
		return

	_lado_chunk = Partida.core.lado_de_chunk()

	_construir_criatura()
	_construir_camara()
	_construir_carteles()
	_construir_caja()
	_construir_velo()

	_cargar_mapa(Partida.mapa, false)

	Partida.cambio.connect(_refrescar_estado)
	_refrescar_estado()
	set_process(true)


# ---------------------------------------------------------------------------
# Cargar un mapa
# ---------------------------------------------------------------------------

## Cambia de mapa sin cambiar de escena.
##
## Se rearma el tilemap en vez de instanciar otra escena porque los tres mapas
## usan exactamente el mismo código: lo único distinto es la grilla. Cambiar de
## escena obligaría además a volver a colgarse de `Partida` y a rearmar la caja
## de diálogo, y ninguna de las dos cosas tiene por qué enterarse de que cruzaste
## una puerta.
func _cargar_mapa(id: String, con_fundido: bool) -> void:
	if con_fundido:
		await _fundir(1.0)

	_mapa_id = id
	Partida.mapa = id
	_def = Mapas.script_de(id)
	_cerca = {}

	_rehacer_capa()
	if _infinito():
		# El mundo no se vuelca de una: se carga el anillo alrededor de donde
		# estés, y el resto aparece caminando.
		_mapa = []
		_chunk_actual = Vector2i(999999, 999999)
	else:
		_mapa = _def.generar()
		_volcar_grilla()

	_construir_vecinos()

	# Al entrar por una puerta aparecés en la entrada del mapa nuevo. La posición
	# no se recuerda a propósito: volver del criadero y reaparecer en la piedra
	# del criadero es lo que uno espera, y guardarla obligaría a decidir qué pasa
	# cuando el mapa cambia de forma.
	_criatura.position = _volver_donde_corresponde()
	if _infinito():
		_actualizar_chunks()
	_ubicar_camara(true)

	_caja.cerrar()
	_cartel.text = _texto_de_ayuda()

	if con_fundido:
		await _fundir(0.0)


## Dónde aparece la criatura al entrar a un mapa.
##
## Si venís del pueblo, en la entrada del interior. Si volvés al pueblo, sobre la
## puerta por la que saliste — no en el medio de la plaza, que sería teletransporte.
func _volver_donde_corresponde() -> Vector2:
	# Si hay una posición guardada de este mapa, esa manda: es donde dejaste de
	# jugar. Solo vale la primera vez que se carga el mapa en esta sesión —
	# después ya estás caminando y `Partida.donde` te sigue.
	if not _restaurada and Partida.mapa == _mapa_id and Partida.donde != Vector2.ZERO:
		_restaurada = true
		return Partida.donde

	if _mapa_id == "pueblo" and Partida.venir_de != "":
		for punto in _def.PUNTOS:
			if punto.get("mapa", "") == Partida.venir_de:
				# Un tile más abajo de la piedra: la piedra es sólida y aparecer
				# adentro de un tile sólido te deja trabado contra el borde.
				return Vector2((punto["x"] + 0.5) * TILE, (punto["y"] + 1.5) * TILE)
	return _def.ENTRADA


func _infinito() -> bool:
	return _def != null and _def.INFINITO


## Rearma la capa de tiles vacía. Lo que se vuelque encima depende del mapa.
func _rehacer_capa() -> void:
	for c in _capas:
		c.queue_free()
	_capas.clear()
	if _capa_objetos != null:
		_capa_objetos.queue_free()
	_chunks.clear()

	_layout = Partida.core.atlas_layout()

	var imagen: Image = Partida.core.atlas_tiles()
	var textura := ImageTexture.create_from_image(imagen)

	var fuente := TileSetAtlasSource.new()
	fuente.texture = textura
	fuente.texture_region_size = Vector2i(TILE, TILE)
	for fila in int(_layout["filas"]):
		for col in int(_layout["columnas"]):
			fuente.create_tile(Vector2i(col, fila))

	var conjunto := TileSet.new()
	conjunto.tile_size = Vector2i(TILE, TILE)
	conjunto.add_source(fuente, 0)

	# Una capa por material, en orden. El z de cada una lo da el orden de los
	# hijos, así que agregarlas de abajo hacia arriba ya las apila bien.
	for i in int(_layout["capas"]):
		var capa := TileMapLayer.new()
		capa.tile_set = conjunto
		# Nearest: sin esto el pixel art se ve borroso, igual que con el sprite.
		capa.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		# MEDIO TILE de corrimiento, y es toda la técnica. El tile que se dibuja
		# se apoya sobre el cruce de cuatro celdas del mundo en vez de sobre una,
		# y por eso puede mostrar la transición entre ellas.
		capa.position = Vector2(-TILE / 2.0, -TILE / 2.0)
		add_child(capa)
		move_child(capa, i)
		_capas.append(capa)

	# Y la de objetos, sin corrimiento: un árbol se para sobre SU celda.
	_capa_objetos = TileMapLayer.new()
	_capa_objetos.tile_set = conjunto
	_capa_objetos.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	add_child(_capa_objetos)
	move_child(_capa_objetos, _capas.size())


## Vuelca la grilla de un interior, que entra entera de una.
##
## Los interiores NO usan grilla dual: una sala no tiene costas, y sus cuatro
## tiles —piso, pared, alfombra, pedestal— se dibujan enteros. Van todos en la
## capa de objetos, que es la única sin corrimiento.
func _volcar_grilla() -> void:
	for y in _def.ALTO:
		for x in _def.ANCHO:
			var d: Dictionary = Partida.core.tile_de_grilla(_mapa[y][x])
			_capa_objetos.set_cell(Vector2i(x, y), 0, Vector2i(int(d["col"]), int(d["fila"])))


# ---------------------------------------------------------------------------
# Los chunks
# ---------------------------------------------------------------------------

## Mantiene cargado el anillo de 3 × 3 chunks alrededor de la criatura.
##
## Se llama en cada cuadro pero solo hace algo cuando cambia el chunk, que es
## cada 32 tiles caminados. Y cuando hace algo, vuelca únicamente los chunks
## nuevos: cruzar un borde trae tres, no nueve. Son 3072 celdas en vez de 9216, y
## esa diferencia es la que separa un tirón visible de uno que no se nota.
func _actualizar_chunks() -> void:
	var centro := _chunk_de(_criatura.position)
	if centro == _chunk_actual:
		return
	_chunk_actual = centro

	var semilla: String = Partida.semilla_mundo
	if semilla == "":
		return

	var quiero := {}
	for dy in [-1, 0, 1]:
		for dx in [-1, 0, 1]:
			var c := centro + Vector2i(dx, dy)
			quiero[c] = true
			if not _chunks.has(c):
				_volcar_chunk(semilla, c)
				_chunks[c] = true

	# Y se borran los que quedaron lejos. Sin esto el tilemap crece sin techo:
	# una caminata larga terminaría con decenas de miles de celdas puestas que
	# nadie va a mirar nunca más.
	for c in _chunks.keys():
		if quiero.has(c):
			continue
		_borrar_chunk(c)
		_chunks.erase(c)


func _volcar_chunk(semilla: String, c: Vector2i) -> void:
	for i in _capas.size():
		var datos: PackedByteArray = Partida.core.mundo_capa_chunk(semilla, c.x, c.y, i)
		if datos.is_empty():
			continue
		for y in _lado_chunk:
			for x in _lado_chunk:
				var m: int = datos[y * _lado_chunk + x]
				# La máscara cero es "esta capa no está acá". No se pinta: dejar
				# la celda vacía es lo que permite ver la capa de abajo, y pintar
				# el tile transparente costaría lo mismo sin ganar nada.
				if m == 0:
					continue
				_capas[i].set_cell(
					Vector2i(c.x * _lado_chunk + x, c.y * _lado_chunk + y), 0, Vector2i(m, i)
				)

	var objetos: PackedByteArray = Partida.core.mundo_objetos_chunk(semilla, c.x, c.y)
	if objetos.is_empty():
		return
	var fila: int = int(_layout["fila_objetos"])
	for y in _lado_chunk:
		for x in _lado_chunk:
			var col: int = objetos[y * _lado_chunk + x]
			if col == 255:
				continue
			_capa_objetos.set_cell(
				Vector2i(c.x * _lado_chunk + x, c.y * _lado_chunk + y), 0, Vector2i(col, fila)
			)


func _borrar_chunk(c: Vector2i) -> void:
	for y in _lado_chunk:
		for x in _lado_chunk:
			var celda := Vector2i(c.x * _lado_chunk + x, c.y * _lado_chunk + y)
			for capa in _capas:
				capa.erase_cell(celda)
			_capa_objetos.erase_cell(celda)


func _chunk_de(pos: Vector2) -> Vector2i:
	# Hacia abajo, no hacia el cero: con división entera el chunk 0 mediría el
	# doble y el mundo quedaría partido justo en el origen. Es la misma cuenta que
	# `chunkDe` del lado del C++.
	return Vector2i(
		int(floor(pos.x / (_lado_chunk * TILE))),
		int(floor(pos.y / (_lado_chunk * TILE)))
	)


# ---------------------------------------------------------------------------
# La cámara
# ---------------------------------------------------------------------------

## Hasta la Fase 3 no había ninguna: el mapa medía exactamente lo que la pantalla
## y no había nada que encuadrar. Un mundo infinito la vuelve obligatoria.
func _construir_camara() -> void:
	_camara = Camera2D.new()
	_camara.enabled = true
	# Suavizado corto. Sin él, la cámara pegada a la criatura hace que el mundo
	# tiemble con cada paso; con mucho, el personaje se despega del encuadre al
	# cambiar de dirección.
	_camara.position_smoothing_enabled = true
	_camara.position_smoothing_speed = 12.0
	add_child(_camara)


## Encuadra según el mapa.
##
## En un interior la cámara se queda quieta en el centro: la sala mide justo lo
## que la pantalla, y moverla solo mostraría el vacío de afuera. En el mundo
## sigue a la criatura.
func _ubicar_camara(de_golpe: bool) -> void:
	if _camara == null:
		return

	var destino: Vector2
	if _infinito():
		destino = _criatura.position
	else:
		destino = Vector2(_def.ANCHO * TILE, _def.ALTO * TILE) * 0.5

	if de_golpe:
		# Al entrar a un mapa la cámara no se desliza desde donde estaba: eso se
		# vería como un barrido por encima del mundo entre un lugar y otro.
		_camara.position_smoothing_enabled = false
		_camara.position = destino
		_camara.reset_smoothing()
		_camara.position_smoothing_enabled = true
	else:
		_camara.position = destino


# ---------------------------------------------------------------------------
# Construcción de la escena
# ---------------------------------------------------------------------------

## Dibuja a los NPC del mapa.
##
## Usan el mismo generador de sprites que tu criatura, con su propio seed fijo.
## No hay un dibujo aparte para "gente del pueblo": si el juego entero sale de
## semillas, los vecinos también, y así comparten paleta y grilla sin que nadie
## tenga que igualarlas a mano.
func _construir_vecinos() -> void:
	for v in _vecinos:
		v.queue_free()
	_vecinos.clear()

	for punto in _def.PUNTOS:
		if punto["tipo"] != "npc":
			continue
		var s := Sprite2D.new()
		# En etapa adulta y forma definida: un vecino que se ve como un bebé no
		# se lee como alguien que vive acá desde antes que vos.
		s.texture = ImageTexture.create_from_image(
			Partida.core.sprite(punto["seed"], "adulto", "guardian", false)
		)
		s.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		s.scale = Vector2(0.5, 0.5)
		s.position = Vector2((punto["x"] + 0.5) * TILE, (punto["y"] + 0.5) * TILE)
		add_child(s)
		_vecinos.append(s)


func _construir_criatura() -> void:
	_criatura = Sprite2D.new()
	_criatura.texture = ImageTexture.create_from_image(Partida.core.sprite_actual(false))
	_criatura.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	# El sprite es de 32×32 y los tiles de 16: a escala 1 la criatura ocuparía
	# cuatro tiles y taparía el mapa. A la mitad ocupa uno, que es la proporción
	# de un personaje de consola portátil.
	_criatura.scale = Vector2(0.5, 0.5)
	add_child(_criatura)


## Las dos líneas fijas: quién sos arriba, qué podés hacer abajo.
##
## No son la caja de diálogo y no tienen que serlo. Un aviso permanente y una
## conversación son cosas distintas: el primero es parte del decorado y se lee de
## reojo, la segunda interrumpe y pide respuesta.
## Las dos líneas y la caja cuelgan de la CÁMARA, no del mundo.
##
## Con la cámara quieta daba lo mismo. Ahora que se mueve, un cartel puesto en
## coordenadas del mundo se quedaría atrás en cuanto camines: la interfaz tiene
## que vivir en el espacio de la pantalla, y colgarla de la cámara es la forma
## más corta de decir eso.
func _construir_carteles() -> void:
	var vp := _viewport()
	_cartel = _etiqueta(Vector2(6 - vp.x / 2, vp.y / 2 - 18), TEXTO)
	_estado = _etiqueta(Vector2(6 - vp.x / 2, 4 - vp.y / 2), FOSFORO)


func _construir_caja() -> void:
	_caja = CajaDialogo.new()
	_camara.add_child(_caja)

	var vp := _viewport()
	_caja.size = Vector2(vp.x - 8, _caja.custom_minimum_size.y)
	_caja.position = Vector2(4 - vp.x / 2, vp.y / 2 - _caja.size.y - 4)
	_caja.termino.connect(_al_terminar_de_hablar)


## El velo negro del fundido entre mapas.
##
## Va al final de los hijos para quedar encima de todo, y con el ratón
## deshabilitado: un Control que no atrapa el ratón no le roba los clics a nada
## de abajo aunque esté tapándolo.
func _construir_velo() -> void:
	_velo = ColorRect.new()
	_velo.color = Color(0, 0, 0, 0)
	_velo.size = _viewport()
	_velo.position = -_viewport() * 0.5
	_velo.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_camara.add_child(_velo)


func _viewport() -> Vector2:
	return Vector2(
		ProjectSettings.get_setting("display/window/size/viewport_width"),
		ProjectSettings.get_setting("display/window/size/viewport_height")
	)


## Texto legible sobre cualquier tile.
##
## El contorno hace el trabajo de una caja de diálogo sin ocupar el lugar de una:
## sobre el pasto claro y sobre la sombra de las paredes se lee igual.
func _etiqueta(donde: Vector2, color: Color) -> Label:
	var etiqueta := Label.new()
	etiqueta.position = donde
	etiqueta.add_theme_font_size_override("font_size", Partida.tam_fuente)
	etiqueta.add_theme_color_override("font_color", color)
	etiqueta.add_theme_color_override("font_outline_color", SOMBRA)
	etiqueta.add_theme_constant_override("outline_size", 2)
	_camara.add_child(etiqueta)
	return etiqueta


func _fundir(hasta: float) -> void:
	var tween := create_tween()
	tween.tween_property(_velo, "color:a", hasta, FUNDIDO)
	await tween.finished


# ---------------------------------------------------------------------------
# Caminar
# ---------------------------------------------------------------------------

func _process(delta: float) -> void:
	if _criatura == null or _def == null:
		return

	# Mientras habla no se camina. Es la regla de todos los juegos del género y
	# no es capricho: si te pudieras ir mientras la caja escribe, el texto
	# quedaría diciéndole algo a nadie.
	if _caja != null and _caja.abierta():
		# Y el aviso de abajo se va: la caja se le apoya encima y quedarían dos
		# textos peleando por el mismo lugar.
		_cartel.visible = false
		return
	_cartel.visible = true

	var dir := Vector2(
		Input.get_axis("move_left", "move_right"),
		Input.get_axis("move_up", "move_down")
	)
	if dir == Vector2.ZERO:
		_mirar_alrededor()
		return

	# Normalizado: sin esto, ir en diagonal sería un 41% más rápido que ir
	# derecho, que es el bug de movimiento más viejo del mundo.
	var paso := dir.normalized() * VELOCIDAD * delta

	# Los dos ejes se prueban por separado. Moviéndolos juntos, rozar una esquina
	# frena el movimiento entero y el personaje se traba en las paredes en vez de
	# deslizarse.
	if not _choca(_criatura.position + Vector2(paso.x, 0)):
		_criatura.position.x += paso.x
	if not _choca(_criatura.position + Vector2(0, paso.y)):
		_criatura.position.y += paso.y

	Partida.donde = _criatura.position
	if _infinito():
		_actualizar_chunks()
	_ubicar_camara(false)
	_mirar_alrededor()


## ¿Ese punto cae sobre un tile sólido?
##
## Se prueban las cuatro esquinas de una caja más chica que el sprite, no el
## centro: con el centro solo, media criatura se mete adentro de la pared antes
## de que algo la frene.
func _choca(pos: Vector2) -> bool:
	const MEDIO := 5.0
	for dx in [-MEDIO, MEDIO]:
		for dy in [-MEDIO, MEDIO]:
			var celda := Vector2i(
				int(floor((pos.x + dx) / TILE)), int(floor((pos.y + dy) / TILE))
			)
			if _solido_en(celda):
				return true
	return false


## ¿Hay algo sólido en esa celda?
##
## En un interior el borde del mapa es una pared implícita; en el mundo no hay
## borde, así que la pregunta se le hace al generador. El `floor` de arriba y no
## un casteo a entero es por lo mismo: con coordenadas negativas, `int(-0.5)` da
## 0 y la criatura atravesaría el tile que está a su izquierda.
func _solido_en(celda: Vector2i) -> bool:
	if _infinito():
		return Partida.core.tile_solido(
			Partida.core.mundo_tile(Partida.semilla_mundo, celda.x, celda.y)
		)

	if celda.x < 0 or celda.y < 0 or celda.x >= _def.ANCHO or celda.y >= _def.ALTO:
		return true
	return Partida.core.tile_solido(_mapa[celda.y][celda.x])


## Si está parada sobre un punto, lo nombra y lo deja listo para usar.
func _mirar_alrededor() -> void:
	var celda := Vector2i(
		int(floor(_criatura.position.x / TILE)), int(floor(_criatura.position.y / TILE))
	)

	for punto in _def.PUNTOS:
		# Se acepta el tile del punto y sus vecinos: pararse EXACTO sobre una
		# celda de 16 píxeles con movimiento continuo es pedirle demasiado a
		# quien juega.
		if absi(celda.x - punto["x"]) <= 1 and absi(celda.y - punto["y"]) <= 1:
			if _cerca != punto:
				_cerca = punto
				_cartel.text = _anunciar(punto)
			return

	if not _cerca.is_empty():
		_cerca = {}

	if _infinito():
		_cartel.text = _texto_del_suelo(celda)
	else:
		_cartel.text = _texto_de_ayuda()


func _texto_de_ayuda() -> String:
	if _infinito():
		return "Flechas para caminar · Esc para volver"
	return "Flechas para caminar · Esc para salir"


## Qué dice el cartel al pararse sobre un punto.
##
## El motivo del rechazo lo da el C++ —"todavía está muy chica para ir tan
## lejos"— y se muestra tal cual. Es la misma regla que el tooltip de PetView,
## leída del mismo lado: acá no se decide nada, se pinta.
func _anunciar(punto: Dictionary) -> String:
	match punto["tipo"]:
		"puerta":
			return "%s — Enter para entrar" % punto["nombre"]
		"cruzar":
			return "Los pedestales — Enter para cruzar"
		"estante":
			return "%s — Enter para mirar" % punto["nombre"]
		"npc":
			return "%s — Enter para hablar" % punto["nombre"]
		"expedicion":
			for d in Partida.core.destinos():
				if d["id"] != punto["destino"]:
					continue
				if d["puede"]:
					return "%s — Enter" % punto["nombre"]
				return "%s — %s" % [punto["nombre"], d["motivo"]]
	return punto["nombre"]


# ---------------------------------------------------------------------------
# Entrada
# ---------------------------------------------------------------------------

## Enter y Esc, con la caja adelante de todo.
##
## La caja no ataja teclas por su cuenta: expone `abierta()` y `avanzar()` y acá
## se decide. Si se las robara, esta pantalla tendría que adivinar si la tecla le
## llegó o no, y esa duda aparece después como "a veces caminaba mientras
## hablaba".
func _unhandled_input(evento: InputEvent) -> void:
	if evento.is_action_pressed("action_confirm"):
		if _caja.abierta():
			_caja.avanzar()
		else:
			_usar()
		return

	if evento.is_action_pressed("action_cancel"):
		if _caja.abierta():
			_caja.cerrar()
		elif _mapa_id != "pueblo":
			# Desde un interior, Esc te devuelve al pueblo. Desde el pueblo, a
			# PetView. Es la misma tecla haciendo lo mismo —salir de acá— en dos
			# escalas.
			_ir_a("pueblo")
		else:
			get_tree().change_scene_to_file("res://scenes/PetView.tscn")


func _usar() -> void:
	if _cerca.is_empty():
		# Nada con qué interactuar: se prueba levantar algo del suelo. Es lo que
		# hace que Enter siempre signifique lo mismo —"usar lo que tengo
		# delante"— en vez de estar deshabilitado la mayor parte del tiempo.
		_recolectar()
		return

	match _cerca["tipo"]:
		"puerta":
			_ir_a(_cerca["mapa"])
		"expedicion":
			_enviar()
		"cruzar":
			_cruzar()
		"estante":
			_mirar_estante(_cerca["categoria"])
		"npc":
			_hablar(_cerca)


func _ir_a(mapa: String) -> void:
	Partida.venir_de = _mapa_id
	# Antes de cruzar la puerta: si el juego se cierra del otro lado, la posición
	# guardada tiene que ser la del mapa al que vas, no la de este.
	Partida.guardar_mundo()
	_cargar_mapa(mapa, true)


# ---------------------------------------------------------------------------
# Lo que se puede hacer en cada punto
# ---------------------------------------------------------------------------

func _enviar() -> void:
	var nombre: String = _cerca["nombre"]
	var destino: String = _cerca["destino"]

	# La descripción va primero y en su propia página: es la información con la
	# que el jugador decide, y mezclarla con el resultado en un solo bloque hace
	# que se lea una sola de las dos.
	for d in Partida.core.destinos():
		if d["id"] == destino:
			_caja.decir_ya("%s. %s" % [nombre, d["descripcion"]])
			break

	# Exactamente la misma llamada que el botón de PetView, sobre el mismo core.
	var r: Dictionary = Partida.actuar(func(): return Partida.core.enviar(destino, Partida.ahora_ms()))
	_caja.decir(r["mensaje"])

	# También va a la bitácora: si el jugador vuelve a PetView quiere ver ahí que
	# la mandó, no tener que acordarse de una caja que ya se cerró.
	if r["ok"]:
		Partida.anotar(r["mensaje"], "bien")


## La cruza.
##
## Elige automáticamente las dos primeras criaturas que PUEDEN cruzar, en vez de
## abrir un selector. Con seis criaturas como tope y casi siempre una o dos
## elegibles, un menú de selección sería una pantalla entera para una decisión
## que el juego casi nunca ofrece de verdad. Cuando haya colecciones grandes
## valdrá la pena; hoy sería ceremonia.
##
## Y cuando NO se puede, el motivo lo da el C++ y se muestra tal cual: es la
## misma regla que la web, leída del mismo lado.
func _cruzar() -> void:
	var ahora: int = Partida.ahora_ms()
	var elegibles: Array = []
	for c in Partida.core.criaturas(ahora):
		if c["puede_cruzar"]:
			elegibles.append(c)

	if elegibles.size() < 2:
		_caja.decir_ya(_por_que_no_se_puede_cruzar(ahora))
		return

	var r: Dictionary = Partida.actuar(func(): return Partida.core.cruzar(
		elegibles[0]["id"], elegibles[1]["id"], ahora
	))

	if not r["ok"]:
		_caja.decir_ya(r["mensaje"])
		return

	_caja.decir_ya("Nació. %s" % r["descripcion"])
	if int(r["mutaciones"]) > 0:
		_caja.decir("Mutaron %d bits del genoma." % int(r["mutaciones"]))
	_caja.decir("Es %s. Ahora es la que estás cuidando." % r["seed"])
	_contar_descubrimientos(r["descubrimientos"])

	Partida.anotar("Nació %s. %s" % [r["seed"], r["descripcion"]], "bien")
	_refrescar_sprite()


## Por qué no hay pareja, dicho de la forma más útil posible.
##
## Con una sola criatura el motivo real es que falta la otra, y repetir el motivo
## de la única que hay —"todavía no terminó de crecer"— sería contestar una
## pregunta que nadie hizo.
func _por_que_no_se_puede_cruzar(ahora: int) -> String:
	var todas: Array = Partida.core.criaturas(ahora)
	if todas.size() < 2:
		return "Hace falta otra criatura. Con una sola no hay cruza."

	for c in todas:
		if not c["puede_cruzar"]:
			return "%s no puede: %s" % [c["seed"], c["motivo"]]
	return "Todavía no."


## Le da a la caja todo lo que el vecino tiene para decir, de una.
##
## La caja ya sabe paginar y esperar Enter entre páginas, así que encolar las
## tres frases juntas da exactamente la conversación que uno espera. Meter acá
## una máquina de estados de diálogo sería reimplementar lo que la caja hace.
func _hablar(punto: Dictionary) -> void:
	var dice: Array = punto["dice"]
	if dice.is_empty():
		return
	_caja.decir_ya(dice[0])
	for i in range(1, dice.size()):
		_caja.decir(dice[i])


## Qué dice el cartel cuando estás en el campo, sin nada delante.
##
## El bioma va siempre: es la única forma de saber dónde estás en un mundo sin
## bordes ni nombres de lugar. Y si hay algo para levantar, lo dice — un hallazgo
## que no se anuncia es un hallazgo que nadie encuentra.
func _texto_del_suelo(celda: Vector2i) -> String:
	var semilla: String = Partida.semilla_mundo
	if semilla == "":
		return _texto_de_ayuda()

	var donde: String = Partida.core.mundo_bioma(semilla, celda.x, celda.y)

	var h: Dictionary = Partida.core.mundo_hallazgo(semilla, celda.x, celda.y)
	if h["tipo"] != "nada" and not Partida.ya_recolectado(celda):
		return "%s · %s — Enter para juntar" % [donde, h["nombre"]]

	return "%s · %d, %d" % [donde, celda.x, celda.y]


## Levanta lo que haya en el suelo.
##
## Lo que ya se levantó se recuerda en el archivo del mundo, no en el save
## compartido. Y se recuerda lo LEVANTADO y no lo que queda: en un mundo infinito
## lo que queda también lo es, así que la pregunta hay que darla vuelta.
func _recolectar() -> void:
	if not _infinito():
		return

	var semilla: String = Partida.semilla_mundo
	if semilla == "":
		return

	var celda := Vector2i(
		int(floor(_criatura.position.x / TILE)), int(floor(_criatura.position.y / TILE))
	)

	var h: Dictionary = Partida.core.mundo_hallazgo(semilla, celda.x, celda.y)
	if h["tipo"] == "nada":
		return

	if Partida.ya_recolectado(celda):
		_caja.decir_ya("Acá ya no queda nada. Va a volver a crecer.")
		return

	var r: Dictionary = Partida.actuar(func(): return Partida.core.recolectar(
		semilla, celda.x, celda.y
	))
	if not r["ok"]:
		return

	Partida.marcar_recolectado(celda)
	Partida.guardar_mundo()
	_caja.decir_ya(r["mensaje"])

	# Un hito es un hallazgo de verdad, así que va a la bitácora: al volver a
	# PetView tiene que quedar rastro de que encontraste algo.
	if h["tipo"] == "hito":
		Partida.anotar(r["mensaje"], "raro")


func _mirar_estante(categoria: String) -> void:
	var codex: Dictionary = Partida.core.codex()
	if codex.is_empty():
		_caja.decir_ya("No hay nada anotado todavía.")
		return

	var avance: Dictionary = codex["progreso"][categoria]
	var lista: Array = codex[categoria]

	_caja.decir_ya("%s: %d de %d." % [_cerca["nombre"], int(avance["vistos"]), int(avance["total"])])

	if lista.is_empty():
		_caja.decir("El estante está vacío. Todavía no encontraste ninguna.")
		return

	# Los nombres, no los ids: el codex se mira, no se consulta.
	var nombres: Array = []
	for item in lista:
		nombres.append(item["nombre"])
	_caja.decir(", ".join(nombres) + ".")


func _contar_descubrimientos(descubrimientos: Array) -> void:
	for d in descubrimientos:
		var texto := "Nuevo en el codex: %s." % d["nombre"]
		_caja.decir(texto)
		Partida.anotar(texto, "raro")


func _al_terminar_de_hablar() -> void:
	if _pendiente == "":
		return
	var destino := _pendiente
	_pendiente = ""
	_ir_a(destino)


# ---------------------------------------------------------------------------
# Refresco
# ---------------------------------------------------------------------------

func _refrescar_sprite() -> void:
	var imagen: Image = Partida.core.sprite_actual(false)
	if imagen != null and _criatura != null:
		_criatura.texture = ImageTexture.create_from_image(imagen)


## La línea de arriba: quién es y qué está haciendo.
##
## Cuando está de expedición el sprite se apaga a media luz en vez de esconderse.
## Esconderlo dejaría el mapa sin nada que mover y la pantalla muerta hasta que
## vuelva; a media luz se entiende que no está del todo acá, y se puede seguir
## caminando para ver dónde queda cada cosa.
func _refrescar_estado() -> void:
	if _criatura == null:
		return

	var e: Dictionary = Partida.core.estado()
	if e.is_empty():
		return

	var falta: int = Partida.core.falta_para_volver(Partida.ahora_ms())
	if falta > 0:
		# Minutos redondeados hacia arriba: decir "vuelve en 0 min" cuando todavía
		# faltan cuarenta segundos es mentir.
		_estado.text = "%s · vuelve en %d min" % [e["seed"], ceili(falta / 60000.0)]
		_estado.add_theme_color_override("font_color", AVISO)
		_criatura.modulate = Color(1, 1, 1, 0.35)
	else:
		_estado.text = e["seed"]
		_estado.add_theme_color_override("font_color", FOSFORO)
		_criatura.modulate = Color(1, 1, 1, 1)

	_refrescar_sprite()

	if _cerca.is_empty() and _caja != null and not _caja.abierta():
		_cartel.text = _texto_de_ayuda()


func _sin_extension() -> void:
	var etiqueta := Label.new()
	etiqueta.text = "La GDExtension no cargó. Compilá con `scons` en gdext/."
	etiqueta.position = Vector2(20, 120)
	etiqueta.add_theme_color_override("font_color", AVISO)
	add_child(etiqueta)
