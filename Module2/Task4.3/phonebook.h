#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <stddef.h>

#define MAX_PHONES 3
#define MAX_EMAILS 3
#define TEXT_LENGTH 100

typedef struct Contact {
    char first_name[TEXT_LENGTH];
    char last_name[TEXT_LENGTH];
    char patronymic[TEXT_LENGTH];
    char workplace[TEXT_LENGTH];
    char position[TEXT_LENGTH];

    char phones[MAX_PHONES][TEXT_LENGTH];
    int phone_count;

    char emails[MAX_EMAILS][TEXT_LENGTH];
    int email_count;
} Contact;

typedef struct ContactNode {
    Contact contact;
    struct ContactNode *left;
    struct ContactNode *right;
    int height;
} ContactNode;

typedef struct ContactTree {
    ContactNode *root;
    size_t size;
} ContactTree;

void phonebook_init(ContactTree *tree);

int phonebook_add(ContactTree *tree, const Contact *contact);
int phonebook_remove(ContactTree *tree, size_t index);
int phonebook_update(ContactTree *tree, size_t index, const Contact *updated_contact);

const Contact *phonebook_get_at(const ContactTree *tree, size_t index);
size_t phonebook_count(const ContactTree *tree);

void phonebook_clear(ContactTree *tree);

#endif