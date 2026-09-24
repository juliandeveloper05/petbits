## Tema.gd — la ropa de los controles, en la paleta de la consola.
##
## Sin esto, botones, barras y separadores usaban el tema por defecto de Godot:
## cajas grises redondeadas encima de la consola verde fósforo. Era lo primero que
## veía cualquiera que abriera el juego, y lo que más lo delataba como prototipo.
##
## Se arma por código, igual que la tipografía y el tileset: un `.tres` guardado
## sería una copia que se desincroniza en cuanto alguien toque un color acá.
##
## ---
##
## ESQUINAS RECTAS, BORDES DE UN PÍXEL.
##
## Todo en este juego es pixel art a 480×270. Un botón con esquinas redondeadas a
## esa resolución se ve borroso, porque el redondeo se antialiasa. Rectos y con
## bordes enteros, se ven como parte del mismo dibujo que la criatura.
##
## ---
##
## APRETADO SE INVIERTE.
##
## El botón apretado se pinta fósforo con letra oscura: el negativo de la
## consola. Es lo que hacían las pantallas de una sola tinta, que no tenían otro
## color para marcar algo activo, y se lee al instante.

extends RefCounted

const FONDO := Color("#0a0e0a")
const FONDO_CONTROL := Color("#111a13")
const BORDE := Color("#3d5c46")
const FOSFORO := Color("#9bbc0f")
const TEXTO := Color("#d6e6d0")
const TENUE := Color("#7e937a")


static func construir(tam_fuente: int, fuente: Font = null) -> Theme:
	var tema := Theme.new()

	# La tipografía del juego, puesta EN EL TEMA y no solo como respaldo.
	#
	# `Tipografia.instalar()` la dejaba en `ThemeDB.fallback_font`, que es lo que
	# Godot usa cuando ningún tema define una fuente — pero el tema por defecto
	# del motor SÍ define una, así que el respaldo no se usaba nunca. La fuente de
	# 5×7 que genera el C++ existía, tenía sus 117 glifos y pasaba todos sus
	# tests, y en pantalla se veía la letra suavizada de Godot. Los tests medían
	# el `FontFile`; ninguno miraba qué fuente dibujaba un Label.
	if fuente != null:
		tema.default_font = fuente
		tema.default_font_size = tam_fuente

	# --- Botones ------------------------------------------------------------
	tema.set_stylebox("normal", "Button", _caja(FONDO_CONTROL, BORDE))
	tema.set_stylebox("hover", "Button", _caja(FONDO_CONTROL, FOSFORO))
	tema.set_stylebox("pressed", "Button", _caja(FOSFORO, FOSFORO))
	tema.set_stylebox("disabled", "Button", _caja(FONDO, BORDE.darkened(0.4)))
	# El foco se dibuja ENCIMA del estilo normal: por eso es solo un borde, sin
	# relleno. Es lo que muestra dónde está parado el teclado o el joystick.
	tema.set_stylebox("focus", "Button", _borde(FOSFORO))

	tema.set_color("font_color", "Button", TEXTO)
	tema.set_color("font_hover_color", "Button", FOSFORO)
	tema.set_color("font_focus_color", "Button", FOSFORO)
	tema.set_color("font_pressed_color", "Button", FONDO)
	tema.set_color("font_hover_pressed_color", "Button", FONDO)
	tema.set_color("font_disabled_color", "Button", TENUE.darkened(0.3))
	tema.set_font_size("font_size", "Button", tam_fuente)

	# --- Barras -------------------------------------------------------------
	# El relleno es blanco a propósito: PetView lo tiñe con `modulate` según el
	# semáforo, y sobre blanco el tinte sale puro.
	tema.set_stylebox("background", "ProgressBar", _caja(FONDO_CONTROL, BORDE))
	tema.set_stylebox("fill", "ProgressBar", _caja(Color.WHITE, Color.WHITE, 0))

	# --- Separadores --------------------------------------------------------
	var linea := StyleBoxLine.new()
	linea.color = BORDE
	linea.thickness = 1
	tema.set_stylebox("separator", "HSeparator", linea)
	tema.set_constant("separation", "HSeparator", 3)

	# --- Tooltips -----------------------------------------------------------
	tema.set_stylebox("panel", "TooltipPanel", _caja(FONDO, FOSFORO))
	tema.set_color("font_color", "TooltipLabel", TEXTO)

	# --- Paneles (la confirmación del título) --------------------------------
	tema.set_stylebox("panel", "PanelContainer", _caja(FONDO, FOSFORO, 6))

	return tema


## Una caja rellena con borde de un píxel, esquinas rectas.
static func _caja(relleno: Color, borde: Color, margen: int = 3) -> StyleBoxFlat:
	var caja := StyleBoxFlat.new()
	caja.bg_color = relleno
	caja.border_color = borde
	caja.set_border_width_all(1)
	caja.set_corner_radius_all(0)
	caja.anti_aliasing = false
	caja.content_margin_left = margen + 2
	caja.content_margin_right = margen + 2
	caja.content_margin_top = margen - 1
	caja.content_margin_bottom = margen - 1
	return caja


## Solo el borde, sin relleno. Para dibujar encima de otro estilo.
static func _borde(color: Color) -> StyleBoxFlat:
	var caja := StyleBoxFlat.new()
	caja.draw_center = false
	caja.border_color = color
	caja.set_border_width_all(1)
	caja.set_corner_radius_all(0)
	caja.anti_aliasing = false
	return caja
