#include <iostream>
#include <cassert>
#include <stdexcept>
#include "disk_geometry.h"

// ── Utilidad ──────────────────────────────────────────────────
void ok(const std::string& msg) { std::cout << "[OK] " << msg << "\n"; }

// ─────────────────────────────────────────────────────────────────
void test_constructor() {
    // Geometría válida
    DiskGeometry g(2, 10, 20, 512);
    assert(g.get_platters()          == 2);
    assert(g.get_tracks()            == 10);
    assert(g.get_sectors_per_track() == 20);
    assert(g.get_bytes_per_sector()  == 512);
    ok("constructor válido");

    // Parámetros inválidos deben lanzar
    bool threw = false;
    try { DiskGeometry bad(0, 10, 20, 512); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);

    threw = false;
    try { DiskGeometry bad(2, 10, 20, 9); }   // B <= SECTOR_HEADER_SIZE
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);

    ok("constructor invalido lanza correctamente");
}

// ─────────────────────────────────────────────────────────────────
void test_capacidad() {
    // P=2, T=10, S=20, B=512
    // superficies = 4, sectores = 4*10*20 = 800
    DiskGeometry g(2, 10, 20, 512);

    assert(g.total_surfaces()  == 4);
    assert(g.total_sectors()   == 800);
    assert(g.useful_bytes()    == 512 - 9);       // 503
    assert(g.total_capacity()  == 800ULL * 512);  // 409600
    assert(g.usable_capacity() == 800ULL * 503);  // 402400

    ok("capacidades  → sectores=" + std::to_string(g.total_sectors()) +
       "  útiles/sector=" + std::to_string(g.useful_bytes()) + "B");
}

// ─────────────────────────────────────────────────────────────────
void test_chs_lba_ida_vuelta() {
    // P=2, T=10, S=20 → sectores por superficie = 200, total = 400
    DiskGeometry g(2, 10, 20, 512);

    // Primer sector: plato=0, cara=0, pista=0, sector=0 → LBA=0
    CHSAddress c0{0, 0, 0, 0};
    assert(g.chs_to_lba(c0) == 0);

    CHSAddress back0 = g.lba_to_chs(0);
    assert(back0.platter == 0 && back0.surface == 0 &&
           back0.track   == 0 && back0.sector  == 0);
    ok("LBA 0 → CHS " + back0.to_string());

    // Segundo sector de la primera pista → LBA=1
    CHSAddress c1{0, 0, 0, 1};
    assert(g.chs_to_lba(c1) == 1);

    // Primer sector de la cara inferior del plato 0
    // superficie=1 → lba = 1*200 = 200
    CHSAddress c200{0, 1, 0, 0};
    assert(g.chs_to_lba(c200) == 200);
    CHSAddress back200 = g.lba_to_chs(200);
    assert(back200.platter == 0 && back200.surface == 1 &&
           back200.track   == 0 && back200.sector  == 0);
    ok("CHS{plato=0,cara=1,pista=0,sec=0} ↔ LBA 200");

    // Primer sector del plato 1 cara 0
    // superficie=2 → lba = 2*200 = 400
    CHSAddress c400{1, 0, 0, 0};
    assert(g.chs_to_lba(c400) == 400);
    ok("CHS{plato=1,cara=0,pista=0,sec=0} ↔ LBA 400");

    // Último sector → LBA = 799
    CHSAddress last{1, 1, 9, 19};
    uint32_t lba_last = g.chs_to_lba(last);
    assert(lba_last == 799);
    CHSAddress back_last = g.lba_to_chs(799);
    assert(back_last.platter == 1 && back_last.surface == 1 &&
           back_last.track   == 9 && back_last.sector  == 19);
    ok("último sector CHS ↔ LBA 799");

    // Probar TODOS los LBA: lba_to_chs → chs_to_lba debe dar el mismo LBA
    for (uint32_t lba = 0; lba < g.total_sectors(); ++lba) {
        CHSAddress chs = g.lba_to_chs(lba);
        assert(g.chs_to_lba(chs) == lba);
    }
    ok("ida y vuelta para todos los " + std::to_string(g.total_sectors()) + " sectores");
}

// ─────────────────────────────────────────────────────────────────
void test_lba_invalido() {
    DiskGeometry g(2, 10, 20, 512);  // 800 sectores

    bool threw = false;
    try { g.lba_to_chs(800); }
    catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    threw = false;
    CHSAddress bad{2, 0, 0, 0};   // plato=2 no existe (solo 0 y 1)
    try { g.chs_to_lba(bad); }
    catch (const std::out_of_range&) { threw = true; }
    assert(threw);

    ok("LBA / CHS inválidos lanzan out_of_range");
}

// ─────────────────────────────────────────────────────────────────
void test_sectors_needed() {
    DiskGeometry g(2, 10, 20, 512);  // useful_bytes = 503

    // Registro que cabe en 1 sector
    assert(g.sectors_needed(1)   == 1);
    assert(g.sectors_needed(503) == 1);
    // Registro que necesita exactamente 2 sectores
    assert(g.sectors_needed(504) == 2);
    assert(g.sectors_needed(1006) == 2);
    // 3 sectores
    assert(g.sectors_needed(1007) == 3);

    ok("sectors_needed  → 503B=1sec | 504B=2sec | 1007B=3sec");
}

// ─────────────────────────────────────────────────────────────────
int main() {
    std::cout << "=== Tests disk_geometry ===\n";
    test_constructor();
    test_capacidad();
    test_chs_lba_ida_vuelta();
    test_lba_invalido();
    test_sectors_needed();
    std::cout << "=== Todo OK ===\n";
    return 0;
}
