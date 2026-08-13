/**
 * Escribe un save con el C++, para que lo valide la web.
 *
 *   escribir_save.exe ruta/al/save.json
 *
 * ---
 *
 * POR QUÉ HACE FALTA, SI YA HAY TESTS DE GUARDADO.
 *
 * Los tests de test_parity.cpp comprueban que el C++ lee un save de la web y que
 * lo que escribe lo puede volver a leer él mismo. Eso deja afuera justo la
 * dirección que importa para la promesa del proyecto: que la WEB pueda leer lo
 * que escribió el nativo.
 *
 * Un round-trip contra uno mismo es fácil de pasar estando equivocado —basta con
 * equivocarse igual al leer y al escribir—. La única prueba que vale es que el
 * validador de la web, que no sabe nada de este código, acepte el archivo.
 *
 * Este programa produce el archivo; `npm run validar-save` lo pasa por el
 * parseSave real, con su esquema de Zod y todo.
 */

#include "../src/save_manager.h"
#include "../src/simulation.h"

#include <cstdio>
#include <string>

using namespace petbits;

int main(int argc, char** argv) {
    const std::string destino = argc > 1 ? argv[1] : "save_del_cpp.json";

    // Una criatura con historia: simular la lleva a juvenil y le deja los stats
    // en valores acumulados de quince dígitos, que es donde un guardado pierde
    // precisión si el número se escribe con menos dígitos de los necesarios.
    const int64_t base = 1786406400000LL;
    CreatureState c = createCreature(0xA3F091C477BE2D08ULL, base, -180);
    c = simulate(c, base + 1441 * TICK_MS).state;

    Partida p = partidaInicial(c);

    // Se le mete contenido al codex y al inventario para que el archivo ejercite
    // también las partes que el C++ no interpreta.
    Json codex = Json::objeto();
    Json linajes = Json::arreglo();
    linajes.agregar(Json::numero(2));
    linajes.agregar(Json::numero(8));
    Json formas = Json::arreglo();
    formas.agregar(Json::texto("petreo"));
    Json rarezas = Json::arreglo();
    rarezas.agregar(Json::texto("primordial"));
    codex.poner("linajes", std::move(linajes));
    codex.poner("formas", std::move(formas));
    codex.poner("rarezas", std::move(rarezas));
    codex.poner("totalRegistradas", Json::numero(4));

    Json inventario = Json::objeto();
    inventario.poner("baya", Json::numero(3));
    inventario.poner("larva", Json::numero(1));

    Json semillas = Json::arreglo();
    // Un genoma por encima de 2^53, que es donde un number de JavaScript deja de
    // ser exacto. Si en algún tramo se convirtiera a número, volvería redondeado.
    semillas.agregar(Json::texto("11814994175403368200"));

    p.otros.poner("codex", std::move(codex));
    p.otros.poner("inventario", std::move(inventario));
    p.otros.poner("semillas", std::move(semillas));

    const std::string texto = guardarPartida(p, base + 1441 * TICK_MS);

    std::FILE* f = std::fopen(destino.c_str(), "wb");
    if (f == nullptr) {
        std::printf("No se pudo abrir %s para escribir\n", destino.c_str());
        return 1;
    }
    std::fwrite(texto.data(), 1, texto.size(), f);
    std::fclose(f);

    std::printf("Escrito %s (%zu bytes)\n", destino.c_str(), texto.size());
    return 0;
}
