#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#define MAX_CONTACTS 100
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

void add_contact(Contact contacts[], int *contact_count);
void show_contacts(const Contact contacts[], int contact_count);
void edit_contact(Contact contacts[], int contact_count);
void delete_contact(Contact contacts[], int *contact_count);

#endif