#pragma once
#include "disk_engine.h"
#include "record_serializer.h"
#include "index_manager.h"
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  Resultado de una carga de CSV, para mostrar en el Panel B (P3)
// ─────────────────────────────────────────────────────────────────
struct CargaResultado {
    uint32_t    filas_leidas     = 0;   // total de filas en el CSV
    uint32_t    filas_insertadas = 0;
    uint32_t    filas_omitidas   = 0;
    bool        carga_parcial    = false;
    std::string mensaje;
};

// ─────────────────────────────────────────────────────────────────
//  CsvLoader — Tarea 5
//
//  Capa 3 (validación GLOBAL, antes de insertar nada):
//    sectores_por_fila   = engine.sectors_needed_for_fields(field_sizes)
//    sectores_necesarios = filas_csv * sectores_por_fila
//    sectores_libres     = engine.free_sectors()
//
//    Si necesarios > libres:
//      - permitir_carga_parcial=true  -> carga floor(libres/por_fila) filas
//      - permitir_carga_parcial=false -> no inserta nada, retorna error
// ─────────────────────────────────────────────────────────────────
class CsvLoader {
public:
    static CargaResultado cargar(const std::string&            filepath,
                                 const std::vector<ColumnDef>& schema,
                                 DiskEngine&                   engine,
                                 IndexManager&                 indices,
                                 uint16_t                       table_id,
                                 bool                            permitir_carga_parcial = true);

private:
    static std::vector<std::string> split_csv_line(const std::string& linea);
};
