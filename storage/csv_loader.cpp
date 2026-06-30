#include "csv_loader.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <atomic>

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

    // ── Paso 1: leer todas las filas a RAM (primera línea = encabezado) ──
    std::vector<std::vector<std::string>> filas;
    std::string linea;
    bool primera = true;
    while (std::getline(file, linea)) {
        if (linea.empty()) continue;
        if (primera) { primera = false; continue; }
        filas.push_back(split_csv_line(linea));
    }
    res.filas_leidas = static_cast<uint32_t>(filas.size());

    if (res.filas_leidas == 0) {
        res.mensaje = "El CSV no tiene filas de datos.";
        return res;
    }

    // ── Capa 3: validación global previa ──────────────────────────
    std::vector<uint32_t> field_sizes = ColumnSchemaParser::field_sizes(schema);

    uint32_t secs_por_fila   = engine.sectors_needed_for_fields(field_sizes);
    uint32_t secs_necesarios = res.filas_leidas * secs_por_fila;
    uint32_t secs_libres     = engine.free_sectors();

    uint32_t filas_a_cargar = res.filas_leidas;

    if (secs_necesarios > secs_libres) {
        if (!permitir_carga_parcial) {
            res.mensaje =
                "Espacio insuficiente: se necesitan " + std::to_string(secs_necesarios) +
                " sectores y hay " + std::to_string(secs_libres) + " libres. Carga cancelada.";
            return res;
        }
        filas_a_cargar    = secs_por_fila > 0 ? secs_libres / secs_por_fila : 0;
        res.carga_parcial = true;
        res.mensaje =
            "Espacio insuficiente para las " + std::to_string(res.filas_leidas) +
            " filas. Se cargarán solo " + std::to_string(filas_a_cargar) + " (carga parcial).";
    }

    // ── Paso 2: serializar + insertar en disco + indexar, fila por fila ──
    std::vector<std::vector<uint8_t>> storage;

    for (uint32_t i = 0; i < filas_a_cargar; ++i) {
        try {
            auto fields = RecordSerializer::serialize_row(filas[i], schema, storage);

            RecordID rid = siguiente_record_id(table_id);

            WriteResult wr = engine.insert_fields(rid, fields);
            indices.indexar_registro(schema, filas[i], rid, wr);

            res.filas_insertadas++;
        } catch (const InsertSpaceError&) {
            // Margen de seguridad por si el cálculo de Capa 3 falla en el borde
            res.filas_omitidas++;
            res.carga_parcial = true;
        }
    }

    if (res.mensaje.empty())
        res.mensaje = "Carga completa: " + std::to_string(res.filas_insertadas) + " filas insertadas.";

    return res;
}
