#include "disk_geometry.h"
#include <cmath>
#include <sstream>

// ─────────────────────────────────────────────────────────────────
//  CHSAddress::to_string
// ─────────────────────────────────────────────────────────────────
std::string CHSAddress::to_string() const {
    std::ostringstream oss;
    oss << "[Plato:" << platter
        << " Cara:"  << surface
        << " Pista:" << track
        << " Sector:"<< sector << "]";
    return oss.str();
}

// ─────────────────────────────────────────────────────────────────
//  Constructor — valida que los parámetros sean coherentes
// ─────────────────────────────────────────────────────────────────
DiskGeometry::DiskGeometry(uint32_t platters,
                           uint32_t tracks_per_surface,
                           uint32_t sectors_per_track,
                           uint32_t bytes_per_sector)
    : P(platters)
    , T(tracks_per_surface)
    , S(sectors_per_track)
    , B(bytes_per_sector)
{
    if (P == 0 || P > MAX_PLATTERS)
        throw std::invalid_argument("Número de platos fuera de rango (1-" +
                                    std::to_string(MAX_PLATTERS) + ")");
    if (T == 0 || T > MAX_TRACKS)
        throw std::invalid_argument("Pistas por superficie fuera de rango (1-" +
                                    std::to_string(MAX_TRACKS) + ")");
    if (S == 0 || S > MAX_SECTORS)
        throw std::invalid_argument("Sectores por pista fuera de rango (1-" +
                                    std::to_string(MAX_SECTORS) + ")");
    if (B <= SECTOR_HEADER_SIZE)
        throw std::invalid_argument("El sector debe tener más de " +
                                    std::to_string(SECTOR_HEADER_SIZE) +
                                    " bytes (tamaño del header)");
}

// ─────────────────────────────────────────────────────────────────
//  chs_to_lba
//
//  superficie = plato * 2 + cara
//  lba = superficie * (T * S) + pista * S + sector
// ─────────────────────────────────────────────────────────────────
uint32_t DiskGeometry::chs_to_lba(const CHSAddress& chs) const {
    if (chs.platter >= P || chs.surface > 1 ||
        chs.track   >= T || chs.sector  >= S)
    {
        throw std::out_of_range("CHS fuera de rango: " + chs.to_string());
    }

    uint32_t superficie = chs.platter * 2 + chs.surface;
    return superficie * (T * S) + chs.track * S + chs.sector;
}

// ─────────────────────────────────────────────────────────────────
//  lba_to_chs  (operación inversa)
// ─────────────────────────────────────────────────────────────────
CHSAddress DiskGeometry::lba_to_chs(uint32_t lba) const {
    check_lba(lba);

    CHSAddress chs;
    uint32_t sectors_per_surface = T * S;

    uint32_t superficie = lba / sectors_per_surface;
    uint32_t resto      = lba % sectors_per_surface;

    chs.platter = superficie / 2;
    chs.surface = superficie % 2;
    chs.track   = resto / S;
    chs.sector  = resto % S;

    return chs;
}

// ─────────────────────────────────────────────────────────────────
//  Cálculos de capacidad
// ─────────────────────────────────────────────────────────────────
uint32_t DiskGeometry::total_surfaces()  const { return P * 2; }
uint32_t DiskGeometry::total_sectors()   const { return P * 2 * T * S; }
uint32_t DiskGeometry::useful_bytes()    const { return B - SECTOR_HEADER_SIZE; }
uint64_t DiskGeometry::total_capacity()  const { return static_cast<uint64_t>(total_sectors()) * B; }
uint64_t DiskGeometry::usable_capacity() const { return static_cast<uint64_t>(total_sectors()) * useful_bytes(); }

// ─────────────────────────────────────────────────────────────────
//  sectors_needed  — ceil(record_size / useful_bytes)
//  Usado por el validator y por disk_writer
// ─────────────────────────────────────────────────────────────────
uint32_t DiskGeometry::sectors_needed(uint32_t record_size) const {
    uint32_t ub = useful_bytes();
    return (record_size + ub - 1) / ub;   // equivalente a ceil sin float
}

// ─────────────────────────────────────────────────────────────────
//  Validación de LBA
// ─────────────────────────────────────────────────────────────────
bool DiskGeometry::is_valid_lba(uint32_t lba) const {
    return lba < total_sectors();
}

void DiskGeometry::check_lba(uint32_t lba) const {
    if (!is_valid_lba(lba))
        throw std::out_of_range("LBA " + std::to_string(lba) +
                                " fuera de rango (max=" +
                                std::to_string(total_sectors() - 1) + ")");
}

// ─────────────────────────────────────────────────────────────────
//  to_string — resumen legible de la geometría
// ─────────────────────────────────────────────────────────────────
std::string DiskGeometry::to_string() const {
    std::ostringstream oss;
    oss << "DiskGeometry{"
        << "P=" << P
        << " T=" << T
        << " S=" << S
        << " B=" << B << "B"
        << " | sectores=" << total_sectors()
        << " útiles/sector=" << useful_bytes() << "B"
        << " capacidad_neta=" << usable_capacity() << "B"
        << "}";
    return oss.str();
}
