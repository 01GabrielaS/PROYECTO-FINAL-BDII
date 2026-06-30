#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include "constants.h"

// ─────────────────────────────────────────────────────────────────
//  FreeBitmap — mapa de bits de sectores libres/ocupados
//
//  Cada posición i representa el sector con LBA = i:
//    0 → libre
//    1 → ocupado
//
//  Internamente se usa un vector<uint8_t> donde cada byte
//  almacena 8 sectores (bit 7 = sector más alto del byte).
//
//  Ejemplo con 16 sectores:
//    byte[0] → sectores 0-7
//    byte[1] → sectores 8-15
// ─────────────────────────────────────────────────────────────────
class FreeBitmap {
public:
    // ── Constructor: recibe el total de sectores del disco ─────
    explicit FreeBitmap(uint32_t total_sectors);

    // ── Consulta ───────────────────────────────────────────────
    bool     is_free(uint32_t lba)  const;
    bool     is_used(uint32_t lba)  const { return !is_free(lba); }
    uint32_t count_free()           const;
    uint32_t count_used()           const;
    uint32_t total_sectors()        const { return total_; }

    // ── Asignación ─────────────────────────────────────────────
    // Busca n sectores libres y los marca como OCUPADOS.
    // Devuelve un vector con los LBAs asignados.
    // Estrategia: primero intenta consecutivos; si no hay,
    // usa encadenamiento (no consecutivos).
    // Lanza std::runtime_error si no hay suficientes sectores.
    std::vector<uint32_t> alloc(uint32_t n);

    // Versión que solo busca sin marcar (para previsualizar)
    std::vector<uint32_t> find_free(uint32_t n) const;

    // ── Liberación ─────────────────────────────────────────────
    // Marca los LBAs de la lista como LIBRES
    void free_sectors(const std::vector<uint32_t>& lbas);
    void free_sector (uint32_t lba);

    // ── Marcado manual (usado por disk_writer al reconstruir) ──
    void mark_used(uint32_t lba);
    void mark_free(uint32_t lba);

    // ── Debug ──────────────────────────────────────────────────
    // Imprime el bitmap como una cadena de 0/1
    std::string to_string()     const;
    // Versión compacta: muestra bloques de sectores
    std::string to_summary()    const;

private:
    uint32_t              total_;   // total de sectores
    std::vector<uint8_t>  bits_;    // ceil(total/8) bytes

    // helpers de bit
    void set_bit  (uint32_t lba);
    void clear_bit(uint32_t lba);
    bool get_bit  (uint32_t lba) const;
    void check_lba(uint32_t lba) const;
};
