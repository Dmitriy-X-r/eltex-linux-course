#include <stdio.h>
#include <string.h>
#include "phonebook.h"

static void read_text(const char *prompt, char text[], int size) {
    printf("%s", prompt);

    if(fgets(text, size, stdin) != NULL) {
        text[strcspn(text, "\n")] = '\0';
    }
}

static int read_number(const char *prompt) {
    char buffer[32];
    int number;

    while(1) {
        read_text(prompt, buffer, sizeof(buffer));
        
        if(sscanf(buffer, "%d", &number) == 1) {
            return number;
        }

        printf("Введите целое число\n");
    }
}

static void read_required_text(const char *prompt, char text[], int size) {
    do {
        read_text(prompt, text, size);

        if(text[0] == '\0') {
            printf("Поле обязательно для заполнения\n");
        }
    } while(text[0] == '\0');
}

static void read_contact_data(Contact *contact) {
    read_required_text("Имя: ", contact->first_name, TEXT_LENGTH);

    read_required_text("Фамилия: ", contact->last_name, TEXT_LENGTH);

    read_text("Отчество (необязательно): ", contact->patronymic, TEXT_LENGTH);

    read_text("Место работы (необязательно): ", contact->workplace, TEXT_LENGTH);

    read_text("Должность (необязательно): ", contact->position, TEXT_LENGTH);

    contact->phone_count = 0;
    contact->email_count = 0;

    int count = read_number("Количество телефонов (0-3): ");

    while(count < 0 || count > MAX_PHONES) {
        printf("Допустимое количество: от 0 до %d.\n", MAX_PHONES);
        count = read_number("Количество телефонов: ");
    }

    for(int i = 0; i < count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Телефон %d: ", i + 1);

        read_required_text(prompt, contact->phones[i], TEXT_LENGTH);
        contact->phone_count++;
    }

    count = read_number("Количество адресов email (0-3): ");

    while(count < 0 || count > MAX_EMAILS) {
        printf("Допустимое количество: от 0 до %d.\n", MAX_EMAILS);
        count = read_number("Количество адресов email: ");
    }

    for(int i = 0; i < count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Email %d: ", i + 1);

        read_required_text(prompt, contact->emails[i], TEXT_LENGTH);
        contact->email_count++;
    }
}

int add_contact(Contact contacts[], int *contact_count) {
    if(*contact_count >= MAX_CONTACTS) {
        printf("Телефонная книга заполнена.\n");
        return 0;
    }

    Contact new_contact = {0};
    printf("\n--- Добавление контакта ---\n");
    read_contact_data(&new_contact);

    contacts[*contact_count] = new_contact;
    (*contact_count)++;

    return 1;
}

void show_contacts(const Contact contacts[], int contact_count) {
    if(contact_count == 0) {
        printf("Телефонная книга пустая\n");
        return;
    }

    for(int i = 0; i < contact_count; i++) {
        printf("\n--- Контакт %d ---\n", i + 1);
        printf("Имя: %s\n", contacts[i].first_name);
        printf("Фамилия: %s\n", contacts[i].last_name);
        printf("Отчество: %s\n", contacts[i].patronymic);
        printf("Место работы: %s\n", contacts[i].workplace);
        printf("Должность: %s\n", contacts[i].position);

        printf("Телефоны:\n");
        for(int j = 0; j < contacts[i].phone_count; j++) {
            printf("  %d. %s\n", j + 1, contacts[i].phones[j]);
        }

        printf("Адреса email:\n");
        for(int j = 0; j < contacts[i].email_count; j++) {
            printf("  %d. %s\n", j + 1, contacts[i].emails[j]);
        }
    }
}

static void edit_phones(Contact *contact) {
    int count = read_number("Количество телефонов (0-3): ");

    while (count < 0 || count > MAX_PHONES) {
        printf("Допустимое количество: от 0 до %d.\n", MAX_PHONES);
        count = read_number("Количество телефонов: ");
    }

    contact->phone_count = 0;

    for (int i = 0; i < count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Телефон %d: ", i + 1);
        read_required_text(prompt, contact->phones[i], TEXT_LENGTH);
        contact->phone_count++;
    }
}

static void edit_emails(Contact *contact) {
    int count = read_number("Количество адресов email (0-3): ");

    while (count < 0 || count > MAX_EMAILS) {
        printf("Допустимое количество: от 0 до %d.\n", MAX_EMAILS);
        count = read_number("Количество адресов email: ");
    }

    contact->email_count = 0;

    for (int i = 0; i < count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Email %d: ", i + 1);
        read_required_text(prompt, contact->emails[i], TEXT_LENGTH);
        contact->email_count++;
    }
}

int edit_contact(Contact contacts[], int contact_count) {
    if(contact_count == 0) {
        printf("Нет контактов для редактирования\n");
        return 0;
    }

    show_contacts(contacts, contact_count);

    int index = read_number("\nВведите номер контакта для редактирования:");

    if(index < 1 || index > contact_count) {
        printf("Неверный номер контакта\n");
        return 0;
    }

    Contact *contact = &contacts[index - 1];
    int choice;

    do {
        printf("\n=== Редактирование контакта ===\n");
        printf("1. Имя\n");
        printf("2. Фамилия\n");
        printf("3. Отчество\n");
        printf("4. Место работы\n");
        printf("5. Должность\n");
        printf("6. Телефоны\n");
        printf("7. Адреса email\n");
        printf("0. Завершить редактирование\n");

        choice = read_number("Ваш выбор: ");

        switch (choice) {
            case 1:
                read_required_text(
                    "Новое имя: ",
                    contact->first_name,
                    TEXT_LENGTH
                );
                break;

            case 2:
                read_required_text(
                    "Новая фамилия: ",
                    contact->last_name,
                    TEXT_LENGTH
                );
                break;

            case 3:
                read_text(
                    "Новое отчество: ",
                    contact->patronymic,
                    TEXT_LENGTH
                );
                break;

            case 4:
                read_text(
                    "Новое место работы: ",
                    contact->workplace,
                    TEXT_LENGTH
                );
                break;

            case 5:
                read_text(
                    "Новая должность: ",
                    contact->position,
                    TEXT_LENGTH
                );
                break;

            case 6:
                edit_phones(contact);
                break;

            case 7:
                edit_emails(contact);
                break;

            case 0:
                printf("Редактирование завершено\n");
                break;

            default:
                printf("Неизвестный пункт меню\n");
                break;
        }
    } while (choice != 0);

    return 1;
}

int delete_contact(Contact contacts[], int *contact_count) {
    if (*contact_count == 0) {
        printf("Нет контактов для удаления\n");
        return 0;
    }

    show_contacts(contacts, *contact_count);
    int index = read_number("\nВведите номер контакта для удаления: ");

    if (index < 1 || index > *contact_count) {
        printf("Неверный номер контакта\n");
        return 0;
    }

    for(int i = index-1; i < *contact_count - 1; i++) {
        contacts[i] = contacts[i+1];
    }
    
    (*contact_count)--;

    return 1;
}
