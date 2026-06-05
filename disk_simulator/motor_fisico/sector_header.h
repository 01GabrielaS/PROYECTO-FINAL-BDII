#pragma once
#include <cstdint>
#include <cstring>
#include "constants.h"
#include "record_id.h"

// ─────────────────────────────────────────────────────────────────
//  SectorHeader — encabezado de 9 bytes al inicio de cada sector
//
//  Layout en disco (sin padding gracias a #pragma pack):
//  ┌──────────┬───────────────────┬────────────────────────┐
//  │  FLAG    │    RECORD_ID      │    NEXT_SECTOR_LBA     │
//  │  1 byte  │     4 bytes       │        4 bytes         │
//  └──────────┴───────────────────┴────────────────────────┘
//  Total: 9 bytes  →  useful_bytes = sector_size - 9
// ─────────────────────────────────────────────────────────────────

#pragma pack(push, 1)
struct SectorHeader {
    uint8_t  flag;            // FLAG_START / FLAG_CONT / FLAG_END / FLAG_FREE
    uint32_t record_id;       // RecordID.value
    uint32_t next_sector_lba; // LBA del siguiente sector, o NEXT_LBA_NONE

    // ── Constructores ─────────────────────────────────────────
    SectorHeader();
    SectorHeader(uint8_t f, RecordID rid, uint32_t next_lba);

    // ── Serialización: escribe/lee los 9 bytes en un buffer ───
    //   buf debe tener al menos SECTOR_HEADER_SIZE bytes
    void     to_bytes  (uint8_t* buf) const;
    static SectorHeader from_bytes(const uint8_t* buf);

    // ── Helpers de consulta ───────────────────────────────────
    bool is_start() const { return flag == FLAG_START; }
    bool is_cont()  const { return flag == FLAG_CONT;  }
    bool is_end()   const { return flag == FLAG_END;   }
    bool is_free()  const { return flag == FLAG_FREE;  }
    bool is_last()  const { return next_sector_lba == NEXT_LBA_NONE; }
};
#pragma pack(pop)

// Garantía en tiempo de compilación: exactamente 9 bytes
static_assert(sizeof(SectorHeader) == SECTOR_HEADER_SIZE,
              "SectorHeader no mide exactamente 9 bytes — revisar alineación");
