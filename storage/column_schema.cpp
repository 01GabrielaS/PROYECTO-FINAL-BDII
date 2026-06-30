#include "column_schema.h"
#include "../config/constants.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::vector<ColumnDef> ColumnSchemaParser::parse(const std::string& filepath) {
    std::vector<ColumnDef> columnas;
    std::ifstream file(filepath);
    if (!file.is_open()) return columnas;

    std::string line_original;
    while (std::getline(file, line_original)) {
        std::string line = line_original;

        // Conservamos el nombre de columna ANTES de pasar a mayúsculas
        // para no perder legibilidad, pero comparamos en mayúsculas.
        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
            return std::toupper(c);
        });
        std::replace(upper.begin(), upper.end(), ',', ' ');
        std::replace(upper.begin(), upper.end(), ';', ' ');

        if (upper.empty() ||
            upper.find("CREATE TABLE") != std::string::npos ||
            upper == ")")
        {
            continue;
        }

        std::stringstream ss(upper);
        std::string column_name, data_type;
        if (!(ss >> column_name >> data_type)) continue;

        FieldType type;
        uint32_t  bytes = 0;

        if (data_type.find("INT") != std::string::npos) {
            type  = FieldType::INT;
            bytes = SIZE_INT;
        } else if (data_type.find("FLOAT") != std::string::npos) {
            type  = FieldType::FLOAT;
            bytes = SIZE_FLOAT;
        } else {
            size_t open_paren  = data_type.find('(');
            size_t close_paren = data_type.find(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos &&
                close_paren > open_paren + 1)
            {
                std::string n_str = data_type.substr(open_paren + 1, close_paren - open_paren - 1);
                try { bytes = std::stoul(n_str); } catch (...) { bytes = 0; }
            }

            if (data_type.find("TIMESTAMP") != std::string::npos) { type = FieldType::TIMESTAMP; if (bytes == 0) bytes = 19; }
            else if (data_type.find("DATETIME") != std::string::npos) { type = FieldType::DATETIME; if (bytes == 0) bytes = 19; }
            else if (data_type.find("DATE") != std::string::npos) { type = FieldType::DATE; if (bytes == 0) bytes = 10; }
            else if (data_type.find("TIME") != std::string::npos) { type = FieldType::TIME; if (bytes == 0) bytes = 8; }
            else if (data_type.find("YEAR") != std::string::npos) { type = FieldType::YEAR; if (bytes == 0) bytes = 4; }
            else if (data_type.find("CHAR") != std::string::npos)      { type = FieldType::CHAR;  if (bytes == 0) bytes = 1; }
            else if (data_type.find("TEXT") != std::string::npos)      { type = FieldType::TEXT;  if (bytes == 0) bytes = 256; }
            else if (data_type.find("BLOB") != std::string::npos)      { type = FieldType::BLOB;  if (bytes == 0) bytes = 256; }
            else continue;   // tipo desconocido: se ignora la línea
        }

        if (bytes > 0)
            columnas.push_back({column_name, type, bytes});
    }

    return columnas;
}

std::vector<uint32_t> ColumnSchemaParser::field_sizes(const std::vector<ColumnDef>& schema) {
    std::vector<uint32_t> sizes;
    sizes.reserve(schema.size());
    for (const auto& col : schema) sizes.push_back(col.size);
    return sizes;
}
