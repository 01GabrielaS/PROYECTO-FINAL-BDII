#include "../storage/column_schema.h"
#include "../engine/disk_engine.h"
#include "../indexing/index_manager.h"
#include "../storage/csv_loader.h"
#include "../engine/query_engine.h"
#include "ui_panel_b.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>

static std::string to_upper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return r;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <schema.txt> <datos.csv>\n";
        return 1;
    }

    std::string schema_file = argv[1];
    std::string csv_file    = argv[2];

    std::cout << "\n*** Demo Disco Virtual - Proyecto BDII ***\n\n";

    // 1. Parsear esquema
    auto schema = ColumnSchemaParser::parse(schema_file);
    if (schema.empty()) {
        std::cerr << "Error: no se pudo parsear el esquema de '" << schema_file << "'\n";
        return 1;
    }

    std::cout << "Esquema '" << schema_file << "' (" << schema.size() << " columnas):\n";
    uint32_t total_bytes = 0;
    for (auto& col : schema) {
        std::string tipo;
        switch (col.type) {
            case FieldType::INT:   tipo = "INT";          break;
            case FieldType::FLOAT: tipo = "FLOAT";        break;
            case FieldType::CHAR:  tipo = "VARCHAR/CHAR"; break;
            case FieldType::TEXT:  tipo = "TEXT";         break;
            case FieldType::BLOB:  tipo = "BLOB";         break;
        }
        std::cout << "  " << col.name << " - " << tipo << "(" << col.size << " bytes)\n";
        total_bytes += col.size;
    }
    std::cout << "  Total por registro: " << total_bytes << " bytes\n";

    // 2. Configurar disco
    DiskEngine engine;
    try {
        engine.configure(2, 20, 64, 120, "disco_demo.bin");
    } catch (const std::exception& e) {
        std::cerr << "Error configurando disco: " << e.what() << "\n";
        return 1;
    }
    std::cout << "\nDisco: " << engine.geometry_info() << "\n";
    std::cout << "Sectores totales: " << engine.total_sectors()
              << " | Libres: " << engine.free_sectors() << "\n";

    // 3. Validar esquema (Capa 1)
    try {
        engine.validate_schema(ColumnSchemaParser::field_sizes(schema));
        std::cout << "Esquema valido.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // 4. Crear indices
    IndexManager indices;
    indices.crear_indices(schema);

    // 5. Cargar CSV
    std::cout << "\nCargando '" << csv_file << "'...\n";
    auto carga = CsvLoader::cargar(csv_file, schema, engine, indices, 1);
    std::cout << carga.mensaje << "\n";
    std::cout << "Leidas: " << carga.filas_leidas
              << " | Insertadas: " << carga.filas_insertadas
              << " | Omitidas: " << carga.filas_omitidas << "\n";

    if (carga.filas_insertadas == 0) {
        std::cerr << "No se insertaron registros.\n";
        return 1;
    }

    std::cout << "\nColumnas indexadas: ";
    for (auto& col : indices.columnas()) std::cout << col << " ";
    std::cout << "\n";
    std::cout << "Uso disco: " << engine.used_sectors() << "/"
              << engine.total_sectors() << " sectores\n";

    // 6. Loop de busquedas
    QueryEngine qengine(engine);
    std::cout << "\n=== Motor de busquedas (exacto / rango / salir) ===\n";

    while (true) {
        std::cout << "\nTipo de busqueda: ";
        std::string tipo;
        if (!std::getline(std::cin, tipo)) break;
        tipo = to_upper(trim(tipo));

        if (tipo == "SALIR" || tipo == "S") { std::cout << "Saliendo...\n"; break; }

        if (tipo != "EXACTO" && tipo != "RANGO") {
            std::cout << "Usa: exacto, rango o salir.\n";
            continue;
        }

        std::cout << "Columna (";
        for (auto& c : indices.columnas()) std::cout << c << " ";
        std::cout << "): ";
        std::string columna;
        if (!std::getline(std::cin, columna)) break;
        columna = to_upper(trim(columna));

        auto* avl = indices.get_indice(columna);
        if (!avl) { std::cout << "Columna no encontrada.\n"; continue; }

        avl->resetMetrics();
        QueryResult result;

        if (tipo == "EXACTO") {
            std::cout << "Valor: ";
            std::string valor;
            if (!std::getline(std::cin, valor)) break;
            result = qengine.searchExact(*avl, ColumnKey(trim(valor)));
        } else {
            std::cout << "Desde: ";
            std::string lo;
            if (!std::getline(std::cin, lo)) break;
            std::cout << "Hasta: ";
            std::string hi;
            if (!std::getline(std::cin, hi)) break;
            result = qengine.searchRange(*avl, ColumnKey(trim(lo)), ColumnKey(trim(hi)));
        }

        UIPanelB::display(result, engine, avl->metricasString(), schema);
    }

    std::cout << "Sesion terminada.\n";
    return 0;
}
