# PROYECTO-FINAL-BDII
# 🖴 Disk Simulator — Motor Físico

Simulador de motor físico de disco que implementa la gestión completa de geometría, bitmap de sectores, escritura multi-sector y validación de esquemas con excepciones tipadas.

---

## 📋 Tabla de Contenidos

- [Arquitectura](#-arquitectura)
- [Características Implementadas](#-Características Implementadas)
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

## 🗂️ Funcionalidades del Simulador

- **Geometría de disco** — Modela platos, pistas y sectores reales. Convierte entre direcciones CHS y LBA. Calcula capacidad bruta y neta descontando headers de sector.

- **Bitmap de sectores libres** — Rastrea qué sectores están ocupados. Estrategia doble: primero busca bloques contiguos, luego cae a sectores dispersos.

- **Escritura con spanning multi-sector** — Registros que superan el tamaño útil de un sector se distribuyen en cadena. Cada sector guarda un header de 9 bytes (FLAG + RECORD_ID + NEXT_LBA) para reconstruir la cadena al leer.

- **Validación en capas** — Capa 1 (DDL): verifica que el esquema cabe en el disco antes de crear la tabla. Capa 2 (INSERT): verifica espacio libre antes de cada escritura. Capa 3 (CSV): calcula si el lote completo cabe antes de insertar nada.

- **Parser de esquema** — Lee archivos `.txt` con sintaxis `CREATE TABLE` (soporta `INT`, `FLOAT`, `VARCHAR(N)`, `TEXT`, `BLOB`, con o sin `PRIMARY KEY`).

- **Carga de CSV** — Lee un `.csv`, serializa cada fila según el esquema (tipos binarios para INT/FLOAT, padding fijo para strings) y la inserta en disco.

- **Índice AVL por columna** — Crea un árbol AVL por cada columna de la tabla. Soporta clave compuesta `(valor, LBA)` para manejar duplicados. Registra métricas de comparaciones y accesos a disco.

- **Motor de consultas** — Búsqueda exacta y por rango sobre cualquier columna usando el AVL. Devuelve el log del recorrido y los bytes reales leídos del disco.

- **Deserializador de registros** — Reconstruye los valores originales desde los bytes crudos del disco, respetando los gaps de sector boundary.

- **Panel B (UI consola)** — Muestra el recorrido AVL paso a paso, todos los campos de cada registro encontrado con su dirección CHS, y una comparativa de eficiencia AVL vs escaneo secuencial completo.



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


## Compilación (si se abre desde la carpeta "motor_fisico" en la terminal de Windows)
```bash
// ─── Test motor físico ───

g++ -std=c++17 main_demo.cpp column_schema.cpp csv_loader.cpp disk_engine.cpp disk_geometry.cpp disk_writer.cpp free_bitmap.cpp index_manager.cpp record_id.cpp record_serializer.cpp schema_parser.cpp sector_header.cpp validator.cpp query_engine.cpp ui_panel_b.cpp -o demo

// Para correr el ejecutable
.\demo schema_sin_pk.txt alumnos.csv

