#include "ArbolAVL.h"
#include <sstream>
#include <algorithm>

AVLNode::AVLNode(std::string k, RecordID rid) {
    key = k;
    records.push_back(rid);
    height = 1;
    left = nullptr;
    right = nullptr;
}

int getHeight(AVLNode* n) { return n == nullptr ? 0 : n->height; }
int getBalance(AVLNode* n) { return n == nullptr ? 0 : getHeight(n->left) - getHeight(n->right); }

AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;
    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;
    return x;
}

AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = std::max(getHeight(x->left), getHeight(x->right)) + 1;
    y->height = std::max(getHeight(y->left), getHeight(y->right)) + 1;
    return y;
}

AVLNode* insertAVL(AVLNode* node, std::string key, RecordID rid) {
    if (node == nullptr) return new AVLNode(key, rid);

    if (key == node->key) {
        node->records.push_back(rid);
        return node;
    }

    double k_num, node_num;
    std::stringstream ss1(key), ss2(node->key);
    if ((ss1 >> k_num) && (ss2 >> node_num)) {
        if (k_num < node_num) node->left = insertAVL(node->left, key, rid);
        else node->right = insertAVL(node->right, key, rid);
    } else {
        if (key < node->key) node->left = insertAVL(node->left, key, rid);
        else node->right = insertAVL(node->right, key, rid);
    }

    node->height = 1 + std::max(getHeight(node->left), getHeight(node->right));
    int balance = getBalance(node);

    if (balance > 1) {
        std::stringstream ss_l(node->left->key);
        double left_num;
        std::stringstream ss1_retry(key);
        bool num = (ss1_retry >> k_num) && (ss_l >> left_num);
        if ((num && k_num < left_num) || (!num && key < node->left->key)) {
            return rightRotate(node);
        }
        if ((num && k_num > left_num) || (!num && key > node->left->key)) {
            node->left = leftRotate(node->left);
            return rightRotate(node);
        }
    }
    if (balance < -1) {
        std::stringstream ss_r(node->right->key);
        double right_num;
        std::stringstream ss1_retry(key);
        bool num = (ss1_retry >> k_num) && (ss_r >> right_num);
        if ((num && k_num > right_num) || (!num && key > node->right->key)) {
            return leftRotate(node);
        }
        if ((num && k_num < right_num) || (!num && key < node->right->key)) {
            node->right = rightRotate(node->right);
            return leftRotate(node);
        }
    }
    return node;
}

void collectExact(AVLNode* root, std::string key, std::vector<RecordID>& results, int& nodes_visited) {
    if (root == nullptr) return;
    nodes_visited++;

    if (root->key == key) {
        results.insert(results.end(), root->records.begin(), root->records.end());
        return;
    }

    double k_num, node_num;
    std::stringstream ss1(key), ss2(root->key);
    if ((ss1 >> k_num) && (ss2 >> node_num)) {
        if (k_num < node_num) collectExact(root->left, key, results, nodes_visited);
        else collectExact(root->right, key, results, nodes_visited);
    } else {
        if (key < root->key) collectExact(root->left, key, results, nodes_visited);
        else collectExact(root->right, key, results, nodes_visited);
    }
}

void collectRange(AVLNode* root, std::string min_k, std::string max_k, std::vector<RecordID>& results, int& nodes_visited) {
    if (root == nullptr) return;
    nodes_visited++;

    double val_num, min_num, max_num;
    std::stringstream ss_v(root->key), ss_min(min_k), ss_max(max_k);
    bool is_num = (ss_v >> val_num) && (ss_min >> min_num) && (ss_max >> max_num);

    if (is_num) {
        if (val_num >= min_num) collectRange(root->left, min_k, max_k, results, nodes_visited);
        if (val_num >= min_num && val_num <= max_num) {
            results.insert(results.end(), root->records.begin(), root->records.end());
        }
        if (val_num <= max_num) collectRange(root->right, min_k, max_k, results, nodes_visited);
    } else {
        if (root->key >= min_k) collectRange(root->left, min_k, max_k, results, nodes_visited);
        if (root->key >= min_k && root->key <= max_k) {
            results.insert(results.end(), root->records.begin(), root->records.end());
        }
        if (root->key <= max_k) collectRange(root->right, min_k, max_k, results, nodes_visited);
    }
}

int calculateTreeHeight(AVLNode* node) {
    return node == nullptr ? 0 : node->height;
}