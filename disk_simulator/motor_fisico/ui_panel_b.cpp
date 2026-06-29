#include "ui_panel_b.h"
#include "record_serializer.h"
#include <iomanip>

static void sep(std::ostream& out, const std::string& title) {
    int pad = 55 - (int)title.size();
    out << "\n-- " << title << " ";
    for (int i = 0; i < pad && i < 50; ++i) out << '-';
    out << "\n";
}

static WriteResult wr_from_hit(const QueryResult::Hit& hit) {
    WriteResult wr;
    wr.start_lba   = hit.entry.start_lba;
    wr.num_sectors = hit.entry.num_sectors;
    wr.lba_chain   = hit.disk_data.lba_chain;
    return wr;
}

void UIPanelB::display(const QueryResult&           result,
                       DiskEngine&                  engine,
                       const std::string&           avl_metrics,
                       const std::vector<ColumnDef>& schema,
                       std::ostream&                out)
{
    out << "\n*** Panel B - Resultado de Consulta AVL ***\n";

    // 1. Recorrido AVL
    sep(out, "Recorrido AVL");
    if (result.traversal.empty()) {
        out << "  (sin pasos)\n";
    } else {
        for (const auto& paso : result.traversal)
            out << "  " << paso << "\n";
    }

    // 2. Registros encontrados
    sep(out, "Registros encontrados");
    if (result.hits.empty()) {
        out << "  No se encontraron registros.\n";
    } else {
        out << "  Total: " << result.hits.size() << " registro(s)\n";
        uint32_t useful_bytes = engine.geometry().useful_bytes();
        uint32_t record_bytes = 0;
        for (const auto& col : schema) record_bytes += col.size;
        int idx = 1;

        for (const auto& hit : result.hits) {
            out << "\n  [" << idx++ << "] " << hit.entry.record_id.to_string() << "\n";

            auto valores = RecordSerializer::deserialize_row(
                hit.disk_data.data, schema, useful_bytes);

            for (size_t i = 0; i < schema.size() && i < valores.size(); ++i)
                out << "      " << schema[i].name << " = " << valores[i] << "\n";

            WriteResult wr = wr_from_hit(hit);
            out << "      LBA=" << hit.entry.start_lba
                << " sectores=" << hit.entry.num_sectors << "\n";
            out << "      CHS: " << engine.format_chs_chain(wr, record_bytes) << "\n";
        }
    }

    // 3. Metricas comparativas
    sep(out, "Metricas: AVL vs Escaneo secuencial");
    out << "  " << avl_metrics << "\n";

    uint32_t total = engine.total_sectors();
    out << "  Escaneo completo (peor caso): " << total << " sectores\n";

    uint32_t accesos_avl = 0;
    for (const auto& hit : result.hits)
        accesos_avl += hit.entry.num_sectors;

    if (accesos_avl <= total) {
        double pct = total > 0 ? 100.0 * (total - accesos_avl) / total : 0.0;
        out << "  Accesos AVL: " << accesos_avl << " sectores\n";
        out << std::fixed << std::setprecision(2);
        out << "  Ahorro: " << (total - accesos_avl)
            << " sectores (" << pct << "% menos)\n";
    }
    out << "\n";
}
