#include "record_serializer.h"
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

void RecordSerializer::serialize_int(const std::string& v, std::vector<uint8_t>& buf) {
    int32_t val = v.empty() ? 0 : std::stoi(v);
    buf.resize(sizeof(int32_t));
    std::memcpy(buf.data(), &val, sizeof(int32_t));
}

void RecordSerializer::serialize_float(const std::string& v, std::vector<uint8_t>& buf) {
    float val = v.empty() ? 0.0f : std::stof(v);
    buf.resize(sizeof(float));
    std::memcpy(buf.data(), &val, sizeof(float));
}

void RecordSerializer::serialize_text(const std::string& v, uint32_t size, std::vector<uint8_t>& buf) {
    buf.assign(size, 0x00);
    uint32_t n = std::min<uint32_t>(size, static_cast<uint32_t>(v.size()));
    std::memcpy(buf.data(), v.data(), n);
}

std::vector<std::pair<const uint8_t*, uint32_t>> RecordSerializer::serialize_row(
    const std::vector<std::string>&    valores,
    const std::vector<ColumnDef>&      schema,
    std::vector<std::vector<uint8_t>>& storage)
{
    if (valores.size() != schema.size())
        throw std::runtime_error(
            "RecordSerializer: la fila tiene " + std::to_string(valores.size()) +
            " columnas pero el esquema define " + std::to_string(schema.size()));

    storage.clear();
    storage.resize(schema.size());

    for (size_t i = 0; i < schema.size(); ++i) {
        const ColumnDef& col = schema[i];
        switch (col.type) {
            case FieldType::INT:   serialize_int(valores[i], storage[i]);            break;
            case FieldType::FLOAT: serialize_float(valores[i], storage[i]);          break;
            case FieldType::CHAR:
            case FieldType::TEXT:
            case FieldType::BLOB:
            case FieldType::DATE:
            case FieldType::TIME:
            case FieldType::DATETIME:
            case FieldType::TIMESTAMP:
            case FieldType::YEAR:  serialize_text(valores[i], col.size, storage[i]); break;
        }
    }

    std::vector<std::pair<const uint8_t*, uint32_t>> fields;
    fields.reserve(schema.size());
    for (size_t i = 0; i < schema.size(); ++i)
        fields.push_back({storage[i].data(), static_cast<uint32_t>(storage[i].size())});

    return fields;
}

std::vector<std::string> RecordSerializer::deserialize_row(
    const std::vector<uint8_t>& data,
    const std::vector<ColumnDef>& schema,
    uint32_t useful_bytes)
{
    std::vector<std::string> valores;
    valores.reserve(schema.size());

    uint32_t cursor      = 0;
    uint32_t sector_base = 0;

    for (const auto& col : schema) {
        uint32_t fs = col.size;
        if (fs == 0) { valores.push_back(""); continue; }

        if (cursor + fs > useful_bytes) {
            sector_base += useful_bytes;
            cursor = 0;
        }

        uint32_t offset = sector_base + cursor;

        if (offset + fs > static_cast<uint32_t>(data.size())) {
            valores.push_back("");
            cursor += fs;
            continue;
        }

        if (col.type == FieldType::INT) {
            int32_t val = 0;
            std::memcpy(&val, data.data() + offset, sizeof(int32_t));
            valores.push_back(std::to_string(val));
        } else if (col.type == FieldType::FLOAT) {
            float val = 0.0f;
            std::memcpy(&val, data.data() + offset, sizeof(float));
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", val);
            valores.push_back(buf);
        } else {
            const char* ptr = reinterpret_cast<const char*>(data.data() + offset);
            uint32_t len = 0;
            while (len < fs && ptr[len] != '\0') ++len;
            valores.push_back(std::string(ptr, len));
        }

        cursor += fs;
    }

    return valores;
}
