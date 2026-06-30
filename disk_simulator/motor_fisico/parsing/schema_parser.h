#pragma once
#include <string>
#include <vector>
#include <cstdint>

class SchemaParser {
public:
    // Lee el archivo .txt línea por línea, identifica los tipos de datos,
    // calcula su tamaño binario y retorna el vector de field_sizes para Iair.
    static std::vector<uint32_t> parse_txt_schema(const std::string& filepath);
};