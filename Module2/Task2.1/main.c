#include <stdio.h>
#include "phonebook.h"

int main(void)
{
    Contact contacts[MAX_CONTACTS] = {0};
    int contact_count = 0;
    int choice;

    char buffer[32];

    while (1) {
        printf("\n=== Телефонная книга ===\n");
        printf("1. Добавить контакт\n");
        printf("2. Показать контакты\n");
        printf("3. Редактировать контакт\n");
        printf("4. Удалить контакт\n");
        printf("0. Выход\n");
        printf("Ваш выбор: ");

        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        if (sscanf(buffer, "%d", &choice) != 1) {
            printf("Введите номер пункта меню.\n");
            continue;
        }

        switch (choice) {
            case 1:
                add_contact(contacts, &contact_count);
                break;

            case 2:
                show_contacts(contacts, contact_count);
                break;

            case 3:
                edit_contact(contacts, contact_count);
                break;

            case 4:
                delete_contact(contacts, &contact_count);
                break;

            case 0:
                printf("Выход из программы\n");
                return 0;

            default:
                printf("Неизвестный пункт меню\n");
                break;
        }
    }

    return 0;
}