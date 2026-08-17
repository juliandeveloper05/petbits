## Escritura.gd — escribir texto adentro de una `Image`, a mano.
##
## Lo usan las herramientas de línea de comandos que componen PNG sin abrir una
## ventana. No sirve para el juego: ahí dibuja Godot, con la misma fuente pero
## por su cuenta.
##
## ---
##
## POR QUÉ HACE FALTA.
##
## Godot en modo headless no dibuja nada —pedirle una captura devuelve negro— así
## que la única forma de VER cómo queda algo antes de abrir el editor es componer
## la imagen píxel por píxel. Y como la fuente es un atlas de recortes, escribir
## un renglón es recortar y pegar, que es exactamente lo que hace el motor.
##
## Eso le da un valor extra: si el recorte estuviera mal calculado, se vería acá
## igual que en pantalla. Las muestras no son una aproximación de cómo va a
## quedar, son la misma cuenta.

extends RefCounted


## Prepara todo lo que hace falta para escribir: el atlas, dónde está cada glifo
## y las medidas de la caja. `core` es un `PetBitsCore` ya instanciado.
static func preparar(core: RefCounted) -> Dictionary:
	var ubicacion := {}
	for g in core.fuente_glifos():
		ubicacion[g["codigo"]] = Vector2i(g["x"], g["y"])
	return {
		"atlas": core.atlas_fuente(),
		"ubicacion": ubicacion,
		"m": core.fuente_metricas(),
	}


## Cuánto va a ocupar un texto, en píxeles, a esa escala.
static func ancho(tipo: Dictionary, texto: String, escala: int = 1) -> int:
	return texto.length() * int(tipo["m"]["avance"]) * escala


## Escribe una línea. `y` es el borde de ARRIBA de la caja, no la línea de base.
##
## Un carácter sin glifo sale como un recuadro en vez de como un hueco: el punto
## de estas herramientas es que los agujeros se vean, y un espacio en blanco
## donde tendría que haber una `ñ` se confunde con un espacio de verdad.
static func escribir(
	destino: Image, tipo: Dictionary, texto: String,
	x0: int, y0: int, color: Color, escala: int = 1
) -> void:
	var m: Dictionary = tipo["m"]
	var x := x0
	for i in texto.length():
		var codigo := texto.unicode_at(i)
		if tipo["ubicacion"].has(codigo):
			_pegar(destino, tipo, tipo["ubicacion"][codigo], x, y0, color, escala)
		elif codigo != 32:
			_recuadro(destino, m, x, y0, Color("#ff6b6b"), escala)
		x += int(m["avance"]) * escala


static func _pegar(
	destino: Image, tipo: Dictionary, origen: Vector2i,
	px: int, py: int, color: Color, escala: int
) -> void:
	var atlas: Image = tipo["atlas"]
	var m: Dictionary = tipo["m"]
	for f in int(m["alto"]):
		for c in int(m["ancho"]):
			if atlas.get_pixel(origen.x + c, origen.y + f).a == 0.0:
				continue
			_punto(destino, px + c * escala, py + f * escala, color, escala)


static func _recuadro(
	destino: Image, m: Dictionary, px: int, py: int, color: Color, escala: int
) -> void:
	for f in int(m["alto"]):
		for c in int(m["ancho"]):
			var borde: bool = (
				f == 0 or f == int(m["alto"]) - 1 or c == 0 or c == int(m["ancho"]) - 1
			)
			if borde:
				_punto(destino, px + c * escala, py + f * escala, color, escala)


static func _punto(destino: Image, px: int, py: int, color: Color, escala: int) -> void:
	for dy in escala:
		for dx in escala:
			var x := px + dx
			var y := py + dy
			if x >= 0 and y >= 0 and x < destino.get_width() and y < destino.get_height():
				destino.set_pixel(x, y, color)
