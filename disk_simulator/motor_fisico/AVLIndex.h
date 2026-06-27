#include "disk_engine.h"
#include <functional>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <utility>

template<typename A, typename B>
std::ostream& operator<<(std::ostream& os, const std::pair<A,B>& p) {
    return os << p.first;  // solo imprime la parte significativa (la columna)
}

// ── Entrada que el AVL guarda por nodo ──────────────────────────

struct IndexEntry {
    RecordID record_id;    // identificador lógico 
    uint32_t start_lba;    // LBA del primer sector → arg para engine.read()
    uint32_t num_sectors;  // cuántos accesos a disco hará engine.read()

    std::string to_string() const {
        std::ostringstream oss;
        oss << record_id.to_string()
            << " lba=" << start_lba
            << " nsec=" << num_sectors;
        return oss.str();
    }
};

// ────────────────────────────────────────────────────────────────

template <typename KeyType> //soporta pair para la clave compuesta
class AVLIndex {

// ─── Nodo interno ────────────────────────────────────────────────
private:
    struct Node {
        KeyType    key;
        IndexEntry entry;
        int        height;
        Node*      left;
        Node*      right;

        Node(const KeyType& k, const IndexEntry& e)
            : key(k), entry(e), height(1), left(nullptr), right(nullptr) {}
    };

    Node*  root_;
    size_t size_;

    // Métricas I/O (comparativa AVL vs escaneo secuencial)
    mutable size_t comparaciones_;
    mutable size_t accesos_disco_;

// ─── Utilidades AVL ──────────────────────────────────────────────

    int height(Node* n) const { return n ? n->height : 0; }

    int balance(Node* n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }

    void updateHeight(Node* n) {
        if (n) n->height = 1 + std::max(height(n->left), height(n->right));
    }

    Node* rotateRight(Node* y) {
        Node* x  = y->left;
        Node* T2 = x->right;
        x->right = y;
        y->left  = T2;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    Node* rotateLeft(Node* x) {
        Node* y  = x->right;
        Node* T2 = y->left;
        y->left  = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    Node* rebalance(Node* n) {
        updateHeight(n);
        int bf = balance(n);

        if (bf > 1  && balance(n->left)  >= 0) return rotateRight(n);          // LL
        if (bf > 1  && balance(n->left)  <  0) {                               // LR
            n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
        if (bf < -1 && balance(n->right) <= 0) return rotateLeft(n);           // RR
        if (bf < -1 && balance(n->right) >  0) {                               // RL
            n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
        return n;
    }

// ─── INSERT recursivo ────────────────────────────────────────────

    Node* insertNode(Node* n, const KeyType& key, const IndexEntry& entry) {
        if (!n) { size_++; return new Node(key, entry); }

        comparaciones_++;
        if      (key < n->key) n->left  = insertNode(n->left,  key, entry);
        else if (key > n->key) n->right = insertNode(n->right, key, entry);
        else                   n->entry = entry;   // duplicado: actualizar

        return rebalance(n);
    }

// ─── SEARCH exacta ───────────────────────────────────────────────

    Node* searchNode(Node* n, const KeyType& key) const {
        if (!n) return nullptr;
        comparaciones_++;
        if (key == n->key) return n;
        if (key <  n->key) return searchNode(n->left,  key);
        return                     searchNode(n->right, key);
    }

// ─── RANGE SEARCH In-order ───────────────────────────────────────

    void rangeInOrder(Node* n,
                      const KeyType& lo,
                      const KeyType& hi,
                      std::vector<std::pair<KeyType, IndexEntry>>& out) const {
        if (!n) return;
        comparaciones_++;
        if (lo < n->key) rangeInOrder(n->left,  lo, hi, out);
        if (lo <= n->key && n->key <= hi)
            out.push_back({n->key, n->entry});
        if (n->key < hi) rangeInOrder(n->right, lo, hi, out);
    }

// ─── REMOVE ──────────────────────────────────────────────────────

    Node* minNode(Node* n) { while (n->left) n = n->left; return n; }

    Node* removeNode(Node* n, const KeyType& key,
                     std::function<void(IndexEntry&)> onDelete) {
        if (!n) return nullptr;
        comparaciones_++;
        if      (key < n->key) n->left  = removeNode(n->left,  key, onDelete);
        else if (key > n->key) n->right = removeNode(n->right, key, onDelete);
        else {
            onDelete(n->entry);   // → engine.remove(entry.start_lba)

            if (!n->left || !n->right) {
                Node* tmp = n->left ? n->left : n->right;
                if (!tmp) { delete n; size_--; return nullptr; }
                *n = *tmp; delete tmp; size_--;
            } else {
                Node* succ = minNode(n->right);
                n->key   = succ->key;
                n->entry = succ->entry;
                n->right = removeNode(n->right, succ->key, [](IndexEntry&){});
            }
        }
        return rebalance(n);
    }

// ─── Destructor recursivo ─────────────────────────────────────────

    void destroy(Node* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }

// ─── Debug (árbol) ───────────────────────────────────────────────

    void printNode(Node* n, const std::string& pre, bool isLeft) const {
        if (!n) return;
        std::cout << pre << (isLeft ? "├── " : "└── ")
                  << n->key << "  [lba=" << n->entry.start_lba
                  << " nsec=" << n->entry.num_sectors
                  << " h=" << n->height << "]\n";
        printNode(n->left,  pre + (isLeft ? "│   " : "    "), true);
        printNode(n->right, pre + (isLeft ? "│   " : "    "), false);
    }

// ─── API pública ─────────────────────────────────────────────────
public:

    AVLIndex() : root_(nullptr), size_(0), comparaciones_(0), accesos_disco_(0) {}
    ~AVLIndex() { destroy(root_); }
    AVLIndex(const AVLIndex&)            = delete;
    AVLIndex& operator=(const AVLIndex&) = delete;

    size_t size()          const { return size_; }
    size_t comparaciones() const { return comparaciones_; }
    size_t accesos_disco() const { return accesos_disco_; }
    int    height()        const { return height(root_); }

    void resetMetrics() { comparaciones_ = 0; accesos_disco_ = 0; }

    // ── INSERT ──────────────────────────────────────────────────
    // se llama a engine.insert() → recibe WriteResult
    // Luego llama: avl.insert(key, rid, wr)
    //
    // IMPORTANTE: solo llamar si engine.insert() tuvo éxito.
    // Si falló por espacio, NO insertar (consistencia AVL ↔ disco).
    void insert(const KeyType& key,
                const RecordID& record_id,
                const WriteResult& wr) {
        IndexEntry e;
        e.record_id   = record_id;
        e.start_lba   = wr.start_lba;
        e.num_sectors = wr.num_sectors;
        root_ = insertNode(root_, key, e);
    }

    // ── BÚSQUEDA EXACTA ─────────────────────────────────────────
    // Retorna nullptr si no existe.
    // Si existe, suma num_sectors a accesos_disco_ (métrica I/O).
    //llama engine.read(entry->start_lba) con el resultado.
    const IndexEntry* search(const KeyType& key) const {
        Node* n = searchNode(root_, key);
        if (!n) return nullptr;
        accesos_disco_ += n->entry.num_sectors;
        return &n->entry;
    }

    // ── BÚSQUEDA POR RANGO ──────────────────────────────────────
    // In-order O(log n + k). Resultado ascendente por clave.
    // Iair itera el resultado y llama engine.read() por cada entrada.
    std::vector<std::pair<KeyType, IndexEntry>>
    rangeSearch(const KeyType& lo, const KeyType& hi) const {
        std::vector<std::pair<KeyType, IndexEntry>> result;
        rangeInOrder(root_, lo, hi, result);
        for (auto& [k, e] : result)
            accesos_disco_ += e.num_sectors;
        return result;
    }

    // ── ELIMINACIÓN LÓGICA ──────────────────────────────────────
    // onDelete debe llamar engine.remove(entry.start_lba)
    // El motor físico de Gabriela:
    //   1. Marca FLAG_FREE en el sector
    //   2. Libera el bitmap
    // Después de onDelete, el nodo se saca del AVL con rebalanceo.
    //
    // Uso típico:
    //   avl.remove(key, [&](IndexEntry& e){
    //       engine.remove(e.start_lba);
    //   });
    bool remove(const KeyType& key,
                std::function<void(IndexEntry&)> onDelete) {
        size_t antes = size_;
        root_ = removeNode(root_, key, onDelete);
        return size_ < antes;
    }

    // ── MÉTRICAS I/O ────────────────────────────────────────────
    // Para Panel B: comparativa AVL vs escaneo secuencial.
    // El escaneo secuencial siempre lee total_sectors del disco.
    std::string metricasString() const {
        std::ostringstream oss;
        oss << "AVL | comparaciones=" << comparaciones_
            << " | accesos_disco=" << accesos_disco_
            << " | nodos=" << size_;
        return oss.str();
    }

    // ── DEBUG ────────────────────────────────────────────────────
    void printTree() const {
        if (!root_) { std::cout << "(árbol vacío)\n"; return; }
        std::cout << root_->key
                  << "  [lba=" << root_->entry.start_lba
                  << " nsec=" << root_->entry.num_sectors
                  << " h=" << root_->height << "]\n";
        printNode(root_->left,  "", true);
        printNode(root_->right, "", false);
    }
};
