#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include "../validator.h"
#include "../disk_engine.h"

void ok(const std::string& msg) { std::cout << "[OK] " << msg << "\n"; }

// ═════════════════════════════════════════════════════════════════
//  TESTS VALIDATOR
// ═════════════════════════════════════════════════════════════════

void test_validator_capa1_pasa() {
    // P=1,T=4,S=8,B=512 → sectores=64, útiles=503, cap_neta=32192B
    DiskGeometry geo(1, 4, 8, 512);

    // Registro de 100 bytes → cabe perfectamente
    Validator::validate_ddl(100, geo);   // no debe lanzar
    ok("capa 1: registro pequeño pasa sin error");
}

void test_validator_capa1_falla() {
    // Disco muy pequeño: P=1,T=1,S=2,B=64 → 4 sectores, útiles=55, cap=220B
    DiskGeometry geo(1, 1, 2, 64);

    bool threw = false;
    try {
        Validator::validate_ddl(500, geo);   // 500B > 220B → error
    } catch (const DDLCapacityError& e) {
        threw = true;
        std::cout << "   " << e.what() << "\n";
        assert(e.record_size    == 500);
        assert(e.total_capacity == 220);
    }
    assert(threw);
    ok("capa 1: registro grande lanza DDLCapacityError");
}

void test_validator_capa2_pasa() {
    DiskGeometry geo(1, 4, 8, 512);   // 64 sectores libres
    FreeBitmap   bm(geo.total_sectors());

    // Registro de 100B → 1 sector necesario, 64 libres → OK
    Validator::validate_insert(100, geo, bm);
    ok("capa 2: insert con espacio suficiente pasa");
}

void test_validator_capa2_falla() {
    DiskGeometry geo(1, 1, 2, 64);   // 4 sectores totales
    FreeBitmap   bm(geo.total_sectors());
    bm.alloc(4);   // llenar todo el disco

    bool threw = false;
    try {
        Validator::validate_insert(50, geo, bm);  // necesita 1 pero hay 0
    } catch (const InsertSpaceError& e) {
        threw = true;
        std::cout << "   " << e.what() << "\n";
        assert(e.sectors_needed == 1);
        assert(e.sectors_free   == 0);
    }
    assert(threw);
    ok("capa 2: disco lleno lanza InsertSpaceError");
}

void test_validator_compute_size() {
    // INT(4) + CHAR(50) + FLOAT(4) = 58
    std::vector<uint32_t> fields = {4, 50, 4};
    uint32_t sz = Validator::compute_record_size(fields);
    assert(sz == 58);
    ok("compute_record_size: INT+CHAR(50)+FLOAT = " + std::to_string(sz) + "B");
}

// ═════════════════════════════════════════════════════════════════
//  TESTS DISK_ENGINE  (flujo completo integrado)
// ═════════════════════════════════════════════════════════════════

void test_engine_sin_configurar() {
    DiskEngine engine;
    bool threw = false;
    try { engine.insert(RecordID::encode(1,1,1), nullptr, 0); }
    catch (const std::logic_error&) { threw = true; }
    assert(threw);
    ok("engine sin configurar lanza logic_error");
}

void test_engine_flujo_completo() {
    DiskEngine engine;
    // P=1, T=2, S=4, B=64 → 16 sectores, 55B útiles
    engine.configure(1, 2, 4, 64, "/tmp/engine_test.bin");

    assert(engine.is_configured());
    assert(engine.total_sectors() == 16);
    assert(engine.free_sectors()  == 16);
    assert(engine.usage_percent() == 0.0);

    std::cout << "   " << engine.geometry_info() << "\n";
    std::cout << "   " << engine.bitmap_info()   << "\n";
    ok("engine configurado correctamente");

    // ── Validación capa 1 ──────────────────────────────────────
    // cap_neta = 16 * 55 = 880B → esquema de 50B debe pasar
    engine.validate_schema({4, 4, 42});   // INT+FLOAT+CHAR(42) = 50B
    ok("engine validate_schema pasa con esquema de 50B");

    // Esquema que supera la capacidad debe fallar
    bool threw = false;
    try { engine.validate_schema(1000); }   // 1000B > 880B
    catch (const DDLCapacityError&) { threw = true; }
    assert(threw);
    ok("engine validate_schema lanza DDLCapacityError con esquema de 1000B");

    // ── INSERT registro pequeño ────────────────────────────────
    std::vector<uint8_t> data1(40, 0xAA);
    RecordID rid1 = RecordID::encode(1, 1000, 1);
    WriteResult w1 = engine.insert(rid1, data1.data(), 40);

    assert(w1.num_sectors == 1);
    assert(engine.used_sectors() == 1);
    ok("engine insert registro pequeño: LBA=" + std::to_string(w1.start_lba));

    // ── INSERT registro grande (spanning) ─────────────────────
    // 120B → ceil(120/55) = 3 sectores
    std::vector<uint8_t> data2(120);
    for (int i = 0; i < 120; ++i) data2[i] = static_cast<uint8_t>(i);
    RecordID rid2 = RecordID::encode(1, 1000, 2);
    WriteResult w2 = engine.insert(rid2, data2.data(), 120);

    assert(w2.num_sectors == 3);
    assert(engine.used_sectors() == 4);
    ok("engine insert spanning 3 sectores: LBA=" + std::to_string(w2.start_lba));

    // ── READ y verificar datos ─────────────────────────────────
    ReadResult r1 = engine.read(w1.start_lba);
    assert(r1.sectors_read == 1);
    assert(std::memcmp(r1.data.data(), data1.data(), 40) == 0);
    ok("engine read registro pequeño OK");

    ReadResult r2 = engine.read(w2.start_lba);
    assert(r2.sectors_read == 3);
    assert(std::memcmp(r2.data.data(), data2.data(), 120) == 0);
    ok("engine read registro grande OK, I/O=" + std::to_string(r2.sectors_read) + " accesos");

    // ── DELETE ────────────────────────────────────────────────
    engine.remove(w2.start_lba);
    assert(engine.used_sectors() == 1);   // solo queda w1
    ok("engine remove: bitmap liberado, usados=" + std::to_string(engine.used_sectors()));

    // ── Capa 2: disco casi lleno ───────────────────────────────
    // Llenamos hasta que quede 1 sector libre
    uint32_t libre = engine.free_sectors();
    std::vector<uint8_t> relleno(40, 0xFF);
    RecordID rid_r = RecordID::encode(1, 2000, 0);
    for (uint32_t i = 0; i < libre - 1; ++i) {
        rid_r = RecordID::encode(1, 2000, static_cast<uint8_t>(i));
        engine.insert(rid_r, relleno.data(), 40);
    }
    // Ahora queda 1 sector libre. Un registro de 60B necesita 2 → debe fallar
    bool threw2 = false;
    try {
        std::vector<uint8_t> grande(60, 0xBB);
        engine.insert(RecordID::encode(2,3000,0), grande.data(), 60);
    } catch (const InsertSpaceError& e) {
        threw2 = true;
        std::cout << "   " << e.what() << "\n";
    }
    assert(threw2);
    ok("engine capa 2: InsertSpaceError con disco casi lleno");
}

// ═════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Tests Validator ===\n";
    test_validator_capa1_pasa();
    test_validator_capa1_falla();
    test_validator_capa2_pasa();
    test_validator_capa2_falla();
    test_validator_compute_size();

    std::cout << "\n=== Tests DiskEngine (flujo integrado) ===\n";
    test_engine_sin_configurar();
    test_engine_flujo_completo();

    std::cout << "\n=== Todo OK ===\n";
    return 0;
}
