#pragma once
#include "column_schema.h"
#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  RecordSerializer — convierte una fila de texto (CSV) en los
//  bytes que requiere DiskEngine::insert_fields()  (Tarea 4)
//
//  'storage' debe seguir vivo mientras se usa el resultado (los
//  punteros de 'fields' apuntan dentro de storage). Por eso se pasa
//  por referencia y se reutiliza fila por fila desde el loader.
// ─────────────────────────────────────────────────────────────────
class RecordSerializer {
public:
    static std::vector<std::pair<const uint8_t*, uint32_t>> serialize_row(
        const std::vector<std::string>&    valores,
        const std::vector<ColumnDef>&      schema,
        std::vector<std::vector<uint8_t>>& storage);

private:
    static void serialize_int  (const std::string& v, std::vector<uint8_t>& buf);
    static void serialize_float(const std::string& v, std::vector<uint8_t>& buf);
    static void serialize_text (const std::string& v, uint32_t size, std::vector<uint8_t>& buf);
};
