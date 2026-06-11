# Simulador de Almacenamiento Físico de Base de Datos y Búsqueda

Este módulo contiene la implementación completa del motor de base de datos relacional a bajo nivel (C++) integrado a una interfaz gráfica web (Node.js). 

**Nota de Estado del Proyecto (Avance):** El sistema se encuentra al **100% de funcionalidad operativa** en su backend, algoritmos de indexación AVL, control de mapas de bits y persistencia física de registros de longitud fija. El porcentaje restante para la entrega final se limita exclusivamente a optimizaciones de diseño de interfaz, hojas de estilo (CSS) e interactividad estética.

---

## Guía Rápida de Ejecución

Abre la terminal de comandos (CMD/Git Bash en Windows, o la Terminal en Linux) dentro de la raíz del proyecto y ejecuta:

### 1. Preparar el Entorno Web
```bash
npm install

```

### 2. Compilar el Motor de C++ (Selecciona según tu Sistema Operativo)

* **En Windows:**
```bash
g++ -std=c++17 main.cpp motor_db.cpp MotorDB.cpp ArbolAVL.cpp DiscoVirtual.cpp -o motor_db.exe

```


* **En Linux:**
```bash
g++ -std=c++17 main.cpp motor_db.cpp MotorDB.cpp ArbolAVL.cpp DiscoVirtual.cpp -o motor_db

```



### 3. Inicializar el Servidor

```bash
node app.js

```

👉 Abre tu navegador web e ingresa a: **`http://localhost:3000`**

---

## Arquitectura de Persistencia e Indexación

La defensa técnica del motor de almacenamiento se fundamenta en los siguientes componentes estructurados:

1. **Simulación de Hardware Fijo:** El sistema inicializa dinámicamente un archivo binario nativo (`disk.bin`) que actúa como almacenamiento secundario persistente, calculando la capacidad total en función de la geometría ingresada (Platos, Caras, Pistas y Sectores).
2. **Inferencia de Esquema Dinámico:** Al procesar un archivo estructurado (.csv / .txt), el sistema define un esquema de registros de longitud fija (**Fixed-Length Records**) mapeando tipos estáticos (`INT`, `FLOAT`, `CHAR(N)`), inyectando la clave primaria de control `ROW_ID_SISTEMA` y validando estrictamente los límites físicos del sector para evitar desbordamientos.
3. **Mapeo Geométrico de Espacio (LBA y Bit-Map):** Se administra el espacio libre a través de un **Bit-Map** en RAM que rastrea la disponibilidad de sectores lógicos (**LBA**). El motor calcula el desplazamiento absoluto en bytes de cada fila (**Offset**) y su dirección geométrica exacta en hardware mediante la fórmula:

$$\text{Offset Inicio} = (\text{LBA} \times \text{Tamaño Sector}) + (\text{Slot} \times \text{Tamaño Registro})$$


4. **Módulo Analítico de Búsqueda (Índice Secundario AVL):** Para consultas analíticas (Búsqueda Exacta o por Rango), el motor instancia en RAM un **Árbol AVL** optimizado para duplicados. El nodo indexado almacena una lista densa de estructuras de direccionamiento físico (`RecordID`), permitiendo saltos binarios directos de alta velocidad al offset del archivo `disk.bin` y minimizando los costos operativos de operaciones I/O en lecturas de disco.

```

---