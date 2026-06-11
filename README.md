#  Simulador de Almacenamiento Físico de Base de Datos y Búsqueda

Este módulo contiene la implementación del motor de base de datos relacional a bajo nivel (C++) conectado a la interfaz gráfica web (Node.js).
---

## 🚀 Guía Rápida de Ejecución (Solo para Windows)

Abre la terminal de comandos (CMD o Git Bash) dentro de la carpeta del proyecto de tu grupo y ejecuta estos pasos en orden:

### 1. Descargar esta nueva Rama
```bash

git pull origin new-v
git checkout new-v

```

### 2. Instalar Librerías del Servidor Web

```bash
npm install

```

### 3. Compilar el Motor de C++ (Windows)

Copia y pega este comando tal cual en tu terminal para generar el ejecutable de Windows:

```bash
g++ -std=c++17 main.cpp motor_db.cpp MotorDB.cpp ArbolAVL.cpp DiscoVirtual.cpp -o motor_db.exe

```

### 4. Encender el Sistema

```bash
node app.js

```

Abre tu navegador e ingresa a: 👉 **`http://localhost:3000`**

---

## 🧠 Resumen Arquitectónico (Alineado al Informe de la Miss)

Para la sustentación y el informe escrito, la lógica del código se defiende bajo estos 4 pilares concretos:

1. **Configuración de Hardware (Personalizable):** Al ingresar los Platos, Pistas y Sectores en el Panel 1, el motor calcula la geometría física exacta. Reserva un archivo binario real (`disk.bin`) que actúa como la memoria secundaria simulada.
2. **Ingreso de Estructura y Datos (Persistencia Coherente):** Al cargar el CSV, el sistema lee el esquema relacional, inyecta un ID oculto del sistema (`ROW_ID_SISTEMA`) y valida que el tamaño del registro binario entre en los bytes del sector físico para evitar corrupción (`RECORD_SIZE_EXCEEDS_SECTOR_SIZE`).
3. **Mapeo de Direcciones Únicas (CHS):** El **Bit-Map** de sectores en RAM gestiona los bloques libres. Cada registro guardado recibe una coordenada geométrica única basada en hardware: **Plato, Cara (superficie correlativa global), Pista, Sector y su Offset** de bytes exacto.
4. **Ingreso de Consulta (Eficiencia AVL vs Escaneo Secuencial):** * **Por Elemento Primario (ID):** El **Árbol AVL** busca el ID de forma indexada en tiempo logarítmico, saltando directo a la coordenada del disco duro (Mínimo costo de lectura I/O).



```

---

