#include <stdlib.h>
#include <string.h>

#include "phonebook.h"

static int compare_contacts(const Contact *first, const Contact *second) {
    int result = strcmp(first->last_name, second->last_name);

    if(result != 0) {
        return result;
    }

    result = strcmp(first->first_name, second->first_name);

    if (result != 0) {
        return result;
    }

    return strcmp(first->patronymic, second->patronymic);
}

static int node_height(const ContactNode *node) {
    if(node == NULL) {
        return 0;
    }

    return node->height;
}

static int balance_factor(const ContactNode *node) {
    if(node == NULL) {
        return 0;
    }

    return node_height(node->right) - node_height(node->left);
}

static void update_height(ContactNode *node) {
    int left_height = node_height(node->left);
    int right_height = node_height(node->right);

    if(left_height > right_height) {
        node->height = left_height + 1;
    } else {
        node->height = right_height + 1;
    }
}

static ContactNode *rotate_right(ContactNode *root) {
    ContactNode *new_root = root->left;
    ContactNode *middle = new_root->right;

    new_root->right = root;
    root->left = middle;

    update_height(root);
    update_height(new_root);

    return new_root;
}

static ContactNode *rotate_left(ContactNode *root) {
    ContactNode *new_root = root->right;
    ContactNode *middle = new_root->left;

    new_root->left = root;
    root->right = middle;

    update_height(root);
    update_height(new_root);

    return new_root;
}

static ContactNode *balance_node(ContactNode *node) {
    if (node == NULL) {
        return NULL;
    }

    update_height(node);

    int balance = balance_factor(node);

    if (balance < -1) {
        if (balance_factor(node->left) > 0) {
            node->left = rotate_left(node->left);
        }

        return rotate_right(node);
    }

    if (balance > 1) {
        if (balance_factor(node->right) < 0) {
            node->right = rotate_right(node->right);
        }

        return rotate_left(node);
    }

    return node;
}

void phonebook_init(ContactTree *tree) {
    tree->root = NULL;
    tree->size = 0;
}

static ContactNode *create_node(const Contact *contact) {
    ContactNode *node = malloc(sizeof(ContactNode));

    if (node == NULL) {
        return NULL;
    }

    node->contact = *contact;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;

    return node;
}

static ContactNode *insert_node(ContactNode *root, ContactNode *node) {
    if(root == NULL) {
        return node;
    }

    if(compare_contacts(&node->contact, &root->contact) < 0) {
        root->left = insert_node(root->left, node);
    } else {
        root->right = insert_node(root->right, node);
    }

    return balance_node(root);
}

int phonebook_add(ContactTree *tree, const Contact *contact) {
    if (contact == NULL || contact->first_name[0] == '\0' || contact->last_name[0] == '\0') {
        return 0;
    }

    ContactNode *node = create_node(contact);

    if(node == NULL) {
        return 0;
    }

    tree->root = insert_node(tree->root, node);
    tree->size++;

    return 1;
}

size_t phonebook_count(const ContactTree *tree) {
    return tree->size;
}

static const Contact *get_at_recursive(const ContactNode *node, size_t index, size_t *position) {
    if (node == NULL) {
        return NULL;
    }

    const Contact *found = get_at_recursive(node->left, index, position);

    if(found != NULL) {
        return found;
    }

    if(*position == index) {
        return &node->contact;
    }

    (*position)++;

    return get_at_recursive(node->right, index, position);
}

const Contact *phonebook_get_at(const ContactTree *tree, size_t index) {
    if (index >= tree->size) {
        return NULL;
    }

    size_t position = 0;

    return get_at_recursive(tree->root, index, &position);
}

static ContactNode *remove_min(ContactNode *root, ContactNode **min_node) {
    if (root->left == NULL) {
        *min_node = root;
        return root->right;
    }

    root->left = remove_min(root->left, min_node);

    return balance_node(root);
}

static ContactNode *remove_root(ContactNode *root) {
    if (root->left == NULL) {
        ContactNode *right = root->right;
        free(root);
        return right;
    }

    if (root->right == NULL) {
        ContactNode *left = root->left;
        free(root);
        return left;
    }

    ContactNode *successor = NULL;

    ContactNode *right = remove_min(root->right, &successor);

    successor->left = root->left;
    successor->right = right;

    free(root);

    return balance_node(successor);
}

static ContactNode *remove_at_recursive(ContactNode *root, size_t index, size_t *position, int *removed) {
    if (root == NULL) {
        return NULL;
    }

    root->left = remove_at_recursive(root->left, index, position, removed);

    if (*removed) {
        return balance_node(root);
    }

    if (*position == index) {
        *removed = 1;
        return remove_root(root);
    }

    (*position)++;

    root->right = remove_at_recursive(root->right, index, position, removed);

    if (*removed) {
        return balance_node(root);
    }

    return root;
}

int phonebook_remove(ContactTree *tree, size_t index) {
    if (index >= tree->size) {
        return 0;
    }

    size_t position = 0;
    int removed = 0;

    tree->root = remove_at_recursive(tree->root, index, &position, &removed);

    if (removed) {
        tree->size--;
    }

    return removed;
}

int phonebook_update(ContactTree *tree, size_t index, const Contact *updated_contact) {
    if (updated_contact == NULL || updated_contact->first_name[0] == '\0' ||  updated_contact->last_name[0] == '\0' || index >= tree->size) {
        return 0;
    }

    ContactNode *new_node = create_node(updated_contact);

    if (new_node == NULL) {
        return 0;
    }

    size_t position = 0;
    int removed = 0;

    tree->root = remove_at_recursive(tree->root, index, &position, &removed);

    if (!removed) {
        free(new_node);
        return 0;
    }

    tree->size--;

    tree->root = insert_node(tree->root, new_node);
    tree->size++;

    return 1;
}

static void find_prefix_recursive(
    const ContactNode *node,
    const char *prefix,
    size_t prefix_length,
    size_t *position,
    size_t *indices,
    size_t capacity,
    size_t *found
) {
    if (node == NULL) {
        return;
    }

    find_prefix_recursive(node->left, prefix, prefix_length,
                          position, indices, capacity, found);

    if (strncmp(node->contact.last_name, prefix,
                prefix_length) == 0) {
        if (*found < capacity) {
            indices[*found] = *position;
        }
        (*found)++;
    }

    (*position)++;

    find_prefix_recursive(node->right, prefix, prefix_length,
                          position, indices, capacity, found);
}

size_t phonebook_find_by_last_name_prefix(
    const ContactTree *tree,
    const char *prefix,
    size_t *indices,
    size_t capacity
) {
    if (tree == NULL || prefix == NULL || prefix[0] == '\0') {
        return 0;
    }

    size_t position = 0;
    size_t found = 0;

    find_prefix_recursive(tree->root, prefix, strlen(prefix),
                          &position, indices, capacity, &found);

    return found;
}

static void clear_nodes(ContactNode *root) {
    if (root == NULL) {
        return;
    }

    clear_nodes(root->left);
    clear_nodes(root->right);

    free(root);
}

void phonebook_clear(ContactTree *tree) {
    clear_nodes(tree->root);

    tree->root = NULL;
    tree->size = 0;
}










