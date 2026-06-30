document.addEventListener('DOMContentLoaded', () => {
    let totalSectoresGlobal = 0;
    let schemaColumns = [];
    let sectorSize = 120;

    const toastContainer = document.getElementById('toast-container');
    const statusTxt = document.getElementById('log-txt');
    const statusCsv = document.getElementById('log-csv');
    const selectQueryType = document.getElementById('select-query-type');
    const selectQueryCol = document.getElementById('select-query-col');
    const queryVal = document.getElementById('query-val');
    const queryMin = document.getElementById('query-min');
    const queryMax = document.getElementById('query-max');
    const boxVal = document.getElementById('box-val');
    const boxMin = document.getElementById('box-min');
    const boxMax = document.getElementById('box-max');
    const metricCapacity = document.getElementById('metric-capacity');
    const diskSummary = document.getElementById('disk-summary');
    const diskProgressFill = document.getElementById('disk-progress-fill');
    const diskProgressLabel = document.getElementById('disk-progress-label');
    const resultBody = document.querySelector('#table-results tbody');

    function showToast(message, type = 'info') {
        const toast = document.createElement('div');
        toast.className = `toast ${type}`;
        toast.innerText = message;
        toastContainer.appendChild(toast);
        setTimeout(() => toast.remove(), 4000);
    }

    function formatNumber(value) {
        return Number(value).toLocaleString('es-ES');
    }

    function formatBytes(bytes) {
        if (bytes < 1024) return `${bytes} B`;
        const kb = bytes / 1024;
        if (kb < 1024) return `${kb.toFixed(1)} KB`;
        const mb = kb / 1024;
        if (mb < 1024) return `${mb.toFixed(1)} MB`;
        return `${(mb / 1024).toFixed(1)} GB`;
    }

    function updateDiskSummary(usedSectors) {
        const totalBytes = totalSectoresGlobal * sectorSize;
        const usedBytes = usedSectors * sectorSize;
        const percent = totalSectoresGlobal ? Math.min(Math.round((usedSectors / totalSectoresGlobal) * 100), 100) : 0;

        diskSummary.innerHTML = `Usado: <strong>${formatNumber(usedSectors)}</strong> sectores · <strong>${formatBytes(usedBytes)}</strong><br>Capacidad total: <strong>${formatNumber(totalSectoresGlobal)}</strong> sectores · <strong>${formatBytes(totalBytes)}</strong>`;
        diskProgressFill.style.width = `${percent}%`;
        diskProgressLabel.innerText = `Uso del disco: ${percent}% (${formatBytes(usedBytes)} de ${formatBytes(totalBytes)})`;
    }

    function updateGeometryView(data) {
        totalSectoresGlobal = data.total_sectors;
        sectorSize = data.sector_size;
        metricCapacity.innerHTML = `Disco inicializado: <strong>${formatNumber(totalSectoresGlobal)} sectores</strong> · Sector = ${formatNumber(sectorSize)} bytes.`;
        // NO sobreescribimos los inputs — el usuario los configuró adrede
        actualizarMapaFisico(0);
        updateDiskSummary(0);
    }

    function setQueryMode(mode) {
        if (mode === 'exact') {
            boxVal.style.display = 'block';
            boxMin.style.display = 'none';
            boxMax.style.display = 'none';
            queryVal.placeholder = 'Por ejemplo: 1 o Juan';
        } else {
            boxVal.style.display = 'none';
            boxMin.style.display = 'block';
            boxMax.style.display = 'block';
            queryMin.placeholder = 'Valor mínimo';
            queryMax.placeholder = 'Valor máximo';
        }
    }

    function getSelectedColumnType() {
        const columnName = selectQueryCol.value;
        return schemaColumns.find(col => col.nombre === columnName) || null;
    }

    function renderResults(data) {
        document.getElementById('metricas-log').innerText = data.metricas;
        resultBody.innerHTML = '';

        if (!data.datos || data.datos.length === 0) {
            resultBody.innerHTML = `<tr><td colspan="2">No se obtuvieron resultados para la consulta.</td></tr>`;
            return;
        }

        data.datos.forEach(item => {
            const fieldsHtml = item.campos.map(line => `<div>${escapeHtml(line)}</div>`).join('');
            const locationHtml = item.lba ? `<div class="field-location">${escapeHtml(item.lba)}<br>${escapeHtml(item.chs || '')}</div>` : '<div class="field-location">Ubicación física no disponible</div>';
            resultBody.innerHTML += `
                <tr>
                    <td>${escapeHtml(item.titulo)}<br>${locationHtml}</td>
                    <td>${fieldsHtml}</td>
                </tr>
            `;
        });
    }

    function escapeHtml(text) {
        return text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#039;');
    }

    function isNumeric(value) {
        if (value === null || value === undefined) return false;
        return !Number.isNaN(Number(value));
    }

    document.getElementById('btn-init-disk').addEventListener('click', async () => {
        const platters         = parseInt(document.getElementById('in-platos').value)   || 2;
        const tracks           = parseInt(document.getElementById('in-pistas').value)   || 20;
        const sectors_per_track = parseInt(document.getElementById('in-sectores').value) || 64;
        const sector_size      = parseInt(document.getElementById('in-bytes').value)    || 120;

        const res = await fetch('/api/init', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ platters, tracks, sectors_per_track, sector_size })
        });
        const data = await res.json();
        if (data.success) {
            updateGeometryView(data);
            showToast('Disco físico inicializado correctamente.', 'success');
        } else {
            showToast('No se pudo inicializar el disco.', 'error');
        }
    });

    document.getElementById('txt-picker').addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (!file) return;
        document.getElementById('txt-file-name').innerText = file.name;
        statusTxt.innerText = 'Validando esquema...';

        const reader = new FileReader();
        reader.onload = async function(evt) {
            const res = await fetch('/api/create-table', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ddlText: evt.target.result })
            });
            const data = await res.json();
            if (!data.success) {
                statusTxt.innerHTML = `<span style="color:red">${data.error}</span>`;
                showToast(data.error, 'error');
                return;
            }

            schemaColumns = data.campos;
            const selectCol = selectQueryCol;
            selectCol.innerHTML = '';
            schemaColumns.forEach(col => {
                selectCol.innerHTML += `<option value="${col.nombre}">${col.nombre} (${col.tipo})</option>`;
            });

            selectCol.disabled = false;
            document.getElementById('csv-picker').disabled = false;
            document.getElementById('csv-file-name').innerText = 'Listo para cargar CSV.';
            statusTxt.innerHTML = `<span style="color:green">Esquema preparado: ${schemaColumns.length} columnas.</span>`;
            showToast('Estructura DDL cargada.', 'success');
            setQueryMode(selectQueryType.value);
        };
        reader.readAsText(file);
    });

    document.getElementById('csv-picker').addEventListener('change', function(e) {
        const file = e.target.files[0];
        if (!file) return;
        document.getElementById('csv-file-name').innerText = file.name;
        statusCsv.innerText = 'Enviando datos al motor físico...';

        const reader = new FileReader();
        reader.onload = async function(evt) {
            const res = await fetch('/api/load-csv', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ csvData: evt.target.result })
            });
            const data = await res.json();
            if (!data.success) {
                statusCsv.innerHTML = `<span style="color:red">${data.error}</span>`;
                showToast(data.error, 'error');
                return;
            }

            const omitidas = data.summary.total_omitidas || 0;
            const insertadas = data.summary.total_insertadas || 0;
            if (insertadas === 0) {
                statusCsv.innerHTML = `<span style="color:red">Error: el disco no tiene espacio suficiente. 0 filas insertadas.</span>`;
                showToast('El CSV supera la capacidad del disco. Ninguna fila fue insertada.', 'error');
            } else if (omitidas > 0) {
                statusCsv.innerHTML = `<span style="color:#fbbf24">Carga parcial: ${insertadas} filas insertadas, ${omitidas} omitidas por falta de espacio. Uso disco: ${data.sectores_ocupados} sectores.</span>`;
                actualizarMapaFisico(data.sectores_ocupados || totalSectoresGlobal);
                updateDiskSummary(data.sectores_ocupados || 0);
                showToast(`Carga parcial: ${omitidas} filas no cupieron en el disco.`, 'error');
            } else {
                statusCsv.innerHTML = `<span style="color:#7dd3fc">Carga completa: ${insertadas} filas. Uso disco: ${data.sectores_ocupados} sectores.</span>`;
                actualizarMapaFisico(data.sectores_ocupados || totalSectoresGlobal);
                updateDiskSummary(data.sectores_ocupados || 0);
                showToast('Datos cargados en el disco físico.', 'success');
            }
        };
        reader.readAsText(file);
    });

    selectQueryType.addEventListener('change', () => {
        setQueryMode(selectQueryType.value);
    });

    selectQueryCol.addEventListener('change', () => {
        const selected = getSelectedColumnType();
        if (selectQueryType.value === 'range' && selected && !selected.numeric) {
            showToast('Solo columnas numéricas admiten búsqueda por rango.', 'error');
        }
    });

    document.getElementById('btn-run-query').addEventListener('click', async () => {
        const tipo = selectQueryType.value;
        const columna = selectQueryCol.value;
        const valor = queryVal.value.trim();
        const min = queryMin.value.trim();
        const max = queryMax.value.trim();

        if (!columna) {
            showToast('Seleccione una columna para consultar.', 'error');
            return;
        }

        if (tipo === 'exact') {
            if (!valor) {
                showToast('Ingrese un valor para la búsqueda exacta.', 'error');
                return;
            }
        } else {
            const columnInfo = getSelectedColumnType();
            if (!columnInfo || !columnInfo.numeric) {
                showToast('La búsqueda por rango solo funciona con columnas numéricas.', 'error');
                return;
            }
            if (!min || !max) {
                showToast('Ingrese ambos límites mínimo y máximo.', 'error');
                return;
            }
            if (!isNumeric(min) || !isNumeric(max)) {
                showToast('Los límites deben ser valores numéricos válidos.', 'error');
                return;
            }
        }

        resultBody.innerHTML = `<tr><td colspan="2">Ejecutando consulta...</td></tr>`;
        const res = await fetch('/api/query', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tipo, columna, valor, min, max })
        });
        const data = await res.json();
        if (!data.success) {
            resultBody.innerHTML = `<tr><td colspan="2" style="color:red">${data.error}</td></tr>`;
            showToast(data.error, 'error');
            return;
        }

        renderResults(data);
        showToast('Consulta procesada correctamente.', 'success');
    });

    function actualizarMapaFisico(sectoresOcupados) {
        const grid = document.getElementById('disk-bitmap');
        if (!grid) return;
        grid.innerHTML = '';
        for (let i = 0; i < 200; i++) {
            const div = document.createElement('div');
            div.className = sectoresOcupados > i ? 'bitmap-cell filled' : 'bitmap-cell';
            grid.appendChild(div);
        }
    }

    setQueryMode(selectQueryType.value);
});