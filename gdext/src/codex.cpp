#include "codex.h"

#include "js_math.h"

#include <algorithm>

namespace petbits {

std::string_view nombreTipo(TipoDescubrimiento t) {
    switch (t) {
        case TipoDescubrimiento::Linaje: return "linaje";
        case TipoDescubrimiento::Forma:  return "forma";
        case TipoDescubrimiento::Rareza: return "rareza";
    }
    return "?";
}

Registro registrar(const Codex& codex, Seed seed, Form forma) {
    const Genes genes = decodeGenome(seed);

    Registro r;
    r.codex = codex;

    const int linaje = static_cast<int>(genes.lineage);
    if (std::find(r.codex.linajes.begin(), r.codex.linajes.end(), linaje) ==
        r.codex.linajes.end()) {
        r.codex.linajes.push_back(linaje);
        r.nuevos.push_back({TipoDescubrimiento::Linaje, std::to_string(linaje),
                            std::string(lineageName(genes))});
    }

    // "Indefinida" no es un descubrimiento: es la ausencia de uno.
    if (forma != Form::Indefinida &&
        std::find(r.codex.formas.begin(), r.codex.formas.end(), forma) ==
            r.codex.formas.end()) {
        r.codex.formas.push_back(forma);
        r.nuevos.push_back(
            {TipoDescubrimiento::Forma, std::string(formId(forma)), std::string(formName(forma))});
    }

    for (const Trait& trait : detectTraits(seed)) {
        const std::string id(trait.id);
        if (std::find(r.codex.rarezas.begin(), r.codex.rarezas.end(), id) ==
            r.codex.rarezas.end()) {
            r.codex.rarezas.push_back(id);
            r.nuevos.push_back({TipoDescubrimiento::Rareza, id, std::string(trait.name)});
        }
    }

    // Ordenados para que el guardado sea estable: si no, dos partidas
    // equivalentes producen JSON distinto y cualquier comparación miente.
    //
    // Los tres órdenes son distintos entre sí y ninguno es el obvio:
    std::sort(r.codex.linajes.begin(), r.codex.linajes.end());
    // Las formas van por su ID EN TEXTO y no por el valor del enum. El enum
    // arranca en Indefinida, Petreo, Vaporoso…; alfabéticamente el primero es
    // "coloso". Ordenar por enum daría otro archivo, y el validador de la web no
    // se quejaría — solo dejaría de coincidir con lo que escribe el navegador.
    std::sort(r.codex.formas.begin(), r.codex.formas.end(),
              [](Form a, Form b) { return formId(a) < formId(b); });
    std::sort(r.codex.rarezas.begin(), r.codex.rarezas.end());

    r.codex.totalRegistradas = codex.totalRegistradas + 1;
    return r;
}

ProgresoCodex progresoCodex(const Codex& codex) {
    ProgresoCodex p;
    p.linajes = {static_cast<int>(codex.linajes.size()), static_cast<int>(LINEAGES.size())};
    p.formas = {static_cast<int>(codex.formas.size()),
                static_cast<int>(formasColeccionables().size())};
    p.rarezas = {static_cast<int>(codex.rarezas.size()),
                 static_cast<int>(TRAIT_CATALOG.size())};

    const int vistos = p.linajes.vistos + p.formas.vistos + p.rarezas.vistos;
    const int total = p.linajes.total + p.formas.total + p.rarezas.total;

    // jsRound y no std::round: el Math.round de JavaScript redondea el .5 hacia
    // arriba y std::round lo aleja del cero, que difieren en los negativos. Acá
    // nunca hay negativos, pero usar el helper del proyecto es más barato que
    // dejar escrito que "en este caso da igual" y que alguien lo mueva.
    p.porcentaje = static_cast<int>(
        jsRound(static_cast<double>(vistos) / static_cast<double>(total) * 100.0));
    return p;
}

bool conoceLinaje(const Codex& codex, int lineage) {
    return std::find(codex.linajes.begin(), codex.linajes.end(), lineage) != codex.linajes.end();
}

bool conoceForma(const Codex& codex, Form forma) {
    return std::find(codex.formas.begin(), codex.formas.end(), forma) != codex.formas.end();
}

bool conoceRareza(const Codex& codex, std::string_view id) {
    return std::find(codex.rarezas.begin(), codex.rarezas.end(), id) != codex.rarezas.end();
}

} // namespace petbits
