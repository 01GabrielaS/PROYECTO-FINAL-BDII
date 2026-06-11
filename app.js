const express = require('express');
const path = require('path');
const { exec } = require('child_process');
const fs = require('fs');

const app = express();
const PORT = 3000;

app.use(express.json({ limit: '50mb' }));
app.use(express.static(path.join(__dirname, 'public')));

let DISK_GEOMETRY = null; 
let TABLE_SCHEMA = null;  
let TOTAL_REGISTROS_COUNT = 0; // Control de índices secuenciales

// Orquestador central para invocar el binario de C++ compilado
function ejecutarMotor(args) {
    return new Promise((resolve, reject) => {
        const comando = `./motor_db ${args}`;
        exec(comando, (error, stdout, stderr) => {
            if (error) {
                console.error(`[Error de Subproceso C++]: ${stderr}`);
                return reject(stderr || error.message);
            }
            resolve(stdout.trim());
        });
    });
}

// 1. Inicialización Geométrica Real
app.post('/api/init', async (req, res) => {
    const platos = parseInt(req.body.platos);
    const pistes = parseInt(req.body.pistas); 
    const pistas = pistes || parseInt(req.body.pistas);
    const sectores = parseInt(req.body.sectores);
    const bytes = parseInt(req.body.bytes);

    if (!platos || platos <= 0 || !pistas || pistas <= 0 || !sectores || sectores <= 0 || !bytes || bytes <= 0) {
        return res.status(400).json({ success: false, error: "Parámetros geométricos inválidos." });
    }

    try {
        const salida = await ejecutarMotor(`init ${platos} ${pistas} ${sectores} ${bytes}`);
        const partes = salida.split('|');

        if (partes[0] === 'SUCCESS') {
            const totalSectores = platos * 2 * pistas * sectores;
            const capacidadBytes = totalSectores * bytes;

            DISK_GEOMETRY = { platos, carasPorPlato: 2, pistas, sectores, bytes, totalSectores, capacidadBytes };
            TABLE_SCHEMA = null;
            TOTAL_REGISTROS_COUNT = 0;

            res.json({ 
                success: true, 
                capacidadMB: (capacidadBytes / (1024 * 1024)).toFixed(4), 
                sectores: totalSectores 
            });
        } else {
            res.status(400).json({ success: false, error: salida });
        }
    } catch (err) {
        res.status(500).json({ success: false, error: err.message || err });
    }
});

// 2. Definición de Esquema - SOLUCIÓN AL ERROR DE INTERPRETACIÓN DE ARGUMENTOS
app.post('/api/create-table', async (req, res) => {
    if (!DISK_GEOMETRY) return res.status(400).json({ success: false, error: "Disco no inicializado." });
    const { campos } = req.body;

    let registroSize = 0;
    const mapeoCampos = campos.map(c => {
        const [nombre, tipoRaw] = c.split(':');
        let size = 4; let tipo = 'INT';

        if (tipoRaw.includes('CHAR')) {
            const match = tipoRaw.match(/\d+/);
            size = match ? parseInt(match[0], 10) : 30;
            tipo = 'CHAR';
        } else if (tipoRaw.includes('FLOAT')) {
            size = 8; tipo = 'FLOAT';
        }
        registroSize += size;
        return { nombre: nombre.trim(), tipo, size };
    });

    const mapeoConPkSistémica = [
        { nombre: "ROW_ID_SISTEMA", tipo: "INT", size: 4 },
        ...mapeoCampos
    ];
    registroSize += 4; 

    if (registroSize > DISK_GEOMETRY.bytes) {
        return res.status(400).json({ success: false, error: "El registro excede el tamaño del sector físico." });
    }

    try {
        // ENFOQUE SEGURO: Envolvemos cada definición de columna de forma individual en comillas dobles.
        // Esto previene fallos de sintaxis por caracteres especiales y mantiene los argumentos separados para el ejecutable de C++.
        const argsDdl = campos.map(c => `"${c.trim()}"`).join(' ');
        const salidaTable = await ejecutarMotor(`create_table MiTabla ${argsDdl}`);

        if (salidaTable.startsWith('SUCCESS')) {
            const recordsPerSector = Math.floor(DISK_GEOMETRY.bytes / registroSize);
            TABLE_SCHEMA = { campos: mapeoConPkSistémica, registroSize, recordsPerSector, camposOriginales: mapeoCampos };

            res.json({ success: true, campos: mapeoConPkSistémica, registroSize, recordsPerSector });
        } else {
            res.status(400).json({ success: false, error: salidaTable });
        }
    } catch (err) {
        res.status(500).json({ success: false, error: err.message || err });
    }
});

// 3. Ingesta Masiva CSV delegando la inserción atómica a bajo nivel
app.post('/api/load-csv', async (req, res) => {
    if (!DISK_GEOMETRY || !TABLE_SCHEMA) return res.status(400).json({ success: false, error: "Estructuras base ausentes." });
    
    const { csvData } = req.body;
    try {
        const tempCsvPath = path.join(__dirname, 'input_subproceso.csv');
        fs.writeFileSync(tempCsvPath, csvData, 'utf-8');

        const salidaLoad = await ejecutarMotor(`load input_subproceso.csv`);
        if (fs.existsSync(tempCsvPath)) fs.unlinkSync(tempCsvPath);

        const partes = salidaLoad.split('|');
        if (partes[0] === 'SUCCESS') {
            const insertados = parseInt(partes[2].split(':')[1], 10);
            const sectoresOcupados = parseInt(partes[3].split(':')[1], 10);
            
            TOTAL_REGISTROS_COUNT += insertados;

            res.json({ success: true, overflow: false, insertados, sectores_ocupados: sectoresOcupados });
        } else {
            res.status(400).json({ success: false, error: salidaLoad });
        }
    } catch (err) {
        res.status(500).json({ success: false, error: err.message || err });
    }
});

// 4. Ingesta Manual Unitaria Integrada al Motor
app.post('/api/insert-manual', async (req, res) => {
    if (!TABLE_SCHEMA) return res.status(400).json({ success: false, error: "Esquema ausente." });
    const { registro } = req.body;

    try {
        const valoresPlanos = TABLE_SCHEMA.camposOriginales.map(col => `"${registro[col.nombre]}"`).join(' ');
        
        const salidaInsert = await ejecutarMotor(`insert_manual ${valoresPlanos}`);
        const partes = salidaInsert.split('|');

        if (partes[0] === 'SUCCESS') {
            const sectoresOcupados = parseInt(partes[2].split(':')[1], 10);
            TOTAL_REGISTROS_COUNT += 1;

            res.json({ success: true, sectores_ocupados, totalRegistros: TOTAL_REGISTROS_COUNT });
        } else {
            return res.status(400).json({ success: false, error: "DISK_OVERFLOW" });
        }
    } catch (err) {
        res.status(500).json({ success: false, error: err.message || err });
    }
});

// 5. Motor Analítico (Captura el costo real de accesos a bloques I/O sin simular datos)
app.post('/api/query', async (req, res) => {
    const { tipo, columna, valor, min, max } = req.body;
    if (TOTAL_REGISTROS_COUNT === 0) return res.status(400).json({ success: false, error: "No hay registros." });

    const infoCol = TABLE_SCHEMA.campos.find(c => c.nombre === columna);
    if (!infoCol) return res.status(400).json({ success: false, error: "Columna no válida." });

    const esPkSistemica = (columna === "ROW_ID_SISTEMA");

    try {
        let salidaCmd = "";
        if (tipo === 'exact') {
            salidaCmd = await ejecutarMotor(`search ${columna} "${valor}"`);
        } else {
            salidaCmd = await ejecutarMotor(`range ${columna} "${min}" "${max}"`);
        }

        const lineas = salidaCmd.split('\n');
        let ioIndex = 0;
        let ioSeq = 0;
        let formatoDatos = [];
        let dentroData = false;

        lineas.forEach(linea => {
            const l = linea.trim();
            if (l.startsWith("IO_AVL:")) ioIndex = parseInt(l.split(":")[1], 10);
            if (l.startsWith("IO_SECUENCIAL:")) ioSeq = parseInt(l.split(":")[1], 10);
            
            if (l === "DATA_START") { dentroData = true; return; }
            if (l === "DATA_END") { dentroData = false; return; }

            if (dentroData && l.includes("] -> ")) {
                const [coordenadas, datos] = l.split("] -> ");
                formatoDatos.push({
                    coordenadas: coordenadas.replace("[", ""),
                    registro: datos
                });
            }
        });

        res.json({
            success: true,
            isIndex: esPkSistemica,
            ioIndex: ioIndex, 
            ioSeq: ioSeq,     
            datos: formatoDatos
        });

    } catch (err) {
        res.status(500).json({ success: false, error: err.message || err });
    }
});

app.listen(PORT, () => console.log(`[PUENTE ACTIVO: CONTROLADORA INTEGRADA] Servidor respondiendo en puerto ${PORT}`));