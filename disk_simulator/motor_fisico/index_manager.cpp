#include "index_manager.h"
#include <limits>
#include <algorithm>
#include <cctype>

std::string IndexManager::normalizar(const std::string& columna) {
    std::string s = columna;
    std::transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    return s;
}

void IndexManager::crear_indices(const std::vector<ColumnDef>& schema) {
    avl_por_columna_.clear();
    for (const auto& col : schema)
        avl_por_columna_[normalizar(col.name)] = std::make_unique<AVLIndex<ColIndexKey>>();
}

void IndexManager::indexar_registro(const std::vector<ColumnDef>&   schema,
                                    const std::vector<std::string>& valores,
                                    const RecordID&                 rid,
                                    const WriteResult&               wr)
{
    for (size_t i = 0; i < schema.size() && i < valores.size(); ++i) {
        auto it = avl_por_columna_.find(normalizar(schema[i].name));
        if (it != avl_por_columna_.end()) {
            ColIndexKey key{ColumnKey(valores[i]), wr.start_lba};
            it->second->insert(key, rid, wr);
        }
    }
}

std::vector<IndexEntry> IndexManager::buscar_exacto(const std::string& columna,
                                                    const std::string& valor)
{
    std::vector<IndexEntry> resultado;
    auto* avl = get_indice(columna);
    if (!avl) return resultado;

    ColIndexKey lo{ColumnKey(valor), 0u};
    ColIndexKey hi{ColumnKey(valor), std::numeric_limits<uint32_t>::max()};

    for (auto& [key, entry] : avl->rangeSearch(lo, hi))
        resultado.push_back(entry);

    return resultado;
}

std::vector<std::pair<ColIndexKey, IndexEntry>>
IndexManager::buscar_rango(const std::string& columna,
                           const std::string& lo,
                           const std::string& hi)
{
    auto* avl = get_indice(columna);
    if (!avl) return {};

    ColIndexKey k_lo{ColumnKey(lo), 0u};
    ColIndexKey k_hi{ColumnKey(hi), std::numeric_limits<uint32_t>::max()};
    return avl->rangeSearch(k_lo, k_hi);
}

AVLIndex<ColIndexKey>* IndexManager::get_indice(const std::string& columna) {
    auto it = avl_por_columna_.find(normalizar(columna));
    return it != avl_por_columna_.end() ? it->second.get() : nullptr;
}

bool IndexManager::tiene_indice(const std::string& columna) const {
    return avl_por_columna_.count(normalizar(columna)) > 0;
}

std::vector<std::string> IndexManager::columnas() const {
    std::vector<std::string> cols;
    cols.reserve(avl_por_columna_.size());
    for (const auto& [nombre, _] : avl_por_columna_)
        cols.push_back(nombre);
    return cols;
}
