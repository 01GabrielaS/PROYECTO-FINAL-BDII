#pragma once
#include <string>
#include <sstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────────
//  ColumnKey
//
//  Permite tener UN SOLO AVLIndex por columna sin importar si esa
//  columna es INT, FLOAT, CHAR o TEXT. El valor del CSV llega como
//  std::string; ColumnKey decide en cada comparación si compara
//  como número o como texto:
//
//    - Si AMBOS lados son convertibles a double -> compara numérico
//      (si comparara como texto, "10" < "9" sería true, lo cual
//       está mal para una columna numérica)
//    - Si no -> compara lexicográficamente (texto, fechas, etc.)
//
//  NOTA (AVL con clave compuesta): el AVLIndex ahora exige clave
//  única por nodo. Para permitir columnas con valores repetidos
//  (ej. dos alumnos con la misma nota), se indexa con la clave
//  compuesta std::pair<ColumnKey, uint32_t> donde el segundo
//  elemento es el start_lba del registro -> garantiza unicidad
//  aunque el valor de columna se repita. std::pair ya provee
//  operator<, ==, etc. de forma automática usando los de ColumnKey.
// ─────────────────────────────────────────────────────────────────
struct ColumnKey {
    std::string raw;

    ColumnKey() = default;
    ColumnKey(const std::string& s) : raw(s) {}
    ColumnKey(const char* s) : raw(s) {}

    static bool to_double(const std::string& s, double& out) {
        if (s.empty()) return false;
        std::stringstream ss(s);
        ss >> out;
        return !ss.fail() && ss.eof();
    }

    bool operator<(const ColumnKey& o) const {
        double a, b;
        if (to_double(raw, a) && to_double(o.raw, b)) return a < b;
        return raw < o.raw;
    }
    bool operator>(const ColumnKey& o) const { return o < *this; }
    bool operator<=(const ColumnKey& o) const { return !(o < *this); }
    bool operator>=(const ColumnKey& o) const { return !(*this < o); }
    bool operator==(const ColumnKey& o) const {
        double a, b;
        if (to_double(raw, a) && to_double(o.raw, b)) return a == b;
        return raw == o.raw;
    }

    friend std::ostream& operator<<(std::ostream& os, const ColumnKey& k) {
        return os << k.raw;
    }
};
