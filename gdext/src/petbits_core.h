#pragma once
/**
 * PetBitsCore — el puente entre el núcleo portado y GDScript.
 *
 * Es una clase deliberadamente chica. Su razón de ser, por ahora, es que exista
 * un camino completo y comprobable: C++ compilado → biblioteca con el nombre
 * correcto → Godot la carga → los algoritmos del genoma responden. Mientras no
 * haya nada registrado, `scons` produce un .dll que no se puede distinguir de
 * uno roto.
 *
 * ---
 *
 * LOS SEEDS VIAJAN COMO TEXTO, NO COMO ENTERO.
 *
 * El seed es un uint64_t y el int de GDScript es un int64 con signo. Todo lo
 * que pase de 2^63 —la mitad de los seeds posibles— llegaría negativo, y
 * cualquier comparación o impresión del otro lado mostraría otra cosa.
 *
 * Como String no hay ambigüedad, y encima es el formato que el jugador ya ve en
 * pantalla ("A3F0-91C4-77BE-2D08"). El parseo lo hace `parseSeed`, el mismo que
 * usa la web, así que acepta lo mismo: hex con guiones o sin, decimal, o
 * cualquier texto —tu nombre— que se convierte en un seed por hash.
 */

#include "breeding.h"
#include "codex.h"
#include "world_gen.h"
#include "save_manager.h"
#include "simulation.h"

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include <optional>

namespace godot {

class PetBitsCore : public RefCounted {
    GDCLASS(PetBitsCore, RefCounted)

protected:
    static void _bind_methods();

private:
    /**
     * La partida entera, si hay una: las criaturas, cuál está activa, y el
     * codex y el inventario sin interpretar.
     *
     * Se guarda acá en vez de que GDScript pase el estado en cada llamada.
     * Serializar diecisiete campos de ida y de vuelta en cada tick sería un
     * montón de código de traducción, y cada campo una oportunidad de perder
     * algo en el camino — que es exactamente el tipo de error que el resto del
     * proyecto se ocupa de que no pase.
     *
     * Y guarda la Partida y no solo la criatura porque es lo que permite
     * guardar sin tirar nada: el codex viaja adentro aunque este código todavía
     * no sepa qué hacer con él.
     */
    std::optional<petbits::Partida> partida;

    /**
     * Anota la criatura activa en el codex y devuelve lo que fue novedad.
     *
     * Se llama al nacer, al cargar y cada vez que la simulación reporta una
     * evolución. Ese último es el que importa: es el momento en que aparece algo
     * que el codex todavía no tenía, y era justo el que faltaba mientras el
     * codex viajaba sin interpretarse.
     */
    Array registrar_activa();

    /** La criatura activa, para escribirle. nullptr si no hay partida. */
    petbits::CreatureState* activa();
    const petbits::CreatureState* activa() const;

public:
    /** "a3f0 91c4" → "A3F0-91C4-0000-0000". Devuelve "" si la entrada es vacía. */
    String formatear_seed(const String& entrada) const;

    /** Los 14 genes decodificados, más los nombres de linaje, temperamento y demás. */
    Dictionary decodificar(const String& entrada) const;

    /** Las rarezas del genoma. Cada una es un Dictionary con id, nombre, regla y tier. */
    Array rarezas(const String& entrada) const;

    /** Un seed nuevo con entropía del sistema operativo, ya formateado. */
    String seed_al_azar() const;

    /**
     * Para que la escena de arranque pueda confirmar de qué build se está
     * hablando. Si esto responde, toda la cadena funciona.
     */
    String version() const;

    // -----------------------------------------------------------------------
    // Simulación
    // -----------------------------------------------------------------------

    /**
     * Nace una criatura. Devuelve false si el seed no se pudo leer.
     *
     * `tz_min` son minutos de desfasaje horario y se guardan en el estado a
     * propósito: si la hora local se leyera del sistema en cada tick, la misma
     * partida daría resultados distintos según dónde se abra, y se rompería el
     * invariante de que simular por pedazos da lo mismo que de corrido.
     */
    bool nacer(const String& seed, int64_t ahora_ms, int64_t tz_min);

    bool tiene_criatura() const;

    /**
     * Avanza el tiempo hasta `ahora_ms` y devuelve lo que pasó.
     *
     * { ticks, omitidos, eventos: Array[Dictionary], resumen: Dictionary }
     */
    Dictionary simular(int64_t ahora_ms);

    /** El estado actual, listo para pintar en pantalla. */
    Dictionary estado() const;

    // -----------------------------------------------------------------------
    // Acciones
    // -----------------------------------------------------------------------

    /** El catálogo de alimentos, para armar los botones sin repetirlo en GDScript. */
    Array alimentos() const;

    /**
     * Las tres acciones. Todas devuelven { ok: bool, mensaje: String }.
     *
     * Cuando `ok` es false, `mensaje` dice POR QUÉ no se pudo —"no le da la
     * energía para jugar", "está de expedición"— y eso se muestra tal cual. Es
     * mejor que deshabilitar el botón: un botón gris no explica nada, y el
     * motivo forma parte de lo que el jugador tiene que entender para decidir.
     */
    Dictionary alimentar(const String& alimento_id, int64_t ahora_ms);
    Dictionary jugar(int64_t ahora_ms);
    Dictionary acariciar(int64_t ahora_ms);

    // -----------------------------------------------------------------------
    // Expediciones
    // -----------------------------------------------------------------------

    /**
     * Los destinos, con si la criatura puede ir a cada uno y por qué no.
     *
     * El motivo viaja junto al destino en vez de calcularse en GDScript: es una
     * regla del juego —"todavía está muy chica para ir tan lejos"— y las reglas
     * viven de un solo lado.
     */
    Array destinos() const;

    /** La manda. Devuelve { ok, mensaje }. */
    Dictionary enviar(const String& destino_id, int64_t ahora_ms);

    /**
     * La recibe si ya volvió, y le suma el botín a la despensa.
     *
     * { volvio: bool, mensaje: String, botin: Dictionary, semilla: String }
     *
     * `volvio` en false si sigue afuera o si nunca salió — no es un error, es la
     * respuesta normal cuando todavía no es hora.
     */
    Dictionary recibir(int64_t ahora_ms);

    /** Milisegundos que faltan para que vuelva. Cero si está en casa. */
    int64_t falta_para_volver(int64_t ahora_ms) const;

    // -----------------------------------------------------------------------
    // Sprite
    // -----------------------------------------------------------------------

    /**
     * Dibuja la criatura y la devuelve como Image de 32×32 RGBA8.
     *
     * `etapa` y `forma` van como texto ("bebe"/"juvenil"/"adulto",
     * "indefinida"/"petreo"/…) porque es lo que ya devuelve `estado()`: así la
     * escena pasa lo que leyó, sin traducir a números por el camino.
     *
     * Devuelve una Image y no una Texture porque la Image es un recurso de
     * datos, no de video: se puede guardar, comparar píxel a píxel en un test, y
     * convertir a textura de un renglón cuando haga falta mostrarla. Al revés no
     * se puede sin volver a bajar los datos de la placa.
     */
    Ref<Image> sprite(const String& seed, const String& etapa, const String& forma,
                      bool parpadeo = false) const;

    /** El sprite de la criatura viva, con su etapa y su forma actuales. */
    Ref<Image> sprite_actual(bool parpadeo = false) const;

    // -----------------------------------------------------------------------
    // Mundo
    // -----------------------------------------------------------------------

    /**
     * El atlas de tiles del mundo: todos en una fila, 16×16 cada uno.
     *
     * Generado por código como todo lo demás, y con la misma maquinaria OKLCH
     * que las paletas de las criaturas — por eso el pasto y un cuerpo de linaje
     * Musgo se ven de la misma familia en vez de convivir a la fuerza.
     */
    Ref<Image> atlas_tiles() const;

    /** Cuántos tipos de tile hay. */
    int64_t cantidad_tiles() const;

    /** ¿Ese tile frena al que camina? */
    bool tile_solido(int64_t indice) const;

    // -----------------------------------------------------------------------
    // El mundo infinito
    // -----------------------------------------------------------------------

    /**
     * La semilla del mundo que le toca a esta partida.
     *
     * Es el genoma de la PRIMERA criatura de la colección — con la que
     * empezaste, no la que estés cuidando hoy. Que sea la primera y no la activa
     * es lo que hace que el mundo no cambie debajo de tus pies cada vez que
     * incubás algo o cruzás.
     *
     * Devuelve "" si no hay partida.
     */
    String semilla_del_mundo() const;

    /**
     * Cuántos tiles de lado tiene un chunk. GDScript lo necesita para saber
     * cuáles pedir, y tenerlo escrito de los dos lados es tenerlo mal de uno.
     */
    int64_t lado_de_chunk() const;

    /**
     * Un chunk entero, fila por fila, como índices de tile.
     *
     * Va como `PackedByteArray` y no como Array de enteros porque son mil
     * veinticuatro valores por chunk y hasta nueve chunks a la vez: un Array de
     * Variants sería un objeto por tile.
     */
    PackedByteArray mundo_chunk(const String& semilla, int64_t cx, int64_t cy) const;

    /** Un tile suelto, para preguntar por dónde se puede caminar. */
    int64_t mundo_tile(const String& semilla, int64_t x, int64_t y) const;

    /** Cómo se llama el lugar donde estás parado: "el bosque", "la orilla". */
    String mundo_bioma(const String& semilla, int64_t x, int64_t y) const;

    // -----------------------------------------------------------------------
    // Tipografía
    // -----------------------------------------------------------------------

    /**
     * El atlas de la fuente: tinta blanca opaca sobre transparente.
     *
     * Blanca y no verde a propósito. Godot multiplica la textura por el color de
     * la fuente, así que el mismo atlas sirve para el fósforo del registro, el
     * ámbar de los avisos y el rojo de las alertas. Si viniera coloreado habría
     * que generar uno por color.
     */
    Ref<Image> atlas_fuente() const;

    /**
     * Dónde vive cada glifo y cuánto ocupa, para que GDScript pueda armar el
     * `FontFile`: [{ codigo, x, y }], ordenado por punto de código.
     *
     * El recorte es siempre del mismo tamaño (`ancho` × `alto` de las métricas),
     * así que no hace falta mandarlo glifo por glifo.
     */
    Array fuente_glifos() const;

    /**
     * Las medidas de la caja: { ancho, alto, avance, ascenso, descenso }.
     *
     * Van juntas y desde el C++ porque son un conjunto coherente: si el alto
     * cambia sin que cambie el ascenso, el texto queda pegado al borde de arriba
     * y no hay nada que avise.
     */
    Dictionary fuente_metricas() const;

    // -----------------------------------------------------------------------
    // La colección y la cruza
    // -----------------------------------------------------------------------

    /**
     * Cuántas criaturas caben en una partida.
     *
     * Vive acá y no en `breeding.h` porque del lado web tampoco es una regla del
     * núcleo: está en `main.ts`, en la capa de interfaz. Es un tope de
     * presentación —seis fichas entran en pantalla— y no una ley del juego. Pero
     * tiene que ser el MISMO número, o el nativo llenaría un criadero que la web
     * considera lleno.
     */
    static constexpr int MAX_CRIATURAS = 6;

    /**
     * Todas las criaturas de la partida, para poder elegir dos.
     *
     * Cada una trae además si puede cruzar y por qué no, porque esa es la
     * pregunta que la pantalla necesita contestar por criatura y calcularla del
     * lado de GDScript significaría reimplementar las reglas.
     */
    Array criaturas(int64_t ahora_ms) const;

    /** Cambia cuál es la activa. Devuelve false si ese id no existe. */
    bool activar(const String& id);

    /**
     * Las semillas encontradas en expediciones y todavía sin incubar.
     *
     * Cada una: { seed, linaje, rarezas }. El linaje y las rarezas se calculan
     * acá porque son lo único que se puede saber de una criatura antes de que
     * nazca, y es con eso que el jugador decide cuál incubar.
     */
    Array semillas() const;

    /**
     * Incuba una semilla: nace una criatura y pasa a ser la activa.
     *
     * { ok, mensaje, seed }. La semilla se saca de la lista.
     *
     * Es lo que hace que el criadero sea alcanzable. Sin incubar, una partida
     * nativa nunca tiene dos criaturas y la cruza queda como código que no se
     * puede ejecutar jugando — que es la peor clase de código, porque parece
     * terminado.
     */
    Dictionary incubar(const String& seed, int64_t ahora_ms, int64_t tz_min);

    /** ¿Puede cruzar este par? { puede: bool, motivo: String }. */
    Dictionary puede_cruzar(const String& id_a, const String& id_b, int64_t ahora_ms) const;

    /**
     * Los cruza. La cría nace, entra en la colección y pasa a ser la activa.
     *
     * { ok, mensaje, seed, descripcion, mutaciones, descubrimientos }
     *
     * `ahora_ms` hace de nonce, igual que en la web: es lo que hace que cruzar
     * la misma pareja dos veces no dé siempre el mismo hijo. `cruzar()` en sí es
     * determinista.
     */
    Dictionary cruzar(const String& id_a, const String& id_b, int64_t ahora_ms);

    // -----------------------------------------------------------------------
    // Codex
    // -----------------------------------------------------------------------

    /**
     * Lo descubierto: { linajes, formas, rarezas, total_registradas, progreso }.
     *
     * Los linajes y las formas vienen con su nombre legible además del id,
     * porque la pantalla los muestra y traducir índices a nombres del lado de
     * GDScript sería tener el catálogo de los dieciséis linajes escrito dos
     * veces — y esa es la clase de duplicación que se desincroniza.
     */
    Dictionary codex() const;

    // -----------------------------------------------------------------------
    // Guardado
    // -----------------------------------------------------------------------

    /**
     * Serializa la partida al formato de la web.
     *
     * Devuelve "" si no hay criatura. El archivo resultante lo puede abrir el
     * navegador: está verificado contra el validador real de la web con
     * `npm run validar-save`.
     */
    String guardar(int64_t ahora_ms) const;

    /**
     * Carga una partida guardada. Devuelve { ok: bool, mensaje: String }.
     *
     * Cuando falla, `mensaje` dice qué campo está mal. Un save corrupto tiene
     * que degradar a "empezá de nuevo", no dejar el juego a medio cargar.
     */
    Dictionary cargar(const String& texto);
};

} // namespace godot
