#include "schema_parser.h"
#include "constants.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

std::vector<uint32_t> SchemaParser::parse_txt_schema(const std::string& filepath) {
    std::vector<uint32_t> field_sizes;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return field_sizes; // Retorna vacío si el archivo no existe
    }

    std::string line;
    while (std::getline(file, line)) {
        // Convertir la línea a mayúsculas para evitar problemas de mayúsculas/minúsculas
        std::transform(line.begin(), line.end(), line.begin(), [](unsigned char c) {
            return std::toupper(c);
        });

        // Limpiar caracteres molestos comunes en estructuras DDL
        std::replace(line.begin(), line.end(), ',', ' ');
        std::replace(line.begin(), line.end(), ';', ' ');

        // Ignorar líneas vacías o de apertura/cierre de tablas
        if (line.empty() || line.find("CREATE TABLE") != std::string::npos || line == ")") {
            continue;
        }

        std::stringstream ss(line);
        std::string column_name, data_type;
        
        // Leer el nombre de la columna y su tipo de dato
        if (!(ss >> column_name >> data_type)) {
            continue;
        }

        uint32_t bytes = 0;

        if (data_type.find("INT") != std::string::npos) {
            bytes = SIZE_INT;
        } 
        else if (data_type.find("FLOAT") != std::string::npos) {
            bytes = SIZE_FLOAT;
        } 
        else {
            // Manejo de CHAR(N), TEXT(N) y BLOB(N) -> Extraer el número dentro de los paréntesis
            size_t open_paren = data_type.find('(');
            size_t close_paren = data_type.find(')');
            
            if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren + 1) {
                std::string n_str = data_type.substr(open_paren + 1, close_paren - open_paren - 1);
                try {
                    bytes = std::stoul(n_str);
                } catch (...) {
                    bytes = 0;
                }
            }
        }

        if (bytes > 0) {
            field_sizes.push_back(bytes);
        }
    }

    return field_sizes;
}