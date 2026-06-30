#pragma once
#include "../indexing/AVLIndex.h"
#include "disk_engine.h"
#include <vector>
#include <string>
#include <limits>

// ── Resultado de una consulta ────────────────────────────────────
// Agrupa el recorrido del AVL y los datos leídos del disco
// para cada registro encontrado.
struct QueryResult {
    std::vector<std::string> traversal;   // log del recorrido AVL

    struct Hit {
        IndexEntry entry;       // info del AVL: lba, num_sectors, record_id
        ReadResult disk_data;   // bytes y cadena de sectores leídos del disco
    };

    std::vector<Hit> hits;
};

// ── Motor de consultas ───────────────────────────────────────────
// Une el AVLIndex con el DiskEngine:
//   1. Busca en el AVL → obtiene traversal + entries
//   2. Por cada entry llama engine.read() → obtiene los bytes reales
//
// Los métodos son templates para soportar cualquier tipo de clave
class QueryEngine {
    DiskEngine& engine_;

public:
    explicit QueryEngine(DiskEngine& engine) : engine_(engine) {}

    // Búsqueda exacta por valor de columna.
    // Internamente usa rangeSearch({val,0}, {val,UINT32_MAX})
    // porque la clave es compuesta y puede haber duplicados.
    template<typename KeyType>
    QueryResult searchExact(AVLIndex<KeyType>& avl,
                            const typename KeyType::first_type& val) {
        QueryResult result;
        auto range = avl.rangeSearch({val, 0},
                                     {val, std::numeric_limits<uint32_t>::max()});
        result.traversal = std::move(range.traversal);
        for (auto& [key, entry] : range.matches) {
            QueryResult::Hit hit;
            hit.entry     = entry;
            hit.disk_data = engine_.read(entry.start_lba);
            result.hits.push_back(std::move(hit));
        }
        return result;
    }

    // Búsqueda por rango [lo, hi] sobre el valor de columna.
    template<typename KeyType>
    QueryResult searchRange(AVLIndex<KeyType>& avl,
                            const typename KeyType::first_type& lo,
                            const typename KeyType::first_type& hi) {
        QueryResult result;
        auto range = avl.rangeSearch({lo, 0},
                                     {hi, std::numeric_limits<uint32_t>::max()});
        result.traversal = std::move(range.traversal);
        for (auto& [key, entry] : range.matches) {
            QueryResult::Hit hit;
            hit.entry     = entry;
            hit.disk_data = engine_.read(entry.start_lba);
            result.hits.push_back(std::move(hit));
        }
        return result;
    }
};