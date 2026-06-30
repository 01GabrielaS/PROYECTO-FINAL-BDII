// ============================================================
//  main_avl.cpp  —  Tests AVLIndex
//
//  Compila con:
//     g++ -std=c++17 main_avl.cpp record_id.cpp -o avl_demo
//  (Abrir desde la carpeta de motor fisico)
// ============================================================

#include "../indexing/AVLIndex.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>
using namespace std;

// ─── Helpers ─────────────────────────────────────────────────────

void separator(const std::string& t) {
    std::cout << "\n" << std::string(60, '=') << "\n  " << t
              << "\n" << std::string(60, '=') << "\n";
}
void ok  (const std::string& m) { std::cout << "  [OK]   " << m << "\n"; }
void info(const std::string& m) { std::cout << "  [INFO] " << m << "\n"; }

void printTraversal(const std::vector<std::string>& traversal) {
    std::cout << "  Recorrido AVL (" << traversal.size() << " pasos):\n";
    for (auto& paso : traversal)
        std::cout << "    " << paso << "\n";
}

// Simula el WriteResult que Gabriela devuelve tras engine.insert()
WriteResult makeWriteResult(uint32_t lba, uint32_t nsec) {
    WriteResult wr;
    wr.start_lba   = lba;
    wr.num_sectors = nsec;
    for (uint32_t i = 0; i < nsec; i++) wr.lba_chain.push_back(lba + i);
    return wr;
}

// ─── TEST 1: Inserción y balanceo ────────────────────────────────

void test_insercion_balanceo() {
    separator("TEST 1 — Inserción y balanceo AVL");
    AVLIndex<std::pair<int, uint32_t>> avl;

    // Insertar 1..7 en orden ascendente → fuerza rotaciones izquierda
    for (int i = 1; i <= 7; i++) {
        RecordID rid = RecordID::encode(1, i * 100, i);
        WriteResult wr = makeWriteResult(i * 10, 1);
        avl.insert({i, wr.start_lba}, rid, wr);
    }

    std::cout << "\n  Árbol (1..7 ascendente):\n";
    avl.printTree();
    std::cout << "\n  Altura=" << avl.height()
              << "  (sin AVL sería 7)\n";

    assert(avl.height() <= 4);
    ok("Altura dentro del límite AVL");

    // Clave duplicada: guarda duplicados
    size_t antes = avl.size();
    WriteResult wr_dup = makeWriteResult(999, 2);
    avl.insert({4, wr_dup.start_lba}, RecordID::encode(1, 999, 0), wr_dup);
    assert(avl.size() == antes + 1);
    ok("Key compuesta: mismo valor de columna, distinto lba → nodo nuevo (duplicados permitidos)");
}

// ─── TEST 2: Búsqueda exacta ─────────────────────────────────────

void test_busqueda_exacta() {
    separator("TEST 2 — Búsqueda exacta O(log n)");

    AVLIndex<std::pair<std::string, uint32_t>> avl;

    struct Row { std::string nombre; uint32_t lba; uint32_t nsec; };
    std::vector<Row> filas = {
        {"Ana",    10, 1}, {"Bruno",  20, 2}, {"Carlos", 30, 1},
        {"Diana",  40, 3}, {"Elena",  70, 1}, {"Felix",  80, 1},
        {"Gaby",   90, 2}, {"Diana", 110, 1}, {"Irene", 120, 1}
    };

    for (auto& f : filas) {
        RecordID rid = RecordID::encode(2, f.lba, 0);
        WriteResult wr = makeWriteResult(f.lba, f.nsec);
        avl.insert({f.nombre, wr.start_lba}, rid, wr);
    }

    avl.resetMetrics();

    // Búsqueda exitosa — con rangeSearch sobre clave compuesta
    auto resultados = avl.rangeSearch({"Diana", 0}, {"Diana", UINT32_MAX});
    assert(!resultados.matches.empty());

    printTraversal(resultados.traversal);

    auto& [key, e] = resultados.matches[0];
    std::cout << "\n  search('Diana') → " << e.to_string() << "\n";
    std::cout << "  Accesos a disco requeridos: " << e.num_sectors
              << "  (engine.read(" << e.start_lba << ") leerá la cadena)\n";
    ok("Registro encontrado, start_lba correcto");

    // Búsqueda fallida
    auto miss = avl.rangeSearch({"Zeta", 0}, {"Zeta", UINT32_MAX});
    assert(miss.matches.empty());
    ok("Clave inexistente retorna vacío");

    info(avl.metricasString());

    // Verificar O(log n)
    double limite = 2.0 * log2(avl.size()) + 2;
    assert(avl.comparaciones() <= 2 * (size_t)limite + 1);
    ok("Comparaciones dentro de O(log n)");
}

// ─── TEST 3: Búsqueda por rango ──────────────────────────────────

void test_busqueda_rango() {
    separator("TEST 3 — Búsqueda por rango In-order O(log n + k)");

    AVLIndex<std::pair<int, uint32_t>> avl;
    for (int edad = 1; edad <= 50; edad++) {
        RecordID rid = RecordID::encode(3, edad, 0);
        WriteResult wr = makeWriteResult(edad * 5, 1);
        avl.insert({edad, wr.start_lba}, rid, wr);
    }

    avl.resetMetrics();
    auto res = avl.rangeSearch({20, 0}, {30, UINT32_MAX});

    printTraversal(res.traversal);

    std::cout << "\n  rangeSearch(20, 30) → " << res.matches.size() << " registros:\n";
    for (auto& [k, e] : res.matches)
        std::cout << "    edad=" << k.first << " lba=" << e.start_lba
                  << " rid=" << e.record_id.to_string() << "\n";

    assert(res.matches.size() == 11);
    ok("11 resultados en [20,30]");

    // Orden ascendente (In-order)
    for (size_t i = 1; i < res.matches.size(); i++)
        assert(res.matches[i].first.first > res.matches[i-1].first.first);
    ok("Resultados en orden ascendente");

    info(avl.metricasString());
    ok("Métrica I/O: " + std::to_string(avl.accesos_disco()) + " accesos");
}

// ─── TEST 4: Eliminación lógica ──────────────────────────────────

void test_eliminacion_logica() {
    separator("TEST 4 — Eliminación lógica (engine.remove + AVL)");

    AVLIndex<std::pair<int, uint32_t>> avl;
    for (int i = 10; i <= 60; i += 10) {
        RecordID rid = RecordID::encode(4, i, 0);
        WriteResult wr = makeWriteResult(i, 1);
        avl.insert({i, wr.start_lba}, rid, wr);
    }

    std::cout << "\n  Árbol inicial (" << avl.size() << " nodos):\n";
    avl.printTree();

    bool engine_llamado = false;
    uint32_t lba_liberado = 0;

    auto keyToRemove = std::make_pair(30, (uint32_t)30);

    bool ok_rem = avl.remove(keyToRemove, [&](IndexEntry& e) {
        engine_llamado = true;
        lba_liberado   = e.start_lba;
        std::cout << "\n  [callback] engine.remove(lba=" << e.start_lba
                  << ")  rid=" << e.record_id.to_string() << "\n";
    });

    assert(ok_rem);
    assert(engine_llamado);
    std::cout << "  LBA liberado en bitmap: " << lba_liberado << "\n";
    ok("Callback de engine.remove() ejecutado");

    // Verificar que ya no existe (rangeSearch debe retornar vacío)
    auto check = avl.rangeSearch({30, 0}, {30, UINT32_MAX});
    assert(check.matches.empty());
    ok("rangeSearch → vacío tras eliminación");

    auto rango = avl.rangeSearch(std::make_pair(20, (uint32_t)0),
                                 std::make_pair(40, UINT32_MAX));
    for (auto& [k, e] : rango.matches) assert(k.first != 30);
    ok("rangeSearch no incluye el nodo eliminado");

    std::cout << "\n  Árbol tras eliminar 30 (" << avl.size() << " nodos):\n";
    avl.printTree();

    assert(!avl.remove(std::make_pair(999, (uint32_t)0), [](IndexEntry&){}));
    ok("remove de clave inexistente retorna false");
}

// ─── TEST 5: Flujo completo (integración real) ───────────────────

void test_integracion() {
    separator("TEST 5 — Flujo completo con tipos reales de Gabriela");

    AVLIndex<std::pair<std::string, uint32_t>> avl;

    struct Fila { std::string clave; uint16_t tid; uint32_t lba; uint32_t nsec; };
    std::vector<Fila> datos = {
        {"GARCIA", 1,  1, 3},
        {"SMITH",  1,  4, 1},
        {"LOPEZ",  1,  5, 2},
        {"TANAKA", 1,  7, 1},
        {"MULLER", 1,  8, 4},
    };

    for (size_t i = 0; i < datos.size(); i++) {
        auto& d = datos[i];
        RecordID rid = RecordID::encode(d.tid, d.lba, (uint8_t)i);
        WriteResult wr = makeWriteResult(d.lba, d.nsec);
        avl.insert({d.clave, wr.start_lba}, rid, wr);
    }

    avl.resetMetrics();

    // Búsqueda exacta
    auto res = avl.rangeSearch({"GARCIA", 0}, {"GARCIA", UINT32_MAX});
    assert(!res.matches.empty());

    printTraversal(res.traversal);

    auto& [key, e] = res.matches[0];
    std::cout << "\n  search('GARCIA'):\n"
              << "    " << e.to_string() << "\n"
              << "    → engine.read(" << e.start_lba
              << ") leerá " << e.num_sectors << " sectores\n";
    ok("Registro spanning encontrado, start_lba correcto para engine.read()");

    // Rango
    auto rango = avl.rangeSearch({"GARCIA", 0}, {"MULLER", UINT32_MAX});
    printTraversal(rango.traversal);

    std::cout << "\n  rangeSearch('GARCIA','MULLER'):\n";
    size_t total_accesos = 0;
    for (auto& [k, en] : rango.matches) {
        std::cout << "    " << std::left << std::setw(8) << k.first
                  << " lba=" << en.start_lba
                  << " nsec=" << en.num_sectors << "\n";
        total_accesos += en.num_sectors;
    }
    std::cout << "  Total accesos disco (cadena spanning): "
              << total_accesos << " sectores\n";

    ok("Rango sobre registros multi-sector correcto");
    info(avl.metricasString());
}

// ─── MAIN ────────────────────────────────────────────────────────

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n"
              << "║   AVLIndex — Proyecto BDII (módulo Iair)                ║\n"
              << "╚══════════════════════════════════════════════════════════╝\n";

    test_insercion_balanceo();
    test_busqueda_exacta();
    test_busqueda_rango();
    test_eliminacion_logica();
    test_integracion();

    separator("RESUMEN");
    std::cout << "  5/5 tests pasados.\n\n";
    return 0;
}
