# PROYECTO-FINAL-BDII
# 🖴 Disk Simulator — Motor Físico

Simulador de motor físico de disco que implementa la gestión completa de geometría, bitmap de sectores, escritura multi-sector y validación de esquemas con excepciones tipadas.

---

## 📋 Tabla de Contenidos

- [Arquitectura](#-arquitectura)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Características Implementadas](#-características-implementadas)
- [API del Motor Físico](#-api-del-motor-físico)
- [Manejo de Errores](#-manejo-de-errores)
- [Compilación](#-compilación)
- [Equipo](#-equipo)

---

## 🏗️ Arquitectura

El motor físico se organiza en capas con responsabilidad única:

| Capa | Responsabilidad |
|------|-----------------|
| **Geometría** | Conversión CHS ↔ LBA, cálculos de capacidad |
| **Bitmap** | Seguimiento de sectores libres/ocupados |
| **Escritura** | Persistencia física con spanning multi-sector |
| **Validación** | Reglas DDL (capa 1) e INSERT (capa 2) |
| **Fachada** | `DiskEngine` como punto de entrada único |

---

## 📁 Estructura del Proyecto




---

## Características Implementadas

### Geometría del Disco
- Conversión completa **CHS ↔ LBA**
- Cálculos de capacidad total y neta del disco

### Bitmap de Sectores
- Estrategia doble de asignación:
  1. **Consecutivos** → búsqueda de bloques contiguos
  2. **Dispersos** → fallback a sectores fragmentados

### Escritura Física
- Spanning **multi-sector** (6 pasos del flujo definido en el PDF)
- Gestión de cabeceras de sector (`sector_header`)

### Validación
- **Capa 1 (DDL)**: Validación de esquemas antes de escritura
- **Capa 2 (INSERT)**: Verificación de espacio disponible
- **Excepciones tipadas** con mensajes exactos para la UI

###  Fachada Unificada
- `DiskEngine` expone una API limpia como punto de entrada único

---

##  API del Motor Físico

> Punto de entrada: `disk_engine.h` → clase `DiskEngine`

```cpp
// ─── Información del disco (Panel A de la UI) ───

engine.free_sectors()     // sectores libres actuales
engine.used_sectors()     // sectores ocupados
engine.total_sectors()    // total de sectores del disco
engine.usage_percent()    // porcentaje de uso (0.0 – 100.0)
engine.sectors_needed(n)  // cuántos sectores necesita un registro de n bytes
engine.geometry_info()    // string con resumen de la geometría
engine.bitmap_info()      // string con resumen del bitmap



// ─── Validación de esquema (Capa DDL) ───
try {
    engine.validate_schema(record_size);
} catch (const DDLCapacityError& e) {
    // e.what()          → mensaje completo para mostrar al usuario
    // e.record_size     → tamaño del esquema en bytes
    // e.total_capacity  → capacidad neta del disco en bytes
}

// ─── Inserción de registro (Capa INSERT) ───
try {
    engine.insert(record_id, data, size);
} catch (const InsertSpaceError& e) {
    // e.what()           → mensaje completo para mostrar al usuario
    // e.sectors_needed   → sectores que requería el registro
    // e.sectors_free     → sectores que había disponibles
}
```

## Compilación (test de Validator y disk_engine)
```bash
// ─── Test motor físico ───

g++ -std=c++17 -I motor_fisico \
  motor_fisico/record_id.cpp \
  motor_fisico/sector_header.cpp \
  motor_fisico/disk_geometry.cpp \
  motor_fisico/free_bitmap.cpp \
  motor_fisico/disk_writer.cpp \
  motor_fisico/validator.cpp \
  motor_fisico/disk_engine.cpp \
  motor_fisico/tests/test_validator_engine.cpp \
  -o test_validator_engine && ./test_validator_engine
