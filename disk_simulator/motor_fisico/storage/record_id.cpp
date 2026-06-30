#include "record_id.h"
#include <ctime>
#include <sstream>

// ─── Máscaras de bits ──────────────────────────────────────────
//  31-20 → table_id    (12 bits)
//  19-8  → timestamp   (12 bits)
//  7-2   → seq_num     (6 bits)
//  1-0   → reservado
static constexpr uint32_t MASK_TABLE = 0xFFF00000;  // 12 bits altos
static constexpr uint32_t MASK_TIME  = 0x000FFF00;  // 12 bits medios
static constexpr uint32_t MASK_SEQ   = 0x000000FC;  // 6 bits bajos (sin los 2 rsv)

// ──────────────────────────────────────────────────────────────
RecordID RecordID::encode(uint16_t table_id,
                          uint32_t timestamp,
                          uint8_t  seq_num)
{
    // Recortar cada campo a su ancho de bits
    uint32_t t  = static_cast<uint32_t>(table_id & 0xFFF);  // 12 bits
    uint32_t ts = static_cast<uint32_t>(timestamp & 0xFFF); // 12 bits bajos
    uint32_t s  = static_cast<uint32_t>(seq_num  & 0x3F);   // 6 bits

    uint32_t raw = (t  << 20)   // bits 31-20
                 | (ts <<  8)   // bits 19-8
                 | (s  <<  2);  // bits 7-2

    return RecordID(raw);
}

// ──────────────────────────────────────────────────────────────
uint16_t RecordID::get_table_id() const {
    return static_cast<uint16_t>((value & MASK_TABLE) >> 20);
}

uint16_t RecordID::get_timestamp() const {
    return static_cast<uint16_t>((value & MASK_TIME) >> 8);
}

uint8_t RecordID::get_seq_num() const {
    return static_cast<uint8_t>((value & MASK_SEQ) >> 2);
}

// ──────────────────────────────────────────────────────────────
std::string RecordID::to_string() const {
    std::ostringstream oss;
    oss << "RecordID{"
        << "table=" << get_table_id()
        << " ts="   << get_timestamp()
        << " seq="  << static_cast<int>(get_seq_num())
        << " raw=0x" << std::hex << value
        << "}";
    return oss.str();
}
