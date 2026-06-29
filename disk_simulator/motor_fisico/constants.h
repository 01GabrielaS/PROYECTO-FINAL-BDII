#pragma once
#include <cstdint>

// ─────────────────────────────────────────────
//  FLAGS de sector  (1 byte, campo FLAG)
// ─────────────────────────────────────────────
constexpr uint8_t FLAG_START = 0x01;  // primer sector de un registro
constexpr uint8_t FLAG_CONT  = 0x02;  // sector de continuación
constexpr uint8_t FLAG_END   = 0x03;  // último sector de la cadena
constexpr uint8_t FLAG_FREE  = 0x00;  // sector vacío / no usado

// ─────────────────────────────────────────────
//  Sentinel: "no hay siguiente sector"
// ─────────────────────────────────────────────
constexpr uint32_t NEXT_LBA_NONE = 0xFFFFFFFF;

// ─────────────────────────────────────────────
//  Tamaño fijo del SectorHeader en bytes
//  [FLAG:1][RECORD_ID:4][NEXT_LBA:4] = 9
// ─────────────────────────────────────────────
constexpr uint32_t SECTOR_HEADER_SIZE = 9;

// ─────────────────────────────────────────────
//  Tamaños de tipos de datos (DDL)
// ─────────────────────────────────────────────
constexpr uint32_t SIZE_INT   = 4;
constexpr uint32_t SIZE_FLOAT = 4;
// CHAR(N) y TEXT(N) -> N bytes  (el parser los pasa directamente)
// BLOB(N)           -> N bytes

// ─────────────────────────────────────────────
//  Límites de hardware
// ─────────────────────────────────────────────
constexpr uint32_t MAX_PLATTERS  = 16;
constexpr uint32_t MAX_TRACKS    = 1024;
constexpr uint32_t MAX_SECTORS   = 256;
constexpr uint32_t DEFAULT_SECTOR_SIZE = 520;  // bytes
