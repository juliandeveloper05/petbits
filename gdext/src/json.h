#pragma once
/**
 * json.h — Un JSON mínimo, escrito para este proyecto.
 *
 * ---
 *
 * POR QUÉ NO SE USA UNA BIBLIOTECA.
 *
 * El guardado tiene que sobrevivir el viaje web → nativo → web sin perder nada,
 * y para eso hace falta una propiedad que las bibliotecas generales no siempre
 * garantizan: **el orden de las claves se conserva**. Un objeto guardado en un
 * std::map sale alfabetizado, y aunque JSON no le dé significado al orden, eso
 * convierte cada guardado en un diff enorme contra el anterior y hace imposible
 * comparar dos saves de un vistazo.
 *
 * Acá los objetos son un vector de pares, así que salen en el mismo orden en que
 * entraron.
 *
 * Lo otro es que esto se compila junto al núcleo, que es C++ puro y se verifica
 * con un solo comando. Meter una dependencia de novecientos kilobytes de header
 * para leer un archivo de dos kilobytes cambiaría eso a cambio de poco.
 */

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace petbits {

class Json {
public:
    enum class Tipo : uint8_t { Nulo, Bool, Numero, Texto, Arreglo, Objeto };

    Json() = default;
    static Json nulo();
    static Json booleano(bool v);
    static Json numero(double v);
    static Json texto(std::string v);
    static Json arreglo();
    static Json objeto();

    Tipo tipo() const { return tipo_; }
    bool esNulo() const { return tipo_ == Tipo::Nulo; }
    bool esNumero() const { return tipo_ == Tipo::Numero; }
    bool esTexto() const { return tipo_ == Tipo::Texto; }
    bool esArreglo() const { return tipo_ == Tipo::Arreglo; }
    bool esObjeto() const { return tipo_ == Tipo::Objeto; }

    bool comoBool(bool porDefecto = false) const;
    double comoNumero(double porDefecto = 0.0) const;
    /** Entero de 64 bits. Los tiempos del juego no entran en un double exacto por siempre. */
    int64_t comoEntero(int64_t porDefecto = 0) const;
    const std::string& comoTexto() const;

    /** Los elementos, si es arreglo. Vacío si no. */
    const std::vector<Json>& elementos() const { return elementos_; }
    void agregar(Json v);

    /** Los pares del objeto, en el orden en que se insertaron. */
    const std::vector<std::pair<std::string, Json>>& campos() const { return campos_; }

    /** Busca una clave. Devuelve nullptr si no está. */
    const Json* buscar(const std::string& clave) const;
    /** Inserta o reemplaza, conservando la posición original si ya existía. */
    void poner(std::string clave, Json valor);

    /** Serializa. Compacto, sin espacios: es lo que hace JSON.stringify. */
    std::string escribir() const;

    /**
     * Interpreta un texto JSON.
     *
     * Devuelve false y llena `error` si no se pudo. NO lanza: un guardado
     * corrupto tiene que degradar a "empezá de nuevo", no reventar el arranque
     * del juego. Y encima acá las excepciones están deshabilitadas.
     */
    static bool leer(const std::string& texto, Json& salida, std::string& error);

private:
    Tipo tipo_ = Tipo::Nulo;
    bool bool_ = false;
    double numero_ = 0.0;
    std::string texto_;
    std::vector<Json> elementos_;
    std::vector<std::pair<std::string, Json>> campos_;
};

} // namespace petbits
