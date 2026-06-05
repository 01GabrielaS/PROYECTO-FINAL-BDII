#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include "constants.h"

// ─────────────────────────────────────────────────────────────────
//  Dirección física CHS de un sector
// ─────────────────────────────────────────────────────────────────
struct CHSAddress {
    uint32_t platter;   // número de plato  (0 .. P-1)
    uint32_t surface;   // cara del plato   (0 = arriba, 1 = abajo)
    uint32_t track;     // pista (cilindro) (0 .. T-1)
    uint32_t sector;    // sector en pista  (0 .. S-1)

    std::string to_string() const;
};

// ─────────────────────────────────────────────────────────────────
//  DiskGeometry — toda la matemática de la geometría del disco
//
//  Parámetros de hardware:
//    P  = número de platos
//    T  = pistas por superficie
//    S  = sectores por pista
//    B  = bytes por sector (ej. 512)
//
//  Superficies totales  = P * 2          (cada plato tiene 2 caras)
//  Total de sectores    = P * 2 * T * S
//  Bytes útiles/sector  = B - SECTOR_HEADER_SIZE  (B - 9)
//
//  Fórmula LBA:
//    superficie = plato * 2 + cara
//    lba = superficie * (T * S) + pista * S + sector
// ─────────────────────────────────────────────────────────────────
class DiskGeometry {
public:
    // ── Constructor — lanza std::invalid_argument si hay parámetros fuera de rango
    DiskGeometry(uint32_t platters,
                 uint32_t tracks_per_surface,
                 uint32_t sectors_per_track,
                 uint32_t bytes_per_sector = DEFAULT_SECTOR_SIZE);

    // ── Conversiones ───────────────────────────────────────────
    uint32_t   chs_to_lba(const CHSAddress& chs) const;
    CHSAddress lba_to_chs(uint32_t lba)           const;

    // ── Cálculos de capacidad ──────────────────────────────────
    uint32_t total_sectors()    const;  // P * 2 * T * S
    uint32_t total_surfaces()   const;  // P * 2
    uint32_t useful_bytes()     const;  // bytes_per_sector - SECTOR_HEADER_SIZE
    uint64_t total_capacity()   const;  // total_sectors * bytes_per_sector (bruto)
    uint64_t usable_capacity()  const;  // total_sectors * useful_bytes (neto para datos)

    // ── Sectores necesarios para un registro de 'record_size' bytes
    uint32_t sectors_needed(uint32_t record_size) const;

    // ── Validación de LBA ──────────────────────────────────────
    bool     is_valid_lba(uint32_t lba) const;
    void     check_lba(uint32_t lba)    const;  // lanza si inválido

    // ── Getters (para que Diego e Iair puedan leer la config) ──
    uint32_t get_platters()           const { return P; }
    uint32_t get_tracks()             const { return T; }
    uint32_t get_sectors_per_track()  const { return S; }
    uint32_t get_bytes_per_sector()   const { return B; }

    // ── Debug ──────────────────────────────────────────────────
    std::string to_string() const;

private:
    uint32_t P;  // platos
    uint32_t T;  // pistas por superficie
    uint32_t S;  // sectores por pista
    uint32_t B;  // bytes por sector
};
