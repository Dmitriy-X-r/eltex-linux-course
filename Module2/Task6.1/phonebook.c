#include <stdlib.h>
#include <string.h>
#include "phonebook.h"

void phonebook_init(ContactList *list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

size_t phonebook_count(const ContactList *list) {
    return list->size;  
}

static int compare_contacts(const Contact *first, const Contact *second) {
    int result = strcmp(first->last_name, second->last_name);

    if (result != 0) {
        return result;
    }

    result = strcmp(first->first_name, second->first_name);

    if (result != 0) {
        return result;
    }

    return strcmp(first->patronymic, second->patronymic);
}

static ContactNode *create_node(const Contact *contact) {
    ContactNode *node = malloc(sizeof(ContactNode));

    if(node == NULL) {
        return NULL;
    }

    node->contact = *contact;
    node->prev = NULL;
    node->next = NULL;

    return node;
}

static void insert_before(ContactList *list, ContactNode *position, ContactNode *node) {
    node->prev = position->prev;
    node->next = position;

    if(position->prev !=NULL) {
        position->prev->next = node;
    } else {
        list->head = node;
    }

    position->prev = node;
    list->size++;
}

int phonebook_add(ContactList *list, const Contact *contact) {
    ContactNode *node = create_node(contact);

    if(node == NULL) {
        return 0;
    }

    ContactNode *current = list->head;

    while (current != NULL) {
        if (compare_contacts(&node->contact, &current->contact) < 0) {
            insert_before(list, current, node);
            return 1;
        }

        current = current->next;
    }

    node->prev = list->tail;
    node->next = NULL;

    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }

    list->tail = node;
    list->size++;

    return 1;
}

static ContactNode *get_node_at(const ContactList *list, size_t index) {
    if (index >= list->size) {
        return NULL;
    }

    ContactNode *current = list->head;

    for (size_t i = 0; i < index; i++) {
        current = current->next;
    }

    return current;
}

const Contact *phonebook_get_at(const ContactList *list, size_t index) {
    ContactNode *node = get_node_at(list, index);

    if (node == NULL) {
        return NULL;
    }

    return &node->contact;
}

int phonebook_remove(ContactList *list, size_t index) {
    ContactNode *node = get_node_at(list, index);

    if (node == NULL) {
        return 0;
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    free(node);
    list->size--;

    return 1;
}

static void detach_node(ContactList *list, ContactNode *node) {
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    node->prev = NULL;
    node->next = NULL;
    list->size--;
}

static void insert_sorted_node(ContactList *list, ContactNode *node) {
    ContactNode *current = list->head;

    while (current != NULL) {
        if (compare_contacts(&node->contact, &current->contact) < 0) {
            insert_before(list, current, node);
            return;
        }

        current = current->next;
    }

    node->prev = list->tail;
    node->next = NULL;

    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }

    list->tail = node;
    list->size++;
}

int phonebook_update(ContactList *list, size_t index, const Contact *updated_contact) {
    ContactNode *node = get_node_at(list, index);

    if (node == NULL || updated_contact == NULL) {
        return 0;
    }

    Contact replacement = *updated_contact;

    detach_node(list, node);

    node->contact = replacement;

    insert_sorted_node(list, node);

    return 1;
}

void phonebook_clear(ContactList *list) {
    ContactNode *current = list->head;

    while (current != NULL) {
        ContactNode *next = current->next;

        free(current);

        current = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}
