#include "sector_header.h"

// ─── Constructor por defecto: sector vacío ─────────────────────
SectorHeader::SectorHeader()
    : flag(FLAG_FREE)
    , record_id(0)
    , next_sector_lba(NEXT_LBA_NONE)
{}

// ─── Constructor con datos ─────────────────────────────────────
SectorHeader::SectorHeader(uint8_t f, RecordID rid, uint32_t next_lba)
    : flag(f)
    , record_id(rid.value)
    , next_sector_lba(next_lba)
{}

// ──────────────────────────────────────────────────────────────
//  to_bytes: serializa el header a los primeros 9 bytes del buffer
//
//  Se usa memcpy para copiar campo a campo y garantizar el orden
//  de bytes en disco independientemente del endianness del host.
//  (Para un proyecto académico en la misma máquina esto es seguro;
//   si necesitaran portabilidad se usaría htonl/ntohl aquí.)
// ──────────────────────────────────────────────────────────────
void SectorHeader::to_bytes(uint8_t* buf) const {
    uint32_t offset = 0;

    // FLAG (1 byte)
    buf[offset] = flag;
    offset += 1;

    // RECORD_ID (4 bytes)
    std::memcpy(buf + offset, &record_id, sizeof(record_id));
    offset += sizeof(record_id);

    // NEXT_SECTOR_LBA (4 bytes)
    std::memcpy(buf + offset, &next_sector_lba, sizeof(next_sector_lba));
}

// ──────────────────────────────────────────────────────────────
//  from_bytes: construye un SectorHeader leyendo 9 bytes del buf
// ──────────────────────────────────────────────────────────────
SectorHeader SectorHeader::from_bytes(const uint8_t* buf) {
    SectorHeader h;
    uint32_t offset = 0;

    // FLAG
    h.flag = buf[offset];
    offset += 1;

    // RECORD_ID
    std::memcpy(&h.record_id, buf + offset, sizeof(h.record_id));
    offset += sizeof(h.record_id);

    // NEXT_SECTOR_LBA
    std::memcpy(&h.next_sector_lba, buf + offset, sizeof(h.next_sector_lba));

    return h;
}
