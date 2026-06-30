#include <iostream>
#include <cassert>
#include <cstring>
#include <fstream>
#include "../config/constants.h"
#include "../hardware/disk_geometry.h"
#include "../hardware/free_bitmap.h"
#include "../hardware/disk_writer.h"

void ok(const std::string& msg) { std::cout << "[OK] " << msg << "\n"; }

// ═════════════════════════════════════════════════════════════════
//  TESTS FREE_BITMAP
// ═════════════════════════════════════════════════════════════════

void test_bitmap_basico() {
    FreeBitmap bm(16);

    // Todos libres al inicio
    assert(bm.count_free() == 16);
    assert(bm.count_used() == 0);
    assert(bm.is_free(0));
    assert(bm.is_free(15));

    // Marcar uno manualmente
    bm.mark_used(5);
    assert(bm.is_used(5));
    assert(bm.count_free() == 15);

    // Liberar
    bm.free_sector(5);
    assert(bm.is_free(5));
    assert(bm.count_free() == 16);

    ok("bitmap basico: marcado / liberado");
}

void test_bitmap_alloc_consecutivo() {
    FreeBitmap bm(20);

    // Pedir 4 sectores → debe dar 0,1,2,3 (consecutivos)
    auto lbas = bm.alloc(4);
    assert(lbas.size() == 4);
    assert(lbas[0] == 0 && lbas[3] == 3);
    assert(bm.count_free() == 16);
    assert(bm.is_used(0) && bm.is_used(3));

    ok("bitmap alloc consecutivo: LBAs " +
       std::to_string(lbas[0]) + "-" + std::to_string(lbas[3]));
}

void test_bitmap_alloc_disperso() {
    FreeBitmap bm(10);

    // Ocupar los sectores 0,1,3,4 manualmente para fragmentar
    bm.mark_used(0); bm.mark_used(1);
    bm.mark_used(3); bm.mark_used(4);
    // Libres: 2, 5, 6, 7, 8, 9

    // Pedir 5 → no hay 5 consecutivos, debe usar dispersos
    auto lbas = bm.alloc(5);
    assert(lbas.size() == 5);
    // Verificar que todos están marcados
    for (uint32_t lba : lbas) assert(bm.is_used(lba));

    ok("bitmap alloc disperso: " + std::to_string(lbas.size()) + " sectores");
}

void test_bitmap_sin_espacio() {
    FreeBitmap bm(4);
    bm.alloc(4);  // llenar todo

    bool threw = false;
    try { bm.alloc(1); }
    catch (const std::runtime_error& e) {
        threw = true;
        std::cout << "   Mensaje error: " << e.what() << "\n";
    }
    assert(threw);
    ok("bitmap lanza error cuando no hay espacio");
}

void test_bitmap_free_sectors() {
    FreeBitmap bm(8);
    auto lbas = bm.alloc(4);
    assert(bm.count_free() == 4);

    bm.free_sectors(lbas);
    assert(bm.count_free() == 8);
    ok("bitmap free_sectors: liberados " + std::to_string(lbas.size()));
}

// ═════════════════════════════════════════════════════════════════
//  TESTS DISK_WRITER
//  Usamos un disco pequeño: P=1, T=2, S=4, B=64
//  → 1*2*2*4 = 16 sectores, útiles = 64-9 = 55 bytes c/u
// ═════════════════════════════════════════════════════════════════

static const std::string TEST_DISK = "/tmp/test_disk.bin";

DiskGeometry make_geo() { return DiskGeometry(1, 2, 4, 64); }

void test_writer_registro_pequeno() {
    DiskGeometry geo = make_geo();     // 16 sectores, 55 útiles
    FreeBitmap   bm(geo.total_sectors());
    DiskWriter   dw(TEST_DISK, geo, bm, /*create_new=*/true);

    // Registro de 30 bytes → cabe en 1 sector
    RecordID rid = RecordID::encode(1, 1000, 1);
    uint8_t  data[30];
    for (int i = 0; i < 30; ++i) data[i] = static_cast<uint8_t>(i);

    WriteResult wr = dw.write_record(rid, data, 30);
    assert(wr.num_sectors == 1);
    assert(wr.start_lba   == 0);
    assert(bm.is_used(0));
    assert(bm.count_used() == 1);

    // Leer de vuelta
    ReadResult rr = dw.read_record(wr.start_lba);
    assert(rr.sectors_read == 1);
    // Los primeros 30 bytes deben coincidir
    assert(std::memcmp(rr.data.data(), data, 30) == 0);

    ok("writer registro pequeño (1 sector): escritura y lectura OK");
}

void test_writer_spanning_multisector() {
    DiskGeometry geo = make_geo();     // 55 bytes útiles
    FreeBitmap   bm(geo.total_sectors());
    DiskWriter   dw(TEST_DISK, geo, bm, /*create_new=*/true);

    // Registro de 120 bytes → necesita ceil(120/55) = 3 sectores
    assert(geo.sectors_needed(120) == 3);

    RecordID rid = RecordID::encode(2, 2000, 5);
    std::vector<uint8_t> data(120);
    for (int i = 0; i < 120; ++i) data[i] = static_cast<uint8_t>(i % 256);

    WriteResult wr = dw.write_record(rid, data.data(), 120);
    assert(wr.num_sectors  == 3);
    assert(wr.lba_chain.size() == 3);
    assert(bm.count_used() == 3);

    // Leer de vuelta
    ReadResult rr = dw.read_record(wr.start_lba);
    assert(rr.sectors_read == 3);
    // Los primeros 120 bytes deben coincidir
    assert(std::memcmp(rr.data.data(), data.data(), 120) == 0);

    ok("writer spanning 3 sectores: cadena LBA " +
       std::to_string(wr.lba_chain[0]) + "→" +
       std::to_string(wr.lba_chain[1]) + "→" +
       std::to_string(wr.lba_chain[2]));
}

void test_writer_delete() {
    DiskGeometry geo = make_geo();
    FreeBitmap   bm(geo.total_sectors());
    DiskWriter   dw(TEST_DISK, geo, bm, /*create_new=*/true);

    RecordID rid = RecordID::encode(1, 3000, 0);
    std::vector<uint8_t> data(110, 0xAB);  // 2 sectores

    WriteResult wr = dw.write_record(rid, data.data(), 110);
    assert(bm.count_used() == 2);

    // Eliminar
    dw.delete_record(wr.start_lba);
    assert(bm.count_used() == 0);
    assert(bm.count_free() == geo.total_sectors());

    ok("writer delete: bitmap liberado correctamente");
}

void test_writer_multiples_registros() {
    DiskGeometry geo = make_geo();   // 16 sectores
    FreeBitmap   bm(geo.total_sectors());
    DiskWriter   dw(TEST_DISK, geo, bm, /*create_new=*/true);

    // Insertar 3 registros
    uint8_t d1[20], d2[60], d3[30];
    std::memset(d1, 0x11, 20);
    std::memset(d2, 0x22, 60);
    std::memset(d3, 0x33, 30);

    WriteResult w1 = dw.write_record(RecordID::encode(1,100,1), d1, 20);  // 1 sec
    WriteResult w2 = dw.write_record(RecordID::encode(1,100,2), d2, 60);  // 2 sec
    WriteResult w3 = dw.write_record(RecordID::encode(1,100,3), d3, 30);  // 1 sec

    assert(bm.count_used() == 4);  // 1+2+1

    // Leer cada uno y verificar datos
    ReadResult r1 = dw.read_record(w1.start_lba);
    ReadResult r2 = dw.read_record(w2.start_lba);
    ReadResult r3 = dw.read_record(w3.start_lba);

    assert(std::memcmp(r1.data.data(), d1, 20) == 0);
    assert(std::memcmp(r2.data.data(), d2, 60) == 0);
    assert(std::memcmp(r3.data.data(), d3, 30) == 0);

    ok("writer múltiples registros: " +
       std::to_string(bm.count_used()) + " sectores usados de " +
       std::to_string(geo.total_sectors()));
}

// ═════════════════════════════════════════════════════════════════
int main() {
    std::cout << "=== Tests FreeBitmap ===\n";
    test_bitmap_basico();
    test_bitmap_alloc_consecutivo();
    test_bitmap_alloc_disperso();
    test_bitmap_sin_espacio();
    test_bitmap_free_sectors();

    std::cout << "\n=== Tests DiskWriter ===\n";
    test_writer_registro_pequeno();
    test_writer_spanning_multisector();
    test_writer_delete();
    test_writer_multiples_registros();

    std::cout << "\n=== Todo OK ===\n";
    return 0;
}
