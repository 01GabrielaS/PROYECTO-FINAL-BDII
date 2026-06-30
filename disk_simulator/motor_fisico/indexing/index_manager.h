#pragma once
#include "AVLIndex.h"
#include "../storage/column_key.h"
#include "../storage/column_schema.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

// ─────────────────────────────────────────────────────────────────
//  IndexManager — un AVLIndex por cada columna de la tabla (Tarea 6)
//
//  El AVLIndex actual exige clave única por nodo (no permite
//  duplicados con la misma clave). Como una columna SÍ puede tener
//  valores repetidos (dos alumnos con la misma nota, por ejemplo),
//  se indexa con clave compuesta:
//
//        std::pair<ColumnKey, uint32_t>
//                     │            └── start_lba (desambigua duplicados)
//                     └── valor de la columna (compara numérico o texto)
//
//  Esto es exactamente el mismo patrón que usa el nuevo main_avl.cpp
//  de P3 (clave = {valor, lba}).
//
//  Flujo:
//    1. Tras validar el esquema (Capa 1) -> crear_indices(schema)
//    2. Tras cada engine.insert_fields() exitoso -> indexar_registro()
//    3. P3 consulta con buscar_exacto()/buscar_rango() o accede al
//       AVLIndex crudo con get_indice() si necesita más control.
// ─────────────────────────────────────────────────────────────────
using ColIndexKey = std::pair<ColumnKey, uint32_t>;

class IndexManager {
public:
    // Crea un AVL vacío por cada columna del esquema. Llamar una
    // sola vez, justo después de engine.validate_schema().
    void crear_indices(const std::vector<ColumnDef>& schema);

    // Indexa una fila ya escrita en disco (RecordID y WriteResult
    // vienen de engine.insert_fields()).
    void indexar_registro(const std::vector<ColumnDef>&   schema,
                          const std::vector<std::string>& valores,
                          const RecordID&                 rid,
                          const WriteResult&               wr);

    // ── Consultas de conveniencia (para P3 / motor de búsquedas) ──
    // Búsqueda exacta: puede haber varios resultados (valores repetidos).
    std::vector<IndexEntry> buscar_exacto(const std::string& columna,
                                          const std::string& valor);

    // Búsqueda por rango [lo, hi] sobre una columna.
    std::vector<std::pair<ColIndexKey, IndexEntry>>
    buscar_rango(const std::string& columna,
                 const std::string& lo,
                 const std::string& hi);

    // Acceso de solo lectura al AVL crudo de una columna.
    AVLIndex<ColIndexKey>* get_indice(const std::string& columna);
    bool tiene_indice(const std::string& columna) const;
    std::vector<std::string> columnas() const;

private:
    std::unordered_map<std::string, std::unique_ptr<AVLIndex<ColIndexKey>>> avl_por_columna_;

    // ColumnSchemaParser guarda los nombres de columna en MAYÚSCULAS
    // (mismo comportamiento que el parser de P1). Normalizamos aquí
    // todas las búsquedas/inserciones para que dé igual si quien
    // llama escribe "nombre", "Nombre" o "NOMBRE".
    static std::string normalizar(const std::string& columna);
};
