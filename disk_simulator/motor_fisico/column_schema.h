#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────
//  ColumnDef / FieldType
//
//  P1 (SchemaParser::parse_txt_schema) ya parsea el .txt de esquema,
//  pero solo devuelve vector<uint32_t> field_sizes (descarta nombre
//  y tipo). Para Tareas 4-6 necesitamos también el NOMBRE de cada
//  columna (para indexar por columna) y el TIPO (para saber cómo
//  serializar: INT/FLOAT en binario vs CHAR/TEXT/BLOB como bytes).
//
//  Por eso ColumnSchemaParser parsea el MISMO formato de archivo,
//  pero conservando los 3 datos. Si P1 llega a exponer su propio
//  vector<ColumnDef>, este archivo se reemplaza por el de P1 y todo
//  lo demás (record_serializer, csv_loader, index_manager) sigue
//  funcionando igual, porque solo dependen de ColumnDef.
// ─────────────────────────────────────────────────────────────────
enum class FieldType { INT, FLOAT, CHAR, TEXT, BLOB };

struct ColumnDef {
    std::string name;
    FieldType   type;
    uint32_t    size;   // bytes: 4 para INT/FLOAT, N para CHAR/TEXT/BLOB
};

class ColumnSchemaParser {
public:
    // Lee el .txt de esquema (mismo formato que usa P1) y devuelve
    // la lista completa de columnas con nombre, tipo y tamaño.
    static std::vector<ColumnDef> parse(const std::string& filepath);

    // Atajo: extrae solo los tamaños (equivalente a lo que da P1),
    // útil para llamar engine.validate_schema() / sectors_needed_for_fields().
    static std::vector<uint32_t> field_sizes(const std::vector<ColumnDef>& schema);
};
