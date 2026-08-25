/**
 * json.cpp — ver json.h.
 */

#include "json.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <system_error>

namespace petbits {

// ---------------------------------------------------------------------------
// Construcción y acceso
// ---------------------------------------------------------------------------

Json Json::nulo() { return Json(); }

Json Json::booleano(bool v) {
    Json j;
    j.tipo_ = Tipo::Bool;
    j.bool_ = v;
    return j;
}

Json Json::numero(double v) {
    Json j;
    j.tipo_ = Tipo::Numero;
    j.numero_ = v;
    return j;
}

Json Json::texto(std::string v) {
    Json j;
    j.tipo_ = Tipo::Texto;
    j.texto_ = std::move(v);
    return j;
}

Json Json::arreglo() {
    Json j;
    j.tipo_ = Tipo::Arreglo;
    return j;
}

Json Json::objeto() {
    Json j;
    j.tipo_ = Tipo::Objeto;
    return j;
}

bool Json::comoBool(bool porDefecto) const {
    return tipo_ == Tipo::Bool ? bool_ : porDefecto;
}

double Json::comoNumero(double porDefecto) const {
    return tipo_ == Tipo::Numero ? numero_ : porDefecto;
}

int64_t Json::comoEntero(int64_t porDefecto) const {
    if (tipo_ != Tipo::Numero) return porDefecto;
    return static_cast<int64_t>(numero_);
}

const std::string& Json::comoTexto() const {
    static const std::string vacio;
    return tipo_ == Tipo::Texto ? texto_ : vacio;
}

void Json::agregar(Json v) {
    tipo_ = Tipo::Arreglo;
    elementos_.push_back(std::move(v));
}

const Json* Json::buscar(const std::string& clave) const {
    for (const auto& [k, v] : campos_) {
        if (k == clave) return &v;
    }
    return nullptr;
}

void Json::poner(std::string clave, Json valor) {
    tipo_ = Tipo::Objeto;
    for (auto& [k, v] : campos_) {
        if (k == clave) {
            v = std::move(valor);
            return;
        }
    }
    campos_.emplace_back(std::move(clave), std::move(valor));
}

// ---------------------------------------------------------------------------
// Escritura
// ---------------------------------------------------------------------------

static void escribirTexto(const std::string& s, std::string& salida) {
    salida += '"';
    for (const char c : s) {
        switch (c) {
            case '"':  salida += "\\\""; break;
            case '\\': salida += "\\\\"; break;
            case '\n': salida += "\\n";  break;
            case '\r': salida += "\\r";  break;
            case '\t': salida += "\\t";  break;
            case '\b': salida += "\\b";  break;
            case '\f': salida += "\\f";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    salida += buf;
                } else {
                    // Los bytes de arriba de 0x7F se copian tal cual: ya son
                    // UTF-8 válido y JSON los acepta sin escapar.
                    salida += c;
                }
        }
    }
    salida += '"';
}

/**
 * Un número, escrito **exactamente** como lo escribe `JSON.stringify`.
 *
 * ---
 *
 * POR QUÉ IMPORTA QUE SEA EXACTO Y NO PARECIDO.
 *
 * Antes esto era `%.17g`, y andábamos bien en cuanto a precisión: diecisiete
 * dígitos significativos garantizan recuperar el mismo double, así que
 * 93.504000000000005 y 93.504 son el mismo número bit por bit.
 *
 * Pero los dos lados escribían el MISMO valor con cadenas distintas, y eso se
 * notó recién al pasar una partida real por web → nativo → web: cada salto
 * cambiaba el aspecto de casi todos los números. Un diff entre dos saves daba
 * diferencias en todos lados aunque nada hubiera cambiado.
 *
 * ---
 *
 * Y POR QUÉ `to_chars` SOLO NO ALCANZA.
 *
 * Acá decía "con to_chars los dos lados producen byte a byte lo mismo". Es casi
 * cierto, y casi no sirve. `std::to_chars` sin formato da la cadena más corta
 * que vuelve al mismo double —que es la mitad del algoritmo de JavaScript— pero
 * la OTRA mitad, cómo se acomodan esos dígitos, es distinta:
 *
 *   - `to_chars` elige entre fija y científica la que dé menos caracteres.
 *     JavaScript usa fija en todo el rango [1e-6, 1e21) aunque salga más larga.
 *     Un 0.000010238147347856556 sale `1.0238147347856556e-05` de un lado.
 *   - `to_chars` rellena el exponente a dos dígitos, como printf: `1e-07`.
 *     JavaScript escribe `1e-7`.
 *   - El atajo de enteros con `%.0f` escribía `-0` para el cero negativo.
 *     `JSON.stringify(-0)` es `0`.
 *
 * Sorteando cien mil doubles al azar por sus bits, mil seiscientos diecisiete
 * salían distintos — el 1,6%. Ninguno alcanzable con los valores que hay hoy en
 * un save (los stats van de 0 a 100 y las marcas de tiempo andan por 1,7e12),
 * así que no rompía nada; pero "byte a byte" no es una promesa que se pueda
 * sostener por casualidad, y el día que aparezca un campo con un flotante chico
 * la casualidad se termina.
 *
 * Así que acá está el algoritmo de verdad: `Number::toString` de ECMAScript
 * (§6.1.6.1.20). `to_chars` en modo científico da los dígitos más cortos `s` y
 * el exponente; el resto es acomodarlos como manda la norma. Con esto los cien
 * mil coinciden, y también el cero negativo, los denormales y DBL_MAX.
 */
static void escribirNumero(double v, std::string& salida) {
    if (!std::isfinite(v)) {
        // JSON no tiene infinito ni NaN. JSON.stringify escribe null; se hace
        // lo mismo para no producir un archivo que el otro lado no pueda leer.
        salida += "null";
        return;
    }

    // Antes del signo: `JSON.stringify(-0)` es "0".
    if (v == 0.0) {
        salida += '0';
        return;
    }

    if (v < 0) {
        salida += '-';
        v = -v;
    }

    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof(buf), v, std::chars_format::scientific);
    if (r.ec != std::errc()) {
        // No debería pasar con 64 bytes, pero si pasa es mejor un número feo que
        // un archivo cortado.
        std::snprintf(buf, sizeof(buf), "%.17g", v);
        salida += buf;
        return;
    }

    const std::string cientifica(buf, r.ptr);
    const std::size_t pos_e = cientifica.find('e');
    std::string digitos = cientifica.substr(0, pos_e);
    const int exponente = std::atoi(cientifica.c_str() + pos_e + 1);
    digitos.erase(std::remove(digitos.begin(), digitos.end(), '.'), digitos.end());

    // La convención de la norma: v = s × 10^(n-k), con k dígitos en s.
    // `to_chars` da d.ddd × 10^E, o sea s × 10^(E-(k-1)), así que n = E + 1.
    const int k = static_cast<int>(digitos.size());
    const int n = exponente + 1;

    if (k <= n && n <= 21) {
        // Entero: los dígitos y ceros hasta la coma.
        salida += digitos;
        salida.append(static_cast<std::size_t>(n - k), '0');
    } else if (0 < n && n <= 21) {
        // Coma adentro de los dígitos.
        salida.append(digitos, 0, static_cast<std::size_t>(n));
        salida += '.';
        salida.append(digitos, static_cast<std::size_t>(n), std::string::npos);
    } else if (-6 < n && n <= 0) {
        // Chico pero no tanto: JavaScript lo escribe con ceros, no en científica.
        salida += "0.";
        salida.append(static_cast<std::size_t>(-n), '0');
        salida += digitos;
    } else {
        // Científica, con el exponente SIN rellenar de ceros.
        if (k == 1) {
            salida += digitos;
        } else {
            salida += digitos[0];
            salida += '.';
            salida.append(digitos, 1, std::string::npos);
        }
        salida += 'e';
        salida += (n - 1 < 0) ? '-' : '+';
        salida += std::to_string(n - 1 < 0 ? -(n - 1) : (n - 1));
    }
}

static void escribirValor(const Json& j, std::string& salida) {
    switch (j.tipo()) {
        case Json::Tipo::Nulo:
            salida += "null";
            break;
        case Json::Tipo::Bool:
            salida += j.comoBool() ? "true" : "false";
            break;
        case Json::Tipo::Numero:
            escribirNumero(j.comoNumero(), salida);
            break;
        case Json::Tipo::Texto:
            escribirTexto(j.comoTexto(), salida);
            break;
        case Json::Tipo::Arreglo: {
            salida += '[';
            bool primero = true;
            for (const Json& e : j.elementos()) {
                if (!primero) salida += ',';
                escribirValor(e, salida);
                primero = false;
            }
            salida += ']';
            break;
        }
        case Json::Tipo::Objeto: {
            salida += '{';
            bool primero = true;
            for (const auto& [k, v] : j.campos()) {
                if (!primero) salida += ',';
                escribirTexto(k, salida);
                salida += ':';
                escribirValor(v, salida);
                primero = false;
            }
            salida += '}';
            break;
        }
    }
}

std::string Json::escribir() const {
    std::string salida;
    escribirValor(*this, salida);
    return salida;
}

// ---------------------------------------------------------------------------
// Lectura
// ---------------------------------------------------------------------------

namespace {

/** Descenso recursivo. Sin excepciones: el error viaja en el valor de retorno. */
class Lector {
public:
    Lector(const std::string& texto) : t_(texto) {}

    bool valor(Json& salida);
    const std::string& error() const { return error_; }

private:
    const std::string& t_;
    size_t i_ = 0;
    std::string error_;
    /** Tope de anidamiento. Un archivo hostil con mil corchetes desbordaría la pila. */
    int profundidad_ = 0;
    static constexpr int MAX_PROFUNDIDAD = 64;

    void espacios();
    bool falla(const char* que);
    bool literal(const char* esperado);
    bool cadena(std::string& salida);
    bool numeroLit(Json& salida);
};

void Lector::espacios() {
    while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\t' || t_[i_] == '\n' || t_[i_] == '\r')) {
        ++i_;
    }
}

bool Lector::falla(const char* que) {
    if (error_.empty()) {
        error_ = std::string(que) + " en la posición " + std::to_string(i_);
    }
    return false;
}

bool Lector::literal(const char* esperado) {
    const size_t largo = std::char_traits<char>::length(esperado);
    if (t_.compare(i_, largo, esperado) != 0) return falla("literal inesperado");
    i_ += largo;
    return true;
}

bool Lector::cadena(std::string& salida) {
    if (i_ >= t_.size() || t_[i_] != '"') return falla("se esperaba una cadena");
    ++i_;
    salida.clear();

    while (i_ < t_.size()) {
        const char c = t_[i_];
        if (c == '"') {
            ++i_;
            return true;
        }
        if (c != '\\') {
            salida += c;
            ++i_;
            continue;
        }

        ++i_;
        if (i_ >= t_.size()) return falla("escape sin terminar");
        switch (t_[i_]) {
            case '"':  salida += '"';  break;
            case '\\': salida += '\\'; break;
            case '/':  salida += '/';  break;
            case 'n':  salida += '\n'; break;
            case 'r':  salida += '\r'; break;
            case 't':  salida += '\t'; break;
            case 'b':  salida += '\b'; break;
            case 'f':  salida += '\f'; break;
            case 'u': {
                if (i_ + 4 >= t_.size()) return falla("escape \\u incompleto");
                unsigned punto = 0;
                for (int k = 1; k <= 4; ++k) {
                    const char h = t_[i_ + static_cast<size_t>(k)];
                    unsigned d;
                    if (h >= '0' && h <= '9') d = static_cast<unsigned>(h - '0');
                    else if (h >= 'a' && h <= 'f') d = static_cast<unsigned>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') d = static_cast<unsigned>(h - 'A' + 10);
                    else return falla("dígito hexadecimal inválido en \\u");
                    punto = (punto << 4) | d;
                }
                i_ += 4;
                // Se vuelca como UTF-8. Los pares suplentes se dejan pasar como
                // dos secuencias de tres bytes: no aparecen en este formato, y
                // resolverlos bien pediría mirar el escape siguiente.
                if (punto < 0x80) {
                    salida += static_cast<char>(punto);
                } else if (punto < 0x800) {
                    salida += static_cast<char>(0xC0 | (punto >> 6));
                    salida += static_cast<char>(0x80 | (punto & 0x3F));
                } else {
                    salida += static_cast<char>(0xE0 | (punto >> 12));
                    salida += static_cast<char>(0x80 | ((punto >> 6) & 0x3F));
                    salida += static_cast<char>(0x80 | (punto & 0x3F));
                }
                break;
            }
            default:
                return falla("escape desconocido");
        }
        ++i_;
    }
    return falla("cadena sin cerrar");
}

bool Lector::numeroLit(Json& salida) {
    const size_t desde = i_;
    if (i_ < t_.size() && (t_[i_] == '-' || t_[i_] == '+')) ++i_;
    while (i_ < t_.size() && ((t_[i_] >= '0' && t_[i_] <= '9') || t_[i_] == '.' || t_[i_] == 'e' ||
                             t_[i_] == 'E' || t_[i_] == '-' || t_[i_] == '+')) {
        ++i_;
    }
    if (i_ == desde) return falla("se esperaba un número");

    const std::string trozo = t_.substr(desde, i_ - desde);
    char* fin = nullptr;
    const double v = std::strtod(trozo.c_str(), &fin);
    if (fin == trozo.c_str()) return falla("número mal formado");

    salida = Json::numero(v);
    return true;
}

bool Lector::valor(Json& salida) {
    if (++profundidad_ > MAX_PROFUNDIDAD) {
        --profundidad_;
        return falla("demasiado anidamiento");
    }
    struct Salir {
        int& p;
        ~Salir() { --p; }
    } salir{profundidad_};

    espacios();
    if (i_ >= t_.size()) return falla("fin de archivo inesperado");

    const char c = t_[i_];

    if (c == 'n') {
        if (!literal("null")) return false;
        salida = Json::nulo();
        return true;
    }
    if (c == 't') {
        if (!literal("true")) return false;
        salida = Json::booleano(true);
        return true;
    }
    if (c == 'f') {
        if (!literal("false")) return false;
        salida = Json::booleano(false);
        return true;
    }
    if (c == '"') {
        std::string s;
        if (!cadena(s)) return false;
        salida = Json::texto(std::move(s));
        return true;
    }
    if (c == '[') {
        ++i_;
        salida = Json::arreglo();
        espacios();
        if (i_ < t_.size() && t_[i_] == ']') {
            ++i_;
            return true;
        }
        while (true) {
            Json e;
            if (!valor(e)) return false;
            salida.agregar(std::move(e));
            espacios();
            if (i_ >= t_.size()) return falla("arreglo sin cerrar");
            if (t_[i_] == ',') { ++i_; continue; }
            if (t_[i_] == ']') { ++i_; return true; }
            return falla("se esperaba ',' o ']'");
        }
    }
    if (c == '{') {
        ++i_;
        salida = Json::objeto();
        espacios();
        if (i_ < t_.size() && t_[i_] == '}') {
            ++i_;
            return true;
        }
        while (true) {
            espacios();
            std::string clave;
            if (!cadena(clave)) return false;
            espacios();
            if (i_ >= t_.size() || t_[i_] != ':') return falla("se esperaba ':'");
            ++i_;
            Json v;
            if (!valor(v)) return false;
            salida.poner(std::move(clave), std::move(v));
            espacios();
            if (i_ >= t_.size()) return falla("objeto sin cerrar");
            if (t_[i_] == ',') { ++i_; continue; }
            if (t_[i_] == '}') { ++i_; return true; }
            return falla("se esperaba ',' o '}'");
        }
    }

    return numeroLit(salida);
}

} // namespace

bool Json::leer(const std::string& texto, Json& salida, std::string& error) {
    Lector lector(texto);
    if (!lector.valor(salida)) {
        error = lector.error();
        return false;
    }
    return true;
}

} // namespace petbits
