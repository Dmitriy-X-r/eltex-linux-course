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
    struct ContactNode *prev;
    struct ContactNode *next;
} ContactNode;

typedef struct ContactList {
    ContactNode *head;
    ContactNode *tail;
    size_t size;
} ContactList;

void phonebook_init(ContactList *list);

int phonebook_add(ContactList *list, const Contact *contact);
int phonebook_remove(ContactList *list, size_t index);
int phonebook_update(ContactList *list, size_t index, const Contact *updated_contact);

const Contact *phonebook_get_at(const ContactList *list, size_t index);
size_t phonebook_count(const ContactList *list);

void phonebook_clear(ContactList *list);

#endif
