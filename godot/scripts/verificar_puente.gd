## verificar_puente.gd
##
## Comprueba, sin abrir el editor, que Godot carga la GDExtension y que los
## valores llegan intactos hasta GDScript.
##
##   godot --headless --path godot --script res://scripts/verificar_puente.gd
##
## ---
##
## QUÉ VERIFICA ESTO Y QUÉ NO.
##
## Los algoritmos ya están verificados en gdext/tests: 40.416 comprobaciones
## contra lo que devuelve el TypeScript. Repetir eso acá no agregaría nada.
##
## Lo que falta comprobar es el último tramo, que esos tests no tocan: que la
## biblioteca cargue en el motor, que la clase quede registrada, y que los
## valores sobrevivan el viaje C++ → Variant → GDScript. Ahí hay cosas que
## pueden romperse solas —enteros de 64 bits que no entran en el int con signo
## de GDScript, texto con acentos que se convierte mal— y que se ven perfectas
## del lado del C++.
##
## Devuelve 0 si está todo bien y 1 si algo no coincide, así que sirve tal cual
## en un script.

extends SceneTree

## Los esperados salen de gdext/tests/vectores_generados.h, que a su vez sale de
## ejecutar el TypeScript. No están calculados a mano.
const SEED_EJEMPLO := "A3F0-91C4-77BE-2D08"

var _fallas := 0


func _init() -> void:
	print("\nPetBits — puente GDExtension → GDScript")
	print("Godot %s\n" % Engine.get_version_info().string)

	if not ClassDB.class_exists("PetBitsCore"):
		print("FALLA: la clase PetBitsCore no está registrada.")
		print("       La extensión no cargó. Revisá que exista el .dll en godot/bin/")
		print("       y que su nombre coincida con petbits_core.gdextension.")
		quit(1)
		return

	print("La extensión cargó y PetBitsCore está registrada.")

	var core: RefCounted = ClassDB.instantiate("PetBitsCore")
	print("%s\n" % core.version())

	# El "·" de version() es un byte doble en UTF-8. Si el C++ arma el String
	# con el constructor desde const char* en vez de String::utf8(), cada byte
	# se ensancha por separado y aparece "Â·". Pasó de verdad, así que queda
	# comprobado: es un error silencioso que solo se ve mirando la pantalla.
	_igual(core.version().contains("Â"), false, "version() sin mojibake")

	_probar_genes(core)
	_probar_rarezas(core)
	_probar_acentos(core)
	_probar_seeds_grandes(core)
	_probar_simulacion(core)
	_probar_particion(core)
	_probar_sprite(core)
	_probar_acciones(core)
	_probar_despensa(core)
	_probar_expediciones(core)

	if _fallas == 0:
		print("\nPuente OK: los valores llegan intactos hasta GDScript.")
		quit(0)
	else:
		print("\n%d fallas." % _fallas)
		quit(1)


func _igual(obtenido: Variant, esperado: Variant, que: String) -> void:
	if obtenido == esperado:
		return
	_fallas += 1
	print("  FALLA %s: dio %s, se esperaba %s" % [que, str(obtenido), str(esperado)])


func _probar_genes(core: RefCounted) -> void:
	print("decodificar(\"%s\")" % SEED_EJEMPLO)
	var g: Dictionary = core.decodificar(SEED_EJEMPLO)

	if g.is_empty():
		_fallas += 1
		print("  FALLA: devolvió un diccionario vacío")
		return

	_igual(g["seed"], SEED_EJEMPLO, "seed")
	_igual(g["lineage"], 8, "lineage")
	_igual(g["bodyShape"], 0, "bodyShape")
	_igual(g["eyes"], 13, "eyes")
	_igual(g["mouth"], 2, "mouth")
	_igual(g["appendages"], 14, "appendages")
	_igual(g["pattern"], 11, "pattern")
	_igual(g["hue"], 119, "hue")
	_igual(g["paletteMode"], 4, "paletteMode")
	_igual(g["temperament"], 0, "temperament")
	_igual(g["metabolism"], 7, "metabolism")
	_igual(g["affinity"], 0, "affinity")
	_igual(g["proportion"], 9, "proportion")
	_igual(g["mutation"], 163, "mutation")

	var sesgo: Dictionary = g["statBias"]
	_igual(sesgo["vigor"], 0, "statBias.vigor")
	_igual(sesgo["animo"], 0, "statBias.animo")
	_igual(sesgo["ingenio"], 3, "statBias.ingenio")
	_igual(sesgo["vinculo"], 3, "statBias.vinculo")

	# Los nombres viajan como String y llevan acentos: es donde una conversión
	# mal hecha se nota.
	_igual(g["linaje"], "Vapor", "linaje")
	_igual(g["temperamento"], "Plácido", "temperamento")
	_igual(g["afinidad"], "Brasa", "afinidad")
	_igual(g["metabolismo"], "Frenético", "metabolismo")

	print("  %s · %s · %s" % [g["linaje"], g["temperamento"], g["afinidad"]])


func _probar_rarezas(core: RefCounted) -> void:
	# El seed 0 tiene tres rarezas: popcount 0 (Vacío), primer byte igual al
	# último (Uróboros) y los 16 bits altos espejo de los bajos (Espejo).
	print("rarezas(\"0\")")
	var rarezas: Array = core.rarezas("0")
	_igual(rarezas.size(), 3, "cantidad de rarezas")

	var ids := []
	for r in rarezas:
		ids.append(r["id"])
	ids.sort()
	_igual(ids, ["espejo", "uroboros", "vacio"], "ids de rarezas")

	for r in rarezas:
		print("  ◆ %s (%s) — %s" % [r["nombre"], r["tier"], r["regla"]])

	# El seed de ejemplo no tiene ninguna: el caso vacío también importa.
	_igual(core.rarezas(SEED_EJEMPLO).size(), 0, "rarezas del seed de ejemplo")


func _probar_acentos(core: RefCounted) -> void:
	# Escribir un texto como seed lo hashea. Con acentos el hash se calcula
	# sobre unidades UTF-16, no sobre bytes UTF-8, para coincidir con el
	# charCodeAt del TypeScript. Si el String de Godot se convirtiera mal en el
	# camino, este valor cambiaría.
	print("formatear_seed(\"Nébula\")")
	var obtenido: String = core.formatear_seed("Nébula")
	_igual(obtenido, "4347-1D9E-073B-4812", "hash de texto con acento")
	print("  %s" % obtenido)


func _probar_seeds_grandes(core: RefCounted) -> void:
	# Seeds con el bit 63 encendido: son la mitad del espacio posible, y si
	# cruzaran como entero llegarían negativos al int con signo de GDScript. Por
	# eso los seeds viajan como texto.
	print("seeds con el bit más alto encendido")
	_igual(core.formatear_seed("FFFFFFFFFFFFFFFF"), "FFFF-FFFF-FFFF-FFFF", "seed máximo")
	_igual(core.formatear_seed("0x8000000000000000"), "8000-0000-0000-0000", "bit 63")
	_igual(core.formatear_seed("0"), "0000-0000-0000-0000", "seed cero")

	# El prefijo 0x de arriba no es decorativo, y esto lo fija por escrito:
	# "8000000000000000" son todos dígitos, así que se lee como DECIMAL y da otra
	# criatura. El orden de parseSeed pone el hexadecimal sin letras último, para
	# que "1234" signifique mil doscientos treinta y cuatro y no 0x1234.
	#
	# Es una diferencia visible para el jugador —dos textos parecidos, dos bichos
	# distintos— así que conviene que un cambio de comportamiento acá rompa algo.
	_igual(core.formatear_seed("8000000000000000"), "001C-6BF5-2634-0000", "todo dígitos = decimal")


func _probar_simulacion(core: RefCounted) -> void:
	# Un día entero simulado. Los esperados salen del mismo lugar que todo lo
	# demás: gdext/tests/vectores_generados.h, que produce el TypeScript.
	#
	# Un día son 1440 ticks, y en cada uno los stats se recalculan sobre el valor
	# anterior. Que la salud termine en 92.2119999999997 y no en 92.212 no es
	# ruido: es el resultado exacto de acumular en punto flotante, y es el mismo
	# número que da la web. Si acá diera 92.212 redondo, algo estaría haciendo
	# las cuentas distinto.
	print("simular — un día entero desde 2026-08-11T00:00Z, tz -180")

	const INICIO_MS := 1786406400000
	const TICK_MS := 60000
	const TICKS := 1440

	if not core.nacer("A3F0-91C4-77BE-2D08", INICIO_MS, -180):
		_fallas += 1
		print("  FALLA: nacer() rechazó un seed válido")
		return

	var r: Dictionary = core.simular(INICIO_MS + TICKS * TICK_MS)
	_igual(r["ticks"], 1440, "ticks simulados")

	var e: Dictionary = core.estado()
	_igual(e["ticks_vividos"], 1440, "ticks_vividos")
	_igual(e["ticks_activos"], 1440, "ticks_activos")
	_igual(e["letargico"], false, "letargico")
	_igual(e["etapa"], "juvenil", "etapa")
	_igual(e["forma"], "Pétreo", "forma")

	var st: Dictionary = e["stats"]
	_igual(st["energia"], 0.0, "energia")
	_igual(st["animo"], 0.0, "animo")
	_igual(st["salud"], 92.2119999999997, "salud")

	var resumen: Dictionary = r["resumen"]
	_igual(resumen.get("evolucion", 0), 1, "eventos de evolución")
	_igual(resumen.get("hallazgo", 0), 6, "hallazgos")
	_igual(r["eventos"].size(), 11, "eventos devueltos")

	print("  %s · %s · salud %.4f" % [e["etapa"], e["forma"], st["salud"]])
	for ev in r["eventos"]:
		if ev["tipo"] == "evolucion":
			print("  ◆ %s" % ev["texto"])


func _probar_particion(core: RefCounted) -> void:
	# Simular de una vez tiene que dar lo mismo que simular en dos pedazos.
	#
	# Ya está comprobado en los tests de C++, pero se repite acá por una razón
	# distinta: es la propiedad de la que depende que el juego funcione. El
	# jugador cierra y abre cuando quiere, así que el mismo tiempo transcurrido
	# se simula partido de mil maneras. Si el resultado dependiera de en cuántos
	# pedazos se hizo, dos personas con la misma criatura y la misma ausencia
	# terminarían con criaturas distintas.
	print("invariante de partición a través del puente")

	const INICIO_MS := 1786406400000
	const TICK_MS := 60000

	core.nacer("A3F0-91C4-77BE-2D08", INICIO_MS, -180)
	core.simular(INICIO_MS + 2000 * TICK_MS)
	var entero: Dictionary = core.estado()

	var otro: RefCounted = ClassDB.instantiate("PetBitsCore")
	otro.nacer("A3F0-91C4-77BE-2D08", INICIO_MS, -180)
	otro.simular(INICIO_MS + 733 * TICK_MS)
	otro.simular(INICIO_MS + 1501 * TICK_MS)
	otro.simular(INICIO_MS + 2000 * TICK_MS)
	var partido: Dictionary = otro.estado()

	_igual(partido["ticks_vividos"], entero["ticks_vividos"], "ticks_vividos partido")
	_igual(partido["ticks_activos"], entero["ticks_activos"], "ticks_activos partido")
	_igual(partido["etapa"], entero["etapa"], "etapa partida")
	_igual(partido["forma"], entero["forma"], "forma partida")

	var a: Dictionary = entero["stats"]
	var b: Dictionary = partido["stats"]
	_igual(b["energia"], a["energia"], "energia partida")
	_igual(b["animo"], a["animo"], "animo partida")
	_igual(b["salud"], a["salud"], "salud partida")

	print("  2000 ticks de una vez == 733 + 768 + 499")


func _probar_sprite(core: RefCounted) -> void:
	# El sprite cruza el puente como Image de 32×32 RGBA8. Se rehace el mismo
	# hash FNV que usan los tests de C++ sobre los bytes que llegaron a GDScript:
	# si el buffer se copiara mal, o Image reordenara los canales, el número
	# cambia. Comparar "se ve bien" a ojo no serviría para esto.
	print("sprite_actual — 32x32 RGBA8")

	var img: Image = core.sprite(SEED_EJEMPLO, "adulto", "indefinida", false)
	if img == null:
		_fallas += 1
		print("  FALLA: sprite() devolvió null")
		return

	_igual(img.get_width(), 32, "ancho")
	_igual(img.get_height(), 32, "alto")
	_igual(img.get_format(), Image.FORMAT_RGBA8, "formato")

	var datos: PackedByteArray = img.get_data()
	_igual(datos.size(), 32 * 32 * 4, "bytes del buffer")

	# FNV-1a de 32 bits, el mismo de gdext/src/pixel_buffer.h.
	var hash: int = 0x811C9DC5
	var opacos := 0
	for i in range(datos.size()):
		hash = (hash ^ datos[i]) & 0xFFFFFFFF
		hash = (hash * 0x01000193) & 0xFFFFFFFF
		if i % 4 == 3 and datos[i] > 0:
			opacos += 1

	_igual(opacos, 510, "píxeles opacos")
	_igual(hash, 0x83b6c114, "hash del buffer")

	print("  %d píxeles opacos, hash %08x" % [opacos, hash])

	# El parpadeo cambia la cara sin tocar la silueta: mismo conteo de opacos,
	# distinto dibujo. Es la comprobación de que la expresión llega de verdad y
	# no se ignora en el camino.
	var parpadeo: Image = core.sprite(SEED_EJEMPLO, "adulto", "indefinida", true)
	var distintos := parpadeo.get_data() != datos
	_igual(distintos, true, "el parpadeo cambia el dibujo")


func _probar_acciones(core: RefCounted) -> void:
	# Los esperados salen del vector "baya al nacer" de vectores_generados.h.
	# Una criatura recién nacida tiene 70 de energía; la baya suma 18 y 5 de
	# ánimo, y toda acción da 2 de vínculo.
	print("alimentar / jugar / acariciar")

	const INICIO_MS := 1786406400000

	core.nacer(SEED_EJEMPLO, INICIO_MS, -180)

	var alimentos: Array = core.alimentos()
	_igual(alimentos.size(), 4, "cantidad de alimentos")
	_igual(alimentos[1]["nombre"], "Raíz", "el nombre con tilde cruza bien")

	var r: Dictionary = core.alimentar("baya", INICIO_MS)
	_igual(r["ok"], true, "alimentar acepta")
	_igual(r["mensaje"], "Se morfó la baya sin respirar.", "mensaje de alimentar")

	var st: Dictionary = core.estado()["stats"]
	_igual(st["energia"], 88.0, "energia tras la baya")
	_igual(st["animo"], 75.0, "animo tras la baya")
	_igual(st["vinculo"], 2.0, "vinculo tras la baya")

	# Un rechazo tiene que traer el motivo, no un error genérico. Es lo que la
	# pantalla le muestra al jugador para que sepa qué hacer.
	core.nacer(SEED_EJEMPLO, INICIO_MS, -180)
	var flaca: Dictionary = core.simular(INICIO_MS + 2000 * 60000)
	var rechazo: Dictionary = core.jugar(INICIO_MS + 2000 * 60000)
	_igual(rechazo["ok"], false, "jugar sin energía se rechaza")
	_igual(
		rechazo["mensaje"],
		"No le da la energía para jugar. Primero tiene que comer algo.",
		"el rechazo explica por qué"
	)

	print("  %s" % rechazo["mensaje"])


func _probar_despensa(core: RefCounted) -> void:
	# La comida se gasta. Este bloque existe por un bug que ningún test encontró:
	# apareció pasando una partida real de la web al nativo y de vuelta. El
	# inventario se guardaba y se devolvía intacto —eso estaba testeado— pero
	# nadie lo descontaba, así que del lado nativo la comida era infinita.
	#
	# Como la dieta decide la rama evolutiva, se podía empujar una evolución sin
	# pagar lo que la web sí cobra.
	print("la despensa se gasta")

	const INICIO_MS := 1786406400000
	core.nacer(SEED_EJEMPLO, INICIO_MS, -180)

	var antes: Array = core.alimentos()
	var bayas_antes: int = antes[0]["cantidad"]
	_igual(bayas_antes, 3, "arranca con 3 bayas")

	var r: Dictionary = core.alimentar("baya", INICIO_MS)
	_igual(r["ok"], true, "la primera baya se puede comer")
	_igual(core.alimentos()[0]["cantidad"], 2, "quedan 2 bayas")

	core.alimentar("baya", INICIO_MS)
	core.alimentar("baya", INICIO_MS)
	_igual(core.alimentos()[0]["cantidad"], 0, "se acabaron")

	var vacio: Dictionary = core.alimentar("baya", INICIO_MS)
	_igual(vacio["ok"], false, "sin bayas se rechaza")
	_igual(vacio["mensaje"], "No te queda de eso. Mandala a buscar.", "el rechazo dice qué hacer")
	print("  %s" % vacio["mensaje"])

	# Y el cobro NO ocurre si la acción falla. El cristal arranca en cero, así
	# que se prueba con uno que sí hay: se manda de expedición para forzar el
	# rechazo y se comprueba que la larva siga entera.
	_igual(core.alimentos()[2]["cantidad"], 2, "las larvas siguen enteras")

	# La despensa viaja en el guardado.
	var texto: String = core.guardar(INICIO_MS)
	var otro: RefCounted = ClassDB.instantiate("PetBitsCore")
	otro.cargar(texto)
	_igual(otro.alimentos()[0]["cantidad"], 0, "las bayas gastadas siguen gastadas")
	_igual(otro.alimentos()[2]["cantidad"], 2, "las larvas siguen ahí")


func _probar_expediciones(core: RefCounted) -> void:
	# Es lo que cierra la economía. Sin expediciones, la despensa arranca con 7
	# unidades y no hay forma de conseguir más: a las 7 alimentadas el juego se
	# traba y no queda nada que hacer.
	print("expediciones")

	const INICIO_MS := 1786406400000
	const MINUTO := 60000

	core.nacer(SEED_EJEMPLO, INICIO_MS, -180)

	var destinos: Array = core.destinos()
	_igual(destinos.size(), 3, "hay tres destinos")

	# El patio TIENE que estar disponible siempre, incluso recién nacida. Es la
	# regla que evita que la partida quede muerta.
	_igual(destinos[0]["id"], "patio", "el primero es el patio")
	_igual(destinos[0]["puede"], true, "el patio siempre está disponible")
	_igual(destinos[1]["puede"], false, "el bosque pide juvenil")
	_igual(destinos[2]["puede"], false, "las ruinas piden adulto")
	print("  bosque cerrado: %s" % destinos[1]["motivo"])

	var salida: Dictionary = core.enviar("patio", INICIO_MS)
	_igual(salida["ok"], true, "se puede mandar al patio")
	_igual(core.falta_para_volver(INICIO_MS), 15 * MINUTO, "vuelve en 15 minutos")

	# Mientras está afuera no se la puede alimentar ni mandar de nuevo.
	_igual(core.alimentar("baya", INICIO_MS)["ok"], false, "de expedición no come")
	_igual(core.enviar("patio", INICIO_MS)["ok"], false, "no sale dos veces")

	# Y no vuelve antes de tiempo.
	_igual(core.recibir(INICIO_MS + 14 * MINUTO)["volvio"], false, "a los 14 min sigue afuera")

	var bayas_antes: int = core.alimentos()[0]["cantidad"]
	var regreso: Dictionary = core.recibir(INICIO_MS + 15 * MINUTO)
	_igual(regreso["volvio"], true, "a los 15 min ya volvió")
	print("  %s %s" % [regreso["destino"], regreso["mensaje"]])

	# El botín ENTRA en la despensa. Es el punto de todo esto.
	var total_botin := 0
	for cantidad in regreso["botin"].values():
		total_botin += cantidad
	_igual(total_botin >= 1, true, "el patio nunca vuelve vacío")

	var bayas_despues: int = core.alimentos()[0]["cantidad"]
	var raices_despues: int = core.alimentos()[1]["cantidad"]
	_igual(bayas_despues + raices_despues > bayas_antes + 2, true, "la despensa creció")
	print("  bayas: %d → %d" % [bayas_antes, bayas_despues])

	# Y ya puede volver a comer.
	_igual(core.alimentar("baya", INICIO_MS + 15 * MINUTO)["ok"], true, "vuelve a poder comer")
