#include "free_bitmap.h"
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────
FreeBitmap::FreeBitmap(uint32_t total_sectors)
    : total_(total_sectors)
    , bits_((total_sectors + 7) / 8, 0x00)  // todos los bits en 0 = libres
{
    if (total_sectors == 0)
        throw std::invalid_argument("FreeBitmap: total_sectors no puede ser 0");
}

// ─────────────────────────────────────────────────────────────────
//  Helpers privados de manipulación de bits
// ─────────────────────────────────────────────────────────────────
void FreeBitmap::check_lba(uint32_t lba) const {
    if (lba >= total_)
        throw std::out_of_range("Bitmap: LBA " + std::to_string(lba) +
                                " fuera de rango (total=" +
                                std::to_string(total_) + ")");
}

// Bit dentro del byte: bit 7 = sector más bajo del byte
//   lba=0 → byte[0] bit 7
//   lba=1 → byte[0] bit 6  ...  lba=7 → byte[0] bit 0
//   lba=8 → byte[1] bit 7
void FreeBitmap::set_bit(uint32_t lba) {
    bits_[lba / 8] |= (0x80 >> (lba % 8));
}

void FreeBitmap::clear_bit(uint32_t lba) {
    bits_[lba / 8] &= ~(0x80 >> (lba % 8));
}

bool FreeBitmap::get_bit(uint32_t lba) const {
    return (bits_[lba / 8] & (0x80 >> (lba % 8))) != 0;
}

// ─────────────────────────────────────────────────────────────────
//  Consulta
// ─────────────────────────────────────────────────────────────────
bool FreeBitmap::is_free(uint32_t lba) const {
    check_lba(lba);
    return !get_bit(lba);
}

uint32_t FreeBitmap::count_free() const {
    uint32_t count = 0;
    for (uint32_t i = 0; i < total_; ++i)
        if (!get_bit(i)) ++count;
    return count;
}

uint32_t FreeBitmap::count_used() const {
    return total_ - count_free();
}

// ─────────────────────────────────────────────────────────────────
//  find_free — busca n sectores sin marcarlos
//
//  Estrategia 1: busca n sectores CONSECUTIVOS (ideal para
//  rendimiento de lectura secuencial).
//  Estrategia 2: si no hay consecutivos, recoge n sectores
//  dispersos (encadenamiento en disk_writer).
// ─────────────────────────────────────────────────────────────────
std::vector<uint32_t> FreeBitmap::find_free(uint32_t n) const {
    if (n == 0) return {};

    // ── Intento 1: consecutivos ────────────────────────────────
    uint32_t run_start = 0, run_len = 0;
    for (uint32_t i = 0; i < total_; ++i) {
        if (!get_bit(i)) {
            if (run_len == 0) run_start = i;
            ++run_len;
            if (run_len == n) {
                // Encontramos bloque consecutivo
                std::vector<uint32_t> result(n);
                for (uint32_t k = 0; k < n; ++k)
                    result[k] = run_start + k;
                return result;
            }
        } else {
            run_len = 0;
        }
    }

    // ── Intento 2: dispersos ───────────────────────────────────
    std::vector<uint32_t> result;
    result.reserve(n);
    for (uint32_t i = 0; i < total_ && result.size() < n; ++i)
        if (!get_bit(i)) result.push_back(i);

    if (result.size() < n)
        throw std::runtime_error(
            "Espacio insuficiente en disco. "
            "Sectores requeridos: " + std::to_string(n) +
            " | Sectores libres: " + std::to_string(count_free()));

    return result;
}

// ─────────────────────────────────────────────────────────────────
//  alloc — busca n sectores y los marca como OCUPADOS
// ─────────────────────────────────────────────────────────────────
std::vector<uint32_t> FreeBitmap::alloc(uint32_t n) {
    std::vector<uint32_t> lbas = find_free(n);   // lanza si no hay
    for (uint32_t lba : lbas)
        set_bit(lba);
    return lbas;
}

// ─────────────────────────────────────────────────────────────────
//  Liberación
// ─────────────────────────────────────────────────────────────────
void FreeBitmap::free_sector(uint32_t lba) {
    check_lba(lba);
    clear_bit(lba);
}

void FreeBitmap::free_sectors(const std::vector<uint32_t>& lbas) {
    for (uint32_t lba : lbas)
        free_sector(lba);
}

// ─────────────────────────────────────────────────────────────────
//  Marcado manual
// ─────────────────────────────────────────────────────────────────
void FreeBitmap::mark_used(uint32_t lba) {
    check_lba(lba);
    set_bit(lba);
}

void FreeBitmap::mark_free(uint32_t lba) {
    check_lba(lba);
    clear_bit(lba);
}

// ─────────────────────────────────────────────────────────────────
//  to_string — cadena completa de 0/1 (útil para debugging)
// ─────────────────────────────────────────────────────────────────
std::string FreeBitmap::to_string() const {
    std::string s;
    s.reserve(total_);
    for (uint32_t i = 0; i < total_; ++i)
        s += get_bit(i) ? '1' : '0';
    return s;
}

// ─────────────────────────────────────────────────────────────────
//  to_summary — resumen compacto
// ─────────────────────────────────────────────────────────────────
std::string FreeBitmap::to_summary() const {
    std::ostringstream oss;
    oss << "Bitmap{total=" << total_
        << " libres="  << count_free()
        << " ocupados=" << count_used()
        << " uso=" << (count_used() * 100 / total_) << "%}";
    return oss.str();
}
