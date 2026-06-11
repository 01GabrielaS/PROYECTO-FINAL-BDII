#ifndef ARBOLAVL_H
#define ARBOLAVL_H

#include "DiscoVirtual.h"
#include <string>
#include <vector>

// NODO DEL ÁRBOL AVL COMPATIBLE CON DUPLICADOS
struct AVLNode {
    std::string key;
    std::vector<RecordID> records;
    int height;
    AVLNode* left;
    AVLNode* right;

    AVLNode(std::string k, RecordID rid);
};

// FUNCIONES AUXILIARES OPERATIVAS DEL ÁRBOL AVL
int getHeight(AVLNode* n);
int getBalance(AVLNode* n);
AVLNode* rightRotate(AVLNode* y);
AVLNode* leftRotate(AVLNode* x);
AVLNode* insertAVL(AVLNode* node, std::string key, RecordID rid);

void collectExact(AVLNode* root, std::string key, std::vector<RecordID>& results, int& nodes_visited);
void collectRange(AVLNode* root, std::string min_k, std::string max_k, std::vector<RecordID>& results, int& nodes_visited);
int calculateTreeHeight(AVLNode* node);

#endif // ARBOLAVL_H