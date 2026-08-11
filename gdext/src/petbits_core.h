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

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class PetBitsCore : public RefCounted {
    GDCLASS(PetBitsCore, RefCounted)

protected:
    static void _bind_methods();

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
};

} // namespace godot
