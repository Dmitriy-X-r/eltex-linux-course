#include <stdio.h>
#include <string.h>

#include "phonebook.h"

static int read_text(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);

    if (fgets(buffer, size, stdin) == NULL) {
        return 0;
    }

    size_t length = strlen(buffer);

    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[length - 1] = '\0';
    } else {
        int ch;

        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
    }

    return 1;
}

static int read_number(const char *prompt, int *number) {
    char buffer[TEXT_LENGTH];
    char extra;

    while (1) {
        if (!read_text(prompt, buffer, sizeof(buffer))) {
            return 0;
        }

        if (sscanf(buffer, "%d %c", number, &extra) == 1) {
            return 1;
        }

        printf("Ошибка: введите целое число.\n");
    }
}

static int read_required_text(const char *prompt,
                              char *buffer,
                              size_t size) {
    while (1) {
        if (!read_text(prompt, buffer, size)) {
            return 0;
        }

        if (buffer[0] != '\0') {
            return 1;
        }

        printf("Поле не должно быть пустым.\n");
    }
}

static int read_contact_data(Contact *contact) {
    int count;

    memset(contact, 0, sizeof(*contact));

    if (!read_required_text("Имя: ",
                            contact->first_name,
                            sizeof(contact->first_name))) {
        return 0;
    }

    if (!read_required_text("Фамилия: ",
                            contact->last_name,
                            sizeof(contact->last_name))) {
        return 0;
    }

    if (!read_text("Отчество: ",
                   contact->patronymic,
                   sizeof(contact->patronymic))) {
        return 0;
    }

    if (!read_text("Место работы: ",
                   contact->workplace,
                   sizeof(contact->workplace))) {
        return 0;
    }

    if (!read_text("Должность: ",
                   contact->position,
                   sizeof(contact->position))) {
        return 0;
    }

    while (1) {
        if (!read_number("Количество телефонов (0-3): ", &count)) {
            return 0;
        }

        if (count >= 0 && count <= MAX_PHONES) {
            break;
        }

        printf("Ошибка: допустимое количество телефонов — от 0 до %d.\n",
               MAX_PHONES);
    }

    contact->phone_count = count;

    for (int i = 0; i < contact->phone_count; i++) {
        char prompt[TEXT_LENGTH];

        snprintf(prompt, sizeof(prompt), "Телефон %d: ", i + 1);

        if (!read_required_text(prompt,
                                contact->phones[i],
                                sizeof(contact->phones[i]))) {
            return 0;
        }
    }

    while (1) {
        if (!read_number("Количество email (0-3): ", &count)) {
            return 0;
        }

        if (count >= 0 && count <= MAX_EMAILS) {
            break;
        }

        printf("Ошибка: допустимое количество email — от 0 до %d.\n",
               MAX_EMAILS);
    }

    contact->email_count = count;

    for (int i = 0; i < contact->email_count; i++) {
        char prompt[TEXT_LENGTH];

        snprintf(prompt, sizeof(prompt), "Email %d: ", i + 1);

        if (!read_required_text(prompt,
                                contact->emails[i],
                                sizeof(contact->emails[i]))) {
            return 0;
        }
    }

    return 1;
}

static void print_contact(const Contact *contact, size_t index) {
    printf("\n--- Контакт №%zu ---\n", index);
    printf("Имя: %s\n", contact->first_name);
    printf("Фамилия: %s\n", contact->last_name);
    printf("Отчество: %s\n", contact->patronymic);
    printf("Место работы: %s\n", contact->workplace);
    printf("Должность: %s\n", contact->position);

    printf("Телефоны:\n");

    if (contact->phone_count == 0) {
        printf("  Не указаны\n");
    } else {
        for (int i = 0; i < contact->phone_count; i++) {
            printf("  %d. %s\n", i + 1, contact->phones[i]);
        }
    }

    printf("Email:\n");

    if (contact->email_count == 0) {
        printf("  Не указаны\n");
    } else {
        for (int i = 0; i < contact->email_count; i++) {
            printf("  %d. %s\n", i + 1, contact->emails[i]);
        }
    }
}

static void show_contacts(const ContactList *list) {
    size_t count = phonebook_count(list);

    if (count == 0) {
        printf("\nТелефонная книга пуста.\n");
        return;
    }

    printf("\n=== Телефонная книга (%zu) ===\n", count);

    for (size_t i = 0; i < count; i++) {
        const Contact *contact = phonebook_get_at(list, i);

        if (contact != NULL) {
            print_contact(contact, i + 1);
        }
    }
}

static int read_contact_index(const ContactList *list, size_t *index) {
    int number;

    if (phonebook_count(list) == 0) {
        printf("Телефонная книга пуста.\n");
        return 0;
    }

    show_contacts(list);

    while (1) {
        if (!read_number("Введите номер контакта: ", &number)) {
            return -1;
        }

        if (number >= 1 && (size_t)number <= phonebook_count(list)) {
            *index = (size_t)(number - 1);
            return 1;
        }

        printf("Ошибка: введите номер от 1 до %zu.\n",
               phonebook_count(list));
    }
}

static int edit_contact_data(Contact *contact) {
    int choice;

    while (1) {
        printf("\n=== Редактирование контакта ===\n");
        printf("1. Имя: %s\n", contact->first_name);
        printf("2. Фамилия: %s\n", contact->last_name);
        printf("3. Отчество: %s\n", contact->patronymic);
        printf("4. Место работы: %s\n", contact->workplace);
        printf("5. Должность: %s\n", contact->position);
        printf("6. Телефоны (%d)\n", contact->phone_count);
        printf("7. Email (%d)\n", contact->email_count);
        printf("0. Завершить редактирование\n");

        if (!read_number("Выберите поле: ", &choice)) {
            return 0;
        }

        switch (choice) {
            case 1:
                if (!read_required_text("Новое имя: ",
                                        contact->first_name,
                                        sizeof(contact->first_name))) {
                    return 0;
                }
                break;

            case 2:
                if (!read_required_text("Новая фамилия: ",
                                        contact->last_name,
                                        sizeof(contact->last_name))) {
                    return 0;
                }
                break;

            case 3:
                if (!read_text("Новое отчество: ",
                               contact->patronymic,
                               sizeof(contact->patronymic))) {
                    return 0;
                }
                break;

            case 4:
                if (!read_text("Новое место работы: ",
                               contact->workplace,
                               sizeof(contact->workplace))) {
                    return 0;
                }
                break;

            case 5:
                if (!read_text("Новая должность: ",
                               contact->position,
                               sizeof(contact->position))) {
                    return 0;
                }
                break;

            case 6: {
                int count;

                while (1) {
                    if (!read_number("Количество телефонов (0-3): ",
                                     &count)) {
                        return 0;
                    }

                    if (count >= 0 && count <= MAX_PHONES) {
                        break;
                    }

                    printf("Ошибка: допустимое количество — от 0 до %d.\n",
                           MAX_PHONES);
                }

                memset(contact->phones, 0, sizeof(contact->phones));
                contact->phone_count = count;

                for (int i = 0; i < count; i++) {
                    char prompt[TEXT_LENGTH];

                    snprintf(prompt, sizeof(prompt),
                             "Новый телефон %d: ", i + 1);

                    if (!read_required_text(prompt,
                                            contact->phones[i],
                                            sizeof(contact->phones[i]))) {
                        return 0;
                    }
                }

                break;
            }

            case 7: {
                int count;

                while (1) {
                    if (!read_number("Количество email (0-3): ", &count)) {
                        return 0;
                    }

                    if (count >= 0 && count <= MAX_EMAILS) {
                        break;
                    }

                    printf("Ошибка: допустимое количество — от 0 до %d.\n",
                           MAX_EMAILS);
                }

                memset(contact->emails, 0, sizeof(contact->emails));
                contact->email_count = count;

                for (int i = 0; i < count; i++) {
                    char prompt[TEXT_LENGTH];

                    snprintf(prompt, sizeof(prompt),
                             "Новый email %d: ", i + 1);

                    if (!read_required_text(prompt,
                                            contact->emails[i],
                                            sizeof(contact->emails[i]))) {
                        return 0;
                    }
                }

                break;
            }

            case 0:
                return 1;

            default:
                printf("Ошибка: неизвестный пункт меню.\n");
                break;
        }
    }
}

int main(void) {
    ContactList list;
    phonebook_init(&list);

    int choice;

    while (1) {
        printf("\n=== Телефонная книга ===\n");
        printf("1. Добавить контакт\n");
        printf("2. Показать контакты\n");
        printf("3. Изменить контакт\n");
        printf("4. Удалить контакт\n");
        printf("0. Выход\n");

        if (!read_number("Выберите действие: ", &choice)) {
            break;
        }

        if (choice == 0) {
            break;
        }

        switch (choice) {
            case 1: {
                Contact contact;

                if (!read_contact_data(&contact)) {
                    goto cleanup;
                }

                if (phonebook_add(&list, &contact)) {
                    printf("Контакт успешно добавлен.\n");
                } else {
                    printf("Ошибка: не удалось выделить память для контакта.\n");
                }

                break;
            }

            case 2:
                show_contacts(&list);
                break;

            case 3: {
                size_t index;
                int result = read_contact_index(&list, &index);

                if (result == -1) {
                    goto cleanup;
                }

                if (result == 0) {
                    break;
                }

                const Contact *existing_contact = phonebook_get_at(&list, index);

                if (existing_contact == NULL) {
                    printf("Ошибка: контакт не найден.\n");
                    break;
                }

                Contact updated_contact = *existing_contact;

                if (!edit_contact_data(&updated_contact)) {
                    goto cleanup;
                }

                if (phonebook_update(&list, index, &updated_contact)) {
                    printf("Контакт успешно изменён.\n");
                } else {
                    printf("Ошибка: не удалось изменить контакт.\n");
                }

                break;
            }

            case 4: {
                size_t index;
                int result = read_contact_index(&list, &index);

                if (result == -1) {
                    goto cleanup;
                }

                if (result == 0) {
                    break;
                }

                if (phonebook_remove(&list, index)) {
                    printf("Контакт успешно удалён.\n");
                } else {
                    printf("Ошибка: не удалось удалить контакт.\n");
                }

                break;
            }

            default:
                printf("Ошибка: неизвестный пункт меню.\n");
                break;
        }
    }

cleanup:
    phonebook_clear(&list);
    printf("Телефонная книга закрыта.\n");

    return 0;
}
