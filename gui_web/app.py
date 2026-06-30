from flask import Flask, request, jsonify
import subprocess
import os

app = Flask(__name__, static_folder='.', static_url_path='')

# Rutas de almacenamiento temporal dentro de tu estructura real
DATA_DIR = os.path.join(os.path.dirname(__file__), '../data')
os.makedirs(DATA_DIR, exist_ok=True)

# Nombre del binario compilado real
BINARY_PATH = os.path.join(os.path.dirname(__file__), '../apps/main_demo')

# Configuración del motor C++ tal como está compilado hoy
DEFAULT_GEOMETRY = {
    'platters': 2,
    'tracks': 20,
    'sectors_per_track': 64,
    'sector_size': 120,
}

# Variable global para mantener el proceso de búsqueda abierto
process = None
CURRENT_SCHEMA = []

def normalize_type(type_str):
    t = type_str.strip().upper().replace(' ', '')
    if t.startswith('VARCHAR'):
        return 'VARCHAR'
    if t.startswith('CHAR'):
        return 'CHAR'
    if t.startswith('TEXT'):
        return 'TEXT'
    if t.startswith('BLOB'):
        return 'BLOB'
    if t.startswith('TIMESTAMP'):
        return 'TIMESTAMP'
    if t.startswith('DATETIME'):
        return 'DATETIME'
    if t == 'DATE':
        return 'DATE'
    if t == 'TIME':
        return 'TIME'
    if t == 'YEAR':
        return 'YEAR'
    if t == 'INT':
        return 'INT'
    if t == 'FLOAT':
        return 'FLOAT'
    return None


def is_numeric_type(type_str):
    return type_str in ('INT', 'FLOAT')


def parse_schema_columns(ddl_text):
    import re
    pattern = re.compile(
        r'([A-Za-z_]\w*)\s+(INT|FLOAT|CHAR\s*\(\s*\d+\s*\)|VARCHAR\s*\(\s*\d+\s*\)|TEXT\s*\(\s*\d+\s*\)|BLOB\s*\(\s*\d+\s*\)|DATE(?:\s*\(\s*\d+\s*\))?|DATETIME(?:\s*\(\s*\d+\s*\))?|TIMESTAMP(?:\s*\(\s*\d+\s*\))?|TIME(?:\s*\(\s*\d+\s*\))?|YEAR(?:\s*\(\s*\d+\s*\))?)',
        re.IGNORECASE
    )
    columns = []
    for name, raw_type in pattern.findall(ddl_text):
        normalized = normalize_type(raw_type)
        if not normalized:
            continue

        size = 0
        if normalized in ('CHAR', 'VARCHAR', 'TEXT', 'BLOB', 'DATE', 'TIME', 'DATETIME', 'TIMESTAMP', 'YEAR'):
            size_match = re.search(r'\((\d+)\)', raw_type)
            if size_match:
                size = int(size_match.group(1))
            elif normalized == 'CHAR':
                size = 1
            elif normalized == 'VARCHAR':
                size = 64
            elif normalized == 'TEXT':
                size = 256
            elif normalized == 'BLOB':
                size = 256
            elif normalized == 'DATE':
                size = 10
            elif normalized == 'TIME':
                size = 8
            elif normalized in ('DATETIME', 'TIMESTAMP'):
                size = 19
            elif normalized == 'YEAR':
                size = 4
        elif normalized == 'INT':
            size = 4
        elif normalized == 'FLOAT':
            size = 4

        if size <= 0:
            continue

        columns.append({
            'nombre': name.upper(),
            'tipo': normalized,
            'size': size,
            'numeric': is_numeric_type(normalized)
        })
    return columns


def is_number(value):
    try:
        float(value)
        return True
    except ValueError:
        return False


def find_schema_column(name):
    for col in CURRENT_SCHEMA:
        if col['nombre'] == name:
            return col
    return None


def parse_query_results(lines):
    results = []
    current = None
    for line in lines:
        if line.startswith('Total:'):
            continue
        if line.startswith('No se encontraron'):
            continue

        if line.startswith('[') and 'RecordID' in line:
            if current:
                results.append(current)
            current = {'title': line, 'fields': [], 'location': None, 'chs': None}
            continue

        if current is None:
            continue

        if line.startswith('LBA='):
            current['location'] = line
            continue
        if line.startswith('CHS:'):
            current['chs'] = line
            continue

        if line:
            current['fields'].append(line)

    if current:
        results.append(current)
    return results


def ensure_schema_loaded():
    global CURRENT_SCHEMA
    if CURRENT_SCHEMA:
        return
    txt_path = os.path.join(DATA_DIR, 'estructura.txt')
    if not os.path.exists(txt_path):
        return
    with open(txt_path, 'r', encoding='utf-8') as f:
        CURRENT_SCHEMA = parse_schema_columns(f.read())


@app.route('/')
def index():
    return app.send_static_file('index.html')

@app.route('/api/init', methods=['POST'])
def init_disk():
    total_sectors = (DEFAULT_GEOMETRY['platters'] * 2 *
                     DEFAULT_GEOMETRY['tracks'] * DEFAULT_GEOMETRY['sectors_per_track'])
    return jsonify({
        "success": True,
        "platters": DEFAULT_GEOMETRY['platters'],
        "tracks": DEFAULT_GEOMETRY['tracks'],
        "sectors_per_track": DEFAULT_GEOMETRY['sectors_per_track'],
        "sector_size": DEFAULT_GEOMETRY['sector_size'],
        "total_sectors": total_sectors
    })

@app.route('/api/create-table', methods=['POST'])
def create_table():
    global CURRENT_SCHEMA
    ddl_text = request.json.get("ddlText", "")
    txt_path = os.path.join(DATA_DIR, 'estructura.txt')

    columns = parse_schema_columns(ddl_text)
    if not columns:
        return jsonify({"success": False, "error": "No se detectaron columnas válidas en el TXT. Use un CREATE TABLE con tipos admitidos."})

    CURRENT_SCHEMA = columns
    with open(txt_path, 'w', encoding='utf-8') as f:
        f.write(ddl_text)

    return jsonify({"success": True, "campos": columns})

@app.route('/api/load-csv', methods=['POST'])
def load_csv():
    global process
    csv_data = request.json.get("csvData", "")
    
    txt_path = os.path.join(DATA_DIR, 'estructura.txt')
    csv_path = os.path.join(DATA_DIR, 'alumnos.csv')
    
    with open(csv_path, 'w', encoding='utf-8') as f:
        f.write(csv_data)
        
    if process and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
        process = None

    binary = BINARY_PATH
    if not os.path.exists(binary):
        return jsonify({"success": False, "error": f"Falta el ejecutable en: {binary}"})

    try:
        # Lanzamos tu motor real C++ en segundo plano interactivo
        process = subprocess.Popen(
            [binary, txt_path, csv_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        
        output_lines = []
        sectores_ocupados = 0
        filas_leidas = 0
        filas_insertadas = 0
        filas_omitidas = 0
        seen_search_menu = False

        while True:
            line = process.stdout.readline()
            if not line:
                break
            raw = line.strip()
            output_lines.append(raw)
            if "Uso disco:" in raw:
                parts = raw.split("Uso disco:")[-1].split("/")
                sectores_ocupados = int(parts[0].strip())
            if "Carga completa:" in raw:
                import re
                m = re.search(r"Carga completa:\s*(\d+) filas insertadas", raw)
                if m:
                    filas_insertadas = int(m.group(1))
            if "Leidas:" in raw and "Insertadas:" in raw:
                import re
                m = re.search(r"Leidas:\s*(\d+)\s*\|\s*Insertadas:\s*(\d+)\s*\|\s*Omitidas:\s*(\d+)", raw)
                if m:
                    filas_leidas = int(m.group(1))
                    filas_insertadas = int(m.group(2))
                    filas_omitidas = int(m.group(3))
            if "=== Motor de busquedas" in raw:
                seen_search_menu = True
                continue
            if seen_search_menu and "Tipo de busqueda:" in raw:
                break

        console_log = "\n".join(output_lines)
        summary = {
            "total_leidas": filas_leidas,
            "total_insertadas": filas_insertadas,
            "total_omitidas": filas_omitidas,
        }
        return jsonify({
            "success": True,
            "insertados": filas_insertadas,
            "sectores_ocupados": sectores_ocupados,
            "summary": summary,
            "console": console_log
        })
        
    except Exception as e:
        return jsonify({"success": False, "error": str(e)})

@app.route('/api/query', methods=['POST'])
def query_data():
    global process
    if not process or process.poll() is not None:
        return jsonify({"success": False, "error": "El motor C++ no está activo. Cargue primero un esquema y un CSV."})

    ensure_schema_loaded()
    req = request.json
    modo = req.get("tipo")
    tipo = "EXACTO" if modo == "exact" else "RANGO"
    columna = req.get("columna", "").upper()

    schema_column = find_schema_column(columna)
    if not schema_column:
        return jsonify({"success": False, "error": "Columna no reconocida en el esquema actual."})

    if tipo == "EXACTO":
        valor = req.get("valor", "").strip()
        if not valor:
            return jsonify({"success": False, "error": "Debe ingresar un valor para la búsqueda exacta."})
    else:
        if not schema_column['numeric']:
            return jsonify({"success": False, "error": "La búsqueda por rango solo está disponible para columnas numéricas."})
        lo = req.get("min", "").strip()
        hi = req.get("max", "").strip()
        if not lo or not hi:
            return jsonify({"success": False, "error": "Debe ingresar ambos límites mínimo y máximo."})
        if not is_number(lo) or not is_number(hi):
            return jsonify({"success": False, "error": "Los límites de rango deben ser valores numéricos."})
        valor = None

    try:
        process.stdin.write(f"{tipo}\n")
        process.stdin.write(f"{columna}\n")

        if tipo == "EXACTO":
            process.stdin.write(f"{valor}\n")
        else:
            process.stdin.write(f"{lo}\n")
            process.stdin.write(f"{hi}\n")
        process.stdin.flush()

        results_lines = []
        metricas = "Resultados de búsqueda cargados"

        while True:
            line = process.stdout.readline()
            if not line or "Tipo de busqueda:" in line:
                break
            clean_line = line.strip()
            if not clean_line:
                continue
            if "AVL |" in clean_line or clean_line.startswith("Métricas") or clean_line.startswith("Comparaciones"):
                metricas = clean_line
            elif "No se encontraron" in clean_line:
                results_lines.append(clean_line)
            elif "Total:" in clean_line:
                results_lines.append(clean_line)
            else:
                results_lines.append(clean_line)

        parsed_results = parse_query_results(results_lines)
        datos_parsed = []
        for item in parsed_results:
            datos_parsed.append({
                "titulo": item['title'],
                "campos": item['fields'],
                "lba": item['location'],
                "chs": item['chs']
            })

        return jsonify({"success": True, "metricas": metricas, "datos": datos_parsed})

    except Exception as e:
        return jsonify({"success": False, "error": str(e)})

@app.route('/api/insert-manual', methods=['POST'])
def insert_manual_fallback():
    return jsonify({"success": True, "sectores_ocupados": 45})


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Arranca el backend Flask del GUI')
    parser.add_argument('--port', type=int, default=int(os.environ.get('PORT', 5000)),
                        help='Puerto HTTP para la aplicación (por defecto 5000)')
    args = parser.parse_args()

    try:
        app.run(debug=True, port=args.port)
    except OSError as err:
        if 'Address already in use' in str(err) and args.port == 5000:
            alt_port = 5001
            print(f'Puerto {args.port} ocupado. Intentando iniciar en {alt_port}...')
            app.run(debug=True, port=alt_port)
        else:
            raise