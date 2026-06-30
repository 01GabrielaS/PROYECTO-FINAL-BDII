# PROYECTO-FINAL-BDII
# 🖴 Disk Simulator — Motor Físico

Simulador de motor físico de disco que implementa la gestión completa de geometría, bitmap de sectores, escritura multi-sector y validación de esquemas con excepciones tipadas.

---

## 📋 Tabla de Contenidos

- [Arquitectura](#-arquitectura)
- [Características Implementadas](#-CaracterísticasImplementadas)
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


## Compilación rápida en Windows

Abre PowerShell, Git Bash o WSL en la carpeta principal del proyecto:

```bash
cd disk_simulator/motor_fisico
```

Compila con este comando exacto:

```bash
g++ -std=c++17 apps/main_demo.cpp apps/ui_panel_b.cpp storage/column_schema.cpp storage/csv_loader.cpp engine/disk_engine.cpp hardware/disk_geometry.cpp hardware/disk_writer.cpp hardware/free_bitmap.cpp indexing/index_manager.cpp storage/record_id.cpp storage/record_serializer.cpp parsing/schema_parser.cpp hardware/sector_header.cpp parsing/validator.cpp engine/query_engine.cpp -o apps/demo.exe
```

> Si usas WSL, puedes cambiar `-o apps/demo.exe` por `-o apps/demo`.

### Ejecución

Ejecuta el demo con el esquema y el CSV de ejemplo:

En PowerShell:

```powershell
.\apps\demo.exe data\estructura.txt data\alumnos.csv
```

En Git Bash o WSL:

```bash
./apps/demo.exe data/estructura.txt data/alumnos.csv
```

> Si compilaste con `apps/demo`, ejecuta `./apps/demo data/estructura.txt data/alumnos.csv`.

---

## Archivos importantes

- `apps/main_demo.cpp` — programa principal del demo.
- `data/estructura.txt` — ejemplo de esquema.
- `data/alumnos.csv` — ejemplo de datos.
- `gui_web/` — frontend web en Flask.
- `storage/`, `hardware/`, `engine/`, `indexing/`, `parsing/` — capas del motor.

---

## Notas rápidas

- Asegúrate de tener `g++` instalado y accesible desde tu terminal.
- Si aparece un error de compilación, revisa que estés en la carpeta `motor_fisico`.
- `git status` te ayuda a ver los archivos modificados antes de subir.

