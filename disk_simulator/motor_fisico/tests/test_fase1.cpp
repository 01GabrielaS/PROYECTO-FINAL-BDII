#include <iostream>
#include <cassert>
#include <ctime>
#include "../constants.h"
#include "../record_id.h"
#include "../sector_header.h"

void test_constants() {
    assert(FLAG_FREE  == 0x00);
    assert(FLAG_START == 0x01);
    assert(FLAG_CONT  == 0x02);
    assert(FLAG_END   == 0x03);
    assert(NEXT_LBA_NONE == 0xFFFFFFFF);
    assert(SECTOR_HEADER_SIZE == 9);
    std::cout << "[OK] constants.h\n";
}

void test_record_id() {
    uint32_t ts = static_cast<uint32_t>(std::time(nullptr));

    RecordID rid = RecordID::encode(/*table=*/5, ts, /*seq=*/12);

    assert(rid.get_table_id() == 5);
    assert(rid.get_seq_num()  == 12);
    // timestamp_low son los 12 bits bajos
    assert(rid.get_timestamp() == (ts & 0xFFF));

    // Dos encodes distintos dan valores distintos
    RecordID r2 = RecordID::encode(1, ts, 0);
    RecordID r3 = RecordID::encode(2, ts, 0);
    assert(r2 < r3);

    std::cout << "[OK] record_id  → " << rid.to_string() << "\n";
}

void test_sector_header() {
    // sizeof garantizado por static_assert en el .h,
    // pero lo comprobamos igual
    assert(sizeof(SectorHeader) == 9);

    RecordID rid = RecordID::encode(3, 1000, 7);

    // Construir header de inicio
    SectorHeader h(FLAG_START, rid, 42);

    assert(h.is_start());
    assert(!h.is_last());
    assert(h.next_sector_lba == 42);

    // Serializar → deserializar
    uint8_t buf[9];
    h.to_bytes(buf);
    SectorHeader h2 = SectorHeader::from_bytes(buf);

    assert(h2.flag            == h.flag);
    assert(h2.record_id       == h.record_id);
    assert(h2.next_sector_lba == h.next_sector_lba);

    // Header de fin
    SectorHeader hfin(FLAG_END, rid, NEXT_LBA_NONE);
    assert(hfin.is_end());
    assert(hfin.is_last());

    // Header vacío
    SectorHeader empty;
    assert(empty.is_free());

    std::cout << "[OK] sector_header  → flag=" << (int)h.flag
              << "  record_id=0x" << std::hex << h.record_id
              << "  next_lba=" << std::dec << h.next_sector_lba << "\n";
}

int main() {
    std::cout << "=== Tests motor_fisico (fase 1) ===\n";
    test_constants();
    test_record_id();
    test_sector_header();
    std::cout << "=== Todo OK ===\n";
    return 0;
}
