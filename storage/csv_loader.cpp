#include "csv_loader.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <atomic>
#include <cstdlib>

static std::string trim_csv_field(const std::string& s) {
    size_t start = 0;
    size_t end = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

static bool is_integer_value(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtol(s.c_str(), &end, 10);
    return end && *end == '\0';
}

static bool is_float_value(const std::string& s) {
    if (s.empty()) return false;
    char* end = nullptr;
    std::strtof(s.c_str(), &end);
    return end && *end == '\0';
}

static bool looks_like_header_row(const std::vector<std::string>& row,
                                 const std::vector<ColumnDef>& schema) {
    if (row.size() != schema.size()) return true;
    int mismatches = 0;
    for (size_t i = 0; i < schema.size(); ++i) {
        const std::string value = trim_csv_field(row[i]);
        switch (schema[i].type) {
            case FieldType::INT:
                if (!is_integer_value(value)) mismatches++;
                break;
            case FieldType::FLOAT:
                if (!is_float_value(value)) mismatches++;
                break;
            default:
                break;
        }
    }
    return mismatches > 0;
}

// ── Parser de línea CSV con soporte de comillas ─────────────────
//   - Una coma DENTRO de comillas ("a, b") no separa campos.
//   - "" dentro de un campo entre comillas se interpreta como una
//     comilla literal (escape estándar tipo RFC 4180).
std::vector<std::string> CsvLoader::split_csv_line(const std::string& linea) {
    std::vector<std::string> campos;
    std::string actual;
    bool dentro_comillas = false;

    for (size_t i = 0; i < linea.size(); ++i) {
        char c = linea[i];

        if (dentro_comillas) {
            if (c == '"') {
                if (i + 1 < linea.size() && linea[i + 1] == '"') {
                    actual += '"';   // comilla escapada ""
                    ++i;
                } else {
                    dentro_comillas = false;
                }
            } else {
                actual += c;
            }
        } else {
            if (c == '"') {
                dentro_comillas = true;
            } else if (c == ',') {
                campos.push_back(actual);
                actual.clear();
            } else {
                actual += c;
            }
        }
    }
    campos.push_back(actual);
    return campos;
}

bool CsvLoader::read_csv_record(std::ifstream& file,
                                      std::vector<std::string>& row)
{
    row.clear();
    std::string line;
    std::string record;
    bool in_quotes = false;

    while (std::getline(file, line)) {
        if (!record.empty())
            record.push_back('\n');
        record += line;

        in_quotes = false;
        for (size_t i = 0; i < record.size(); ++i) {
            char c = record[i];
            if (c == '"') {
                if (i + 1 < record.size() && record[i + 1] == '"') {
                    ++i;
                } else {
                    in_quotes = !in_quotes;
                }
            }
        }

        if (!in_quotes) {
            row = split_csv_line(record);
            for (auto& campo : row)
                campo = trim_csv_field(campo);
            return true;
        }
    }

    return false;
}

bool CsvLoader::is_header_row(const std::vector<std::string>& row,
                                     const std::vector<ColumnDef>& schema)
{
    if (row.size() != schema.size())
        return false;

    bool name_match = true;
    int mismatches = 0;

    auto normalize = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
        return s;
    };

    for (size_t i = 0; i < schema.size(); ++i) {
        const std::string cell = normalize(trim_csv_field(row[i]));
        const std::string header = normalize(schema[i].name);
        if (cell != header)
            name_match = false;

        if (schema[i].type == FieldType::INT && !is_integer_value(cell))
            mismatches++;
        if (schema[i].type == FieldType::FLOAT && !is_float_value(cell))
            mismatches++;
    }

    return name_match || mismatches > 0;
}

// ── Generador de RecordID único ─────────────────────────────────
//   RecordID::encode(table_id, timestamp_low[12 bits], seq[6 bits])
//   Usar std::time() + (i % 64) puede repetirse si se cargan más de
//   64 filas en el mismo segundo. En su lugar usamos un contador
//   global atómico que combina ambos campos (18 bits = 262144
//   valores distintos por table_id antes de reiniciar), garantizando
//   unicidad real fila a fila sin depender del reloj del sistema.
static RecordID siguiente_record_id(uint16_t table_id) {
    static std::atomic<uint32_t> contador_global{0};
    uint32_t c = contador_global.fetch_add(1);

    uint32_t timestamp_low = (c >> 6) & 0xFFF;   // 12 bits altos del contador
    uint8_t  seq_num       = c & 0x3F;           // 6 bits bajos del contador

    return RecordID::encode(table_id, timestamp_low, seq_num);
}

CargaResultado CsvLoader::cargar(const std::string&            filepath,
                                 const std::vector<ColumnDef>& schema,
                                 DiskEngine&                   engine,
                                 IndexManager&                 indices,
                                 uint16_t                       table_id,
                                 bool                            permitir_carga_parcial)
{
    CargaResultado res;

    std::ifstream file(filepath);
    if (!file.is_open()) {
        res.mensaje = "No se pudo abrir el archivo: " + filepath;
        return res;
    }

    // ── Paso 1: leer y procesar filas en streaming ────────────────
    std::vector<std::string> fila;
    bool                    primera_fila = true;
    std::vector<std::vector<uint8_t>> storage;

    while (read_csv_record(file, fila)) {
        if (fila.empty()) continue;

        if (primera_fila) {
            primera_fila = false;
            if (is_header_row(fila, schema)) {
                continue;
            }
        }

        res.filas_leidas++;

        if (fila.size() != schema.size()) {
            res.filas_omitidas++;
            continue;
        }

        try {
            auto fields = RecordSerializer::serialize_row(fila, schema, storage);
            RecordID rid = siguiente_record_id(table_id);
            WriteResult wr = engine.insert_fields(rid, fields);
            indices.indexar_registro(schema, fila, rid, wr);
            res.filas_insertadas++;
        } catch (const InsertSpaceError&) {
            res.filas_omitidas++;
            res.carga_parcial = true;
            break;
        } catch (const std::exception& ex) {
            res.filas_omitidas++;
            continue;
        }
    }

    if (res.filas_leidas == 0) {
        res.mensaje = "El CSV no tiene filas de datos.";
        return res;
    }

    if (res.carga_parcial) {
        res.mensaje = "Carga parcial: " + std::to_string(res.filas_insertadas) +
                      " filas insertadas, " + std::to_string(res.filas_omitidas) +
                      " filas omitidas por falta de espacio o formato.";
    } else {
        res.mensaje = "Carga completa: " + std::to_string(res.filas_insertadas) + " filas insertadas.";
    }

    return res;
}
