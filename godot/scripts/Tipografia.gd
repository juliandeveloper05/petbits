## Tipografia.gd — arma el `FontFile` de Godot desde el atlas del C++.
##
## No dibuja nada: los glifos vienen hechos. Lo que hace es traducirlos a la
## estructura que el motor entiende, que son tres cosas por letra —de dónde
## recortar la textura, dónde apoyarla respecto de la línea de base, y cuánto
## correr el cursor después.
##
## ---
##
## POR QUÉ SE ARMA EN TIEMPO DE EJECUCIÓN Y NO ES UN .TRES.
##
## Por lo mismo que el tileset del mundo: el atlas sale de código. Un recurso
## guardado sería una copia, y en cuanto alguien corrija la curva de una `S` el
## juego seguiría mostrando la vieja hasta que se acuerde de regenerarlo. Armarlo
## al arrancar cuesta un milisegundo y no puede desincronizarse.
##
## ---
##
## EL MODO DE ESCALA ES LO QUE LA MANTIENE NÍTIDA.
##
## `FIXED_SIZE_SCALE_INTEGER_ONLY` hace que pedirle un tamaño distinto del suyo
## la lleve al múltiplo entero más cercano en vez de interpolar. Sin eso, un
## Label con `font_size` 9 la achica a 9/11 y la fuente aparece con los trazos
## rotos y grises — el pixel art escalado a un factor no entero es exactamente lo
## que este proyecto trata de no hacer en ningún lado.

extends RefCounted


## Construye la fuente. `core` es un `PetBitsCore` ya instanciado.
static func construir(core: RefCounted) -> FontFile:
	var m: Dictionary = core.fuente_metricas()
	var ancho: int = m["ancho"]
	var alto: int = m["alto"]
	var avance: int = m["avance"]
	var ascenso: int = m["ascenso"]

	var fuente := FontFile.new()
	fuente.fixed_size = alto
	fuente.fixed_size_scale_mode = TextServer.FIXED_SIZE_SCALE_INTEGER_ONLY
	fuente.antialiasing = TextServer.FONT_ANTIALIASING_NONE
	fuente.subpixel_positioning = TextServer.SUBPIXEL_POSITIONING_DISABLED

	# El segundo componente del tamaño es el grosor del contorno. Cero: los
	# contornos de Godot son un desenfoque alrededor del glifo y sobre una fuente
	# de un píxel de trazo la ensucian entera.
	var tam := Vector2i(alto, 0)

	fuente.set_texture_image(0, tam, 0, core.atlas_fuente())
	fuente.set_cache_ascent(0, alto, ascenso)
	fuente.set_cache_descent(0, alto, m["descenso"])

	for g in core.fuente_glifos():
		var codigo: int = g["codigo"]
		fuente.set_glyph_texture_idx(0, tam, codigo, 0)
		fuente.set_glyph_uv_rect(0, tam, codigo, Rect2(g["x"], g["y"], ancho, alto))
		fuente.set_glyph_size(0, tam, codigo, Vector2(ancho, alto))
		# El offset se mide desde la línea de base hacia arriba, y por eso va en
		# negativo: la caja arranca `ascenso` píxeles por encima de ella.
		fuente.set_glyph_offset(0, tam, codigo, Vector2(0, -ascenso))
		fuente.set_glyph_advance(0, alto, codigo, Vector2(avance, 0))

	return fuente


## La deja como fuente por defecto de todo el juego.
##
## `ThemeDB.fallback_font` es lo que Godot usa cuando un control no tiene tema
## propio, que es el caso de todas las pantallas de PetBits: se arman por código
## y ninguna define un Theme. Poner la fuente acá alcanza para que la tomen los
## Label, los Button, el RichTextLabel del registro y los carteles del mapa, sin
## tocar ni una línea de esas escenas.
static func instalar(core: RefCounted) -> FontFile:
	var fuente := construir(core)
	ThemeDB.fallback_font = fuente
	ThemeDB.fallback_font_size = int(core.fuente_metricas()["alto"])
	return fuente
