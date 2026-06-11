document.addEventListener('DOMContentLoaded', () => {

    // Estado local del Frontend (Metadata de Control Físico)
    let totalSectoresGlobal = 0;
    let bytesPorSectorGlobal = 0;
    let esquemaColumnasGlobal = []; 

    // Formateador Matemático para números colosales (Evita deformar la interfaz)
    function formatearEspacioAvanzado(bytes) {
        if (bytes === 0) return "0 Bytes";
        if (bytes > 1e12) {
            return `${(bytes).toExponential(2)} Bytes`;
        }
        const unidades = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(1024));
        return `${parseFloat((bytes / Math.pow(1024, i)).toFixed(2))} ${unidades[i]}`;
    }

    // CONTROL: Gestión dinámica de inputs según tipo de búsqueda
    document.getElementById('select-query-type').addEventListener('change', (e) => {
        const isExact = (e.target.value === 'exact');
        document.getElementById('box-val').style.display = isExact ? 'flex' : 'none';
        document.getElementById('box-min').style.display = isExact ? 'none' : 'flex';
        document.getElementById('box-max').style.display = isExact ? 'none' : 'flex';
    });

    // CONTROL: Bloqueo de rangos si la columna objetivo es un String (CHAR)
    document.getElementById('select-query-col').addEventListener('change', (e) => {
        const colSeleccionada = e.target.value;
        const metadataCol = esquemaColumnasGlobal.find(c => c.nombre === colSeleccionada);
        const selectTipo = document.getElementById('select-query-type');

        if (metadataCol && metadataCol.tipo === 'CHAR') {
            selectTipo.value = 'exact';
            selectTipo.dispatchEvent(new Event('change'));
            selectTipo.options[1].disabled = true; 
        } else {
            selectTipo.options[1].disabled = false;
        }
    });

    // PANEL 1: INICIALIZACIÓN INSTANTÁNEA (Mecanismo a Escala Estilo Blanco y Negro Elegante)
    document.getElementById('btn-init-disk').addEventListener('click', async () => {
        const platos = parseInt(document.getElementById('in-platos').value);
        const pistas = parseInt(document.getElementById('in-pistas').value);
        const sectores = parseInt(document.getElementById('in-sectores').value);
        const bytes = parseInt(document.getElementById('in-bytes').value);

        if (!platos || !pistas || !sectores || !bytes) {
            alert("Control de Calidad: Todos los campos geométricos deben ser enteros mayores a 0.");
            return;
        }

        try {
            const res = await fetch('/api/init', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ platos, pistas, sectores, bytes })
            });
            const data = await res.json();

            if (data.success) {
                totalSectoresGlobal = data.sectores;
                bytesPorSectorGlobal = bytes;

                // Renderizado Monocromático de la Barra (Gris intermedio limpio)
                const contenedorGrafica = document.getElementById('visual-platters');
                contenedorGrafica.innerHTML = `
                    <div style="margin-top: 15px; background: #ffffff; padding: 20px; border-radius: 0px; border: 1px solid #111111;">
                        <div style="display: flex; justify-content: space-between; font-weight: bold; margin-bottom: 8px; color: #000000;">
                            <span>Disco Local Simulado (C:)</span>
                            <span id="txt-proporcion-bytes">0 Bytes usados</span>
                        </div>
                        <div style="width: 100%; background: #ffffff; height: 24px; border: 1px solid #111111; overflow: hidden; position: relative;">
                            <div id="barra-progreso-disco" style="width: 0%; background: #d1d5db; height: 100%; transition: width 0.3s ease;"></div>
                        </div>
                        <div style="display: flex; gap: 20px; margin-top: 12px; font-size: 13px; color: #333333;">
                            <div><span style="display:inline-block; width:12px; height:12px; background:#d1d5db; border:1px solid #111111; vertical-align:middle;"></span> Espacio Ocupado</div>
                            <div><span style="display:inline-block; width:12px; height:12px; background:#ffffff; border:1px solid #111111; vertical-align:middle;"></span> Espacio Libre</div>
                        </div>
                    </div>
                `;

                const capFormateada = data.capacidadMB > 1024 * 1024 
                    ? formatearEspacioAvanzado(data.capacidadMB * 1024 * 1024) 
                    : `${data.capacidadMB} MB`;

                document.getElementById('metric-capacity').innerHTML = `Capacidad Total Inicializada: <strong>${capFormateada}</strong>`;
                
                actualizarMapaFisicoOptimizada(0);
            } else {
                alert("Error de Inicialización: " + data.error);
            }
        } catch (err) {
            console.error("Falla en la comunicación con el motor de base de datos:", err);
        }
    });

    // PANEL 2: PROCESAMIENTO DE ARCHIVOS CSV (Sin valores ni placeholders de sugerencia)
    document.getElementById('file-picker').addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (!file) return;

        const logCsv = document.getElementById('log-csv');
        logCsv.innerText = "Analizando estructura del archivo...";

        const reader = new FileReader();
        reader.onload = async function(evt) {
            const lines = evt.target.result.split('\n').map(l => l.trim()).filter(l => l.length > 0);
            if (lines.length < 2) { logCsv.innerText = "Error: El archivo no contiene registros suficientes."; return; }

            const headers = lines[0].split(',').map(h => h.trim());
            const sample = lines[1].split(',').map(s => s.trim());

            const camposDdl = headers.map((h, i) => {
                const val = sample[i] || "";
                if (val !== "" && !isNaN(val)) return val.includes('.') ? `${h}:FLOAT` : `${h}:INT`;
                return `${h}:CHAR(30)`;
            });

            try {
                const resSchema = await fetch('/api/create-table', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ campos: camposDdl })
                });
                const dataSchema = await resSchema.json();

                if (!dataSchema.success) { logCsv.innerText = "Falla estructural: " + dataSchema.error; return; }

                esquemaColumnasGlobal = dataSchema.campos;

                const selectCol = document.getElementById('select-query-col');
                selectCol.innerHTML = "";
                esquemaColumnasGlobal.forEach((col, idx) => {
                    const distintivoPk = idx === 0 ? " [ÍNDICE EN ÁRBOL AVL]" : "";
                    selectCol.innerHTML += `<option value="${col.nombre}">${col.nombre}${distintivoPk}</option>`;
                });
                selectCol.dispatchEvent(new Event('change'));

                // Inserción manual limpia de fondos grises y naranjas obsoletos
                const containerManual = document.getElementById('container-manual-fields');
                containerManual.innerHTML = "";
                esquemaColumnasGlobal.forEach((col, idx) => {
                    if (idx === 0) return; 
                    containerManual.innerHTML += `
                        <div class="field" style="margin-bottom: 10px;">
                            <label style="display:block; font-size:13px; color:#000000; margin-bottom:4px; font-weight:bold;">${col.nombre} (${col.tipo}):</label>
                            <input type="text" class="manual-input-field" data-col="${col.nombre}" data-tipo="${col.tipo}" style="width:100%; padding:8px; background:#ffffff; border:1px solid #111111; color:#000000; font-family:inherit;">
                        </div>`;
                });
                document.getElementById('btn-insert-manual').disabled = false;

                const resLoad = await fetch('/api/load-csv', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ csvData: evt.target.result })
                });
                const dataLoad = await resLoad.json();

                logCsv.innerText = `Carga Exitosa: ${dataLoad.insertados} registros procesados.`;
                if(dataLoad.overflow) logCsv.innerText += " [LÍMITE FÍSICO DEL DISCO ALCANZADO]";

                actualizarMetricasGlobales(dataLoad.sectores_ocupados);

            } catch (err) {
                logCsv.innerText = "Excepción crítica de procesamiento: " + err.message;
            }
        };
        reader.readAsText(file);
    });

    // PANEL 2: INSERCIÓN INDIVIDUAL CON VALIDACIÓN DE TIPOS ESTRICTA
    document.getElementById('btn-insert-manual').addEventListener('click', async () => {
        const inputs = document.querySelectorAll('.manual-input-field');
        const registro = {};
        let controlSanidad = true;

        inputs.forEach(inp => {
            const col = inp.dataset.col;
            const tipo = inp.dataset.tipo;
            const val = inp.value.trim();

            if (val === "") {
                alert(`Validación: El campo '${col}' no puede enviarse vacío.`);
                controlSanidad = false;
                return;
            }
            if ((tipo === 'INT' || tipo === 'FLOAT') && isNaN(val)) {
                alert(`Validación de Tipo: El campo '${col}' requiere un valor estrictamente numérico.`);
                controlSanidad = false;
                return;
            }
            registro[col] = val;
        });

        if (!controlSanidad) return;

        try {
            const res = await fetch('/api/insert-manual', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ registro })
            });
            const data = await res.json();

            if (data.success) {
                document.getElementById('log-manual').innerText = `Registro insertado con éxito.`;
                actualizarMetricasGlobales(data.sectores_ocupados);
                inputs.forEach(inp => inp.value = ""); 
            } else {
                alert("Denegado por el motor: " + data.error);
            }
        } catch (err) {
            alert("Error en el envío manual: " + err.message);
        }
    });

    // PANEL 4: MOTOR DE BÚSQUEDAS REPARADO Y RE-DIRECCIONADO AL INDICADOR DE ALTURA AVL
    document.getElementById('btn-run-query').addEventListener('click', async () => {
        const tipo = document.getElementById('select-query-type').value;
        const columna = document.getElementById('select-query-col').value;
        const valor = document.getElementById('query-val').value;
        const min = document.getElementById('query-min').value;
        const max = document.getElementById('query-max').value;

        if (!columna) { alert("Operación Inválida: Primero debe estructurar los datos cargando un archivo."); return; }

        try {
            const res = await fetch('/api/query', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ tipo, columna, valor, min, max })
            });
            const data = await res.json();
            const tbody = document.querySelector('#table-results tbody');
            const avlLogContainer = document.getElementById('avl-height-log');

            if (data.success) {
        

                tbody.innerHTML = "";
                if (!data.datos || data.datos.length === 0) {
                    tbody.innerHTML = `<tr><td colspan="2" style="text-align:center; color:#000000; font-weight:bold; font-style:italic;">Búsqueda Finalizada: Ningún bloque contiene el criterio solicitado.</td></tr>`;
                    return;
                }

                data.datos.forEach(item => {
                    tbody.innerHTML += `
                        <tr>
                            <td class="coord-cell" style="font-family:inherit; color:#000000; font-weight:bold; padding: 8px;">${item.coordenadas || item.coordinates}</td>
                            <td style="font-family:inherit; color:#333333; padding: 8px;">${item.registro}</td>
                        </tr>`;
                });

            } else {
                tbody.innerHTML = `<tr><td colspan="2" style="color:#000000; padding:15px; font-style:italic;">Falla en el motor de consultas: ${data.error}</td></tr>`;
            }
        } catch (err) {
            console.error("Excepción en hilo de consulta:", err);
        }
    });

    // REFRESCAR ELEMENTOS VISUALES EN BLANCO Y NEGRO
    function actualizarMetricasGlobales(sectoresOcupados) {
        const porcentaje = totalSectoresGlobal > 0 ? ((sectoresOcupados / totalSectoresGlobal) * 100).toFixed(2) : 0;
        const bar = document.getElementById('barra-progreso-disco');
        if (bar) bar.style.width = `${porcentaje}%`;

        const txtBytes = document.getElementById('txt-proporcion-bytes');
        if (txtBytes) {
            const bytesUsados = sectoresOcupados * bytesPorSectorGlobal;
            const formatoLegible = formatearEspacioAvanzado(bytesUsados);
            txtBytes.innerText = `${formatoLegible} usados (Progreso Físico: ${porcentaje}%)`;
        }

        actualizarMapaFisicoOptimizada(sectoresOcupados);
    }

    // MAPA DE BITS REFACTORIZADO A BLANCO Y NEGRO PURO
    function actualizarMapaFisicoOptimizada(sectoresOcupados) {
        const grid = document.getElementById('disk-bitmap');
        if (!grid) return;
        
        grid.innerHTML = ""; 

        const numeroCeldasRepresentativas = Math.min(totalSectoresGlobal, 100);
        const sectoresPorCelda = totalSectoresGlobal / numeroCeldasRepresentativas;

        for (let i = 0; i < numeroCeldasRepresentativas; i++) {
            const sectorLimiteInferior = i * sectoresPorCelda;
            const estaLleno = sectoresOcupados > sectorLimiteInferior;
            
            // Reemplazo del naranja por negro (#000000) si tiene datos y blanco (#ffffff) si está libre
            const colorFondo = estaLleno ? '#000000' : '#ffffff';
            const claseCelda = estaLleno ? "bitmap-cell filled" : "bitmap-cell";
            
            grid.innerHTML += `<div class="${claseCelda}" style="height:20px; border-radius:0px; border:1px solid #111111; background:${colorFondo}; transition: background 0.2s;"></div>`;
        }
    }
});