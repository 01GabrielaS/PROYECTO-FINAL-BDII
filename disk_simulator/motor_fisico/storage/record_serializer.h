#pragma once
#include "column_schema.h"
#include <cstdint>
#include <string>
#include <vector>

class RecordSerializer {
public:
    static std::vector<std::pair<const uint8_t*, uint32_t>> serialize_row(
        const std::vector<std::string>&    valores,
        const std::vector<ColumnDef>&      schema,
        std::vector<std::vector<uint8_t>>& storage);

    static std::vector<std::string> deserialize_row(
        const std::vector<uint8_t>& data,
        const std::vector<ColumnDef>& schema,
        uint32_t useful_bytes);

private:
    static void serialize_int  (const std::string& v, std::vector<uint8_t>& buf);
    static void serialize_float(const std::string& v, std::vector<uint8_t>& buf);
    static void serialize_text (const std::string& v, uint32_t size, std::vector<uint8_t>& buf);
};
