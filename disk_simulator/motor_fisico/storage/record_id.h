#pragma once
#include <cstdint>
#include <string>

// ─────────────────────────────────────────────────────────────────
//  RECORD_ID — identificador lógico de 32 bits
//
//  Layout de bits:
//  ┌──────────────┬─────────────────┬────────────┬───────┐
//  │  table_id    │  timestamp_low  │  seq_num   │  rsv  │
//  │  bits 31-20  │   bits 19-8     │  bits 7-2  │  1-0  │
//  │   12 bits    │    12 bits      │   6 bits   │ 2 bits│
//  └──────────────┴─────────────────┴────────────┴───────┘
//
//  - table_id      : identifica la tabla (hasta 4096 tablas)
//  - timestamp_low : 12 bits bajos del Unix timestamp
//  - seq_num       : hasta 64 registros distintos por tick/tabla
//  - rsv           : reservado para extensiones futuras
// ─────────────────────────────────────────────────────────────────
struct RecordID {
    uint32_t value;

    // Constructor por defecto
    RecordID() : value(0) {}

    // Constructor directo (cuando ya tienes el uint32)
    explicit RecordID(uint32_t raw) : value(raw) {}

    // ── Construcción desde partes ──────────────────────────────
    static RecordID encode(uint16_t table_id,
                           uint32_t timestamp,
                           uint8_t  seq_num);

    // ── Extracción de campos ───────────────────────────────────
    uint16_t get_table_id()  const;
    uint16_t get_timestamp() const;   // 12 bits bajos del ts
    uint8_t  get_seq_num()   const;

    // ── Comparación (útil para el AVL de Iair) ─────────────────
    bool operator==(const RecordID& o) const { return value == o.value; }
    bool operator< (const RecordID& o) const { return value <  o.value; }

    // ── Debug ──────────────────────────────────────────────────
    std::string to_string() const;
};
