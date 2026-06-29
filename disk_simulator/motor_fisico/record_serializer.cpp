#include "record_serializer.h"
#include <cstring>
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

// CHAR(N)/TEXT(N)/BLOB(N): copia hasta 'size' bytes y rellena el
// resto con 0x00 (padding). Si v.size() > size, se trunca.
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
            case FieldType::BLOB:  serialize_text(valores[i], col.size, storage[i]); break;
        }
    }

    std::vector<std::pair<const uint8_t*, uint32_t>> fields;
    fields.reserve(schema.size());
    for (size_t i = 0; i < schema.size(); ++i)
        fields.push_back({storage[i].data(), static_cast<uint32_t>(storage[i].size())});

    return fields;
}
