#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "phonebook.h"

static int read_text(const char *prompt, char *buffer, size_t size) {
    printf("%s", prompt);

    if (fgets(buffer, (int)size, stdin) == NULL) {
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

static int read_number(const char *prompt, long *number) {
    char buffer[TEXT_LENGTH];

    while (1) {
        if (!read_text(prompt, buffer, sizeof(buffer))) {
            return 0;
        }

        char *end = NULL;
        long value = strtol(buffer, &end, 10);

        if (buffer == end || *end != '\0') {
            printf("Ошибка: введите целое число.\n");
            continue;
        }

        *number = value;
        return 1;
    }
}

static int read_required_text(const char *prompt, char *buffer, size_t size) {
    while (1) {
        if (!read_text(prompt, buffer, size)) {
            return 0;
        }

        if (buffer[0] != '\0') {
            return 1;
        }

        printf("Поле обязательно для заполнения.\n");
    }
}

static int read_contact_data(Contact *contact) {
    if (!read_required_text("Имя: ", contact->first_name, sizeof(contact->first_name)) ||
        !read_required_text("Фамилия: ", contact->last_name, sizeof(contact->last_name)) ||
        !read_text("Отчество: ", contact->patronymic, TEXT_LENGTH) ||
        !read_text("Место работы: ", contact->workplace, TEXT_LENGTH) ||
        !read_text("Должность: ", contact->position, TEXT_LENGTH)) {
        return 0;
    }

    long count;

    while (1) {
        if (!read_number("Количество телефонов (0-3): ", &count)) {
            return 0;
        }

        if (count >= 0 && count <= MAX_PHONES) {
            break;
        }

        printf("Ошибка: допустимо от 0 до %d телефонов.\n",
               MAX_PHONES);
    }

    contact->phone_count = (int)count;

    for (int i = 0; i < contact->phone_count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Телефон %d: ", i + 1);

        if (!read_text(prompt, contact->phones[i], TEXT_LENGTH)) {
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

        printf("Ошибка: допустимо от 0 до %d email.\n",
               MAX_EMAILS);
    }

    contact->email_count = (int)count;

    for (int i = 0; i < contact->email_count; i++) {
        char prompt[50];

        snprintf(prompt, sizeof(prompt), "Email %d: ", i + 1);

        if (!read_text(prompt, contact->emails[i], TEXT_LENGTH)) {
            return 0;
        }
    }

    return 1;
}

static void print_contact(const Contact *contact, size_t index) {
    printf("\n--- Контакт №%zu ---\n", index + 1);
    printf("Фамилия: %s\n", contact->last_name);
    printf("Имя: %s\n", contact->first_name);
    printf("Отчество: %s\n", contact->patronymic);
    printf("Место работы: %s\n", contact->workplace);
    printf("Должность: %s\n", contact->position);

    printf("Телефоны:\n");

    if (contact->phone_count == 0) {
        printf("  Нет\n");
    }

    for (int i = 0; i < contact->phone_count; i++) {
        printf("  %s\n", contact->phones[i]);
    }

    printf("Email:\n");

    if (contact->email_count == 0) {
        printf("  Нет\n");
    }

    for (int i = 0; i < contact->email_count; i++) {
        printf("  %s\n", contact->emails[i]);
    }
}

static void show_contacts(const ContactTree *tree) {
    size_t count = phonebook_count(tree);

    if (count == 0) {
        printf("Телефонная книга пуста.\n");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        const Contact *contact = phonebook_get_at(tree, i);

        if (contact != NULL) {
            print_contact(contact, i);
        }
    }
}

static void print_tree_recursive(const ContactNode *node, int depth, const char *branch) {
    if (node == NULL) {
        return;
    }

    print_tree_recursive(node->right, depth + 1, "R");

    printf("%*s%s: %s\n",
           depth * 4, "",
           branch,
           node->contact.last_name);

    print_tree_recursive(node->left, depth + 1, "L");
}

static void show_tree(const ContactTree *tree) {
    if (tree->root == NULL) {
        printf("Дерево пустое.\n");
        return;
    }

    printf("\n=== Структура AVL-дерева ===\n");

    print_tree_recursive(tree->root, 0, "#");
    printf("\n");
}

static int read_contact_index(const ContactTree *tree, size_t *index) {
    size_t count = phonebook_count(tree);

    if (count == 0) {
        printf("Телефонная книга пуста.\n");
        return 0;
    }

    long number;

    while (1) {
        if (!read_number("Введите номер контакта: ", &number)) {
            return -1;
        }

        if (number >= 1 && (size_t)number <= count) {
            *index = (size_t)(number - 1);
            return 1;
        }

        printf("Ошибка: введите номер от 1 до %zu.\n", count);
    }
}

static int edit_contact_fields(Contact *contact) {
    printf("\nЧто хотите изменить?\n");
    printf("1. Имя\n");
    printf("2. Фамилию\n");
    printf("3. Отчество\n");
    printf("4. Место работы\n");
    printf("5. Должность\n");
    printf("6. Телефоны\n");
    printf("7. Email\n");
    printf("0. Отмена\n");

    long choice;

    if (!read_number("Ваш выбор: ", &choice)) {
        return 0;
    }

    switch (choice) {
        case 1:
            return read_required_text("Новое имя: ",
                             contact->first_name, sizeof(contact->first_name));

        case 2:
            return read_required_text("Новая фамилия: ",
                             contact->last_name, sizeof(contact->last_name));

        case 3:
            return read_text("Новое отчество: ",
                             contact->patronymic, TEXT_LENGTH);

        case 4:
            return read_text("Новое место работы: ",
                             contact->workplace, TEXT_LENGTH);

        case 5:
            return read_text("Новая должность: ",
                             contact->position, TEXT_LENGTH);

        case 6: {
            long count;

            while (1) {
                if (!read_number("Количество телефонов (0-3): ",
                                 &count)) {
                    return 0;
                }

                if (count >= 0 && count <= MAX_PHONES) {
                    break;
                }

                printf("Ошибка: допустимо от 0 до %d телефонов.\n",
                       MAX_PHONES);
            }

            contact->phone_count = (int)count;

            for (int i = 0; i < contact->phone_count; i++) {
                char prompt[50];

                snprintf(prompt, sizeof(prompt),
                         "Телефон %d: ", i + 1);

                if (!read_text(prompt, contact->phones[i],
                               TEXT_LENGTH)) {
                    return 0;
                }
            }

            return 1;
        }

        case 7: {
            long count;

            while (1) {
                if (!read_number("Количество email (0-3): ",
                                 &count)) {
                    return 0;
                }

                if (count >= 0 && count <= MAX_EMAILS) {
                    break;
                }

                printf("Ошибка: допустимо от 0 до %d email.\n",
                       MAX_EMAILS);
            }

            contact->email_count = (int)count;

            for (int i = 0; i < contact->email_count; i++) {
                char prompt[50];

                snprintf(prompt, sizeof(prompt),
                         "Email %d: ", i + 1);

                if (!read_text(prompt, contact->emails[i],
                               TEXT_LENGTH)) {
                    return 0;
                }
            }

            return 1;
        }

        case 0:
            printf("Редактирование отменено.\n");
            return 2;

        default:
            printf("Ошибка: неизвестный пункт меню.\n");
            return 2;
    }
}

static void search_contacts_by_prefix(const ContactTree *tree) {
    char prefix[TEXT_LENGTH];

    if (!read_text("Введите начало фамилии: ",
                   prefix, sizeof(prefix))) {
        return;
    }

    if (prefix[0] == '\0') {
        printf("Введите хотя бы один символ.\n");
        return;
    }

    size_t capacity = phonebook_count(tree);

    if (capacity == 0) {
        printf("Телефонная книга пуста.\n");
        return;
    }

    size_t *indices = malloc(capacity * sizeof(*indices));

    if (indices == NULL) {
        printf("Не удалось выделить память для поиска.\n");
        return;
    }

    size_t found = phonebook_find_by_last_name_prefix(
        tree, prefix, indices, capacity
    );

    if (found == 0) {
        printf("Совпадений не найдено.\n");
    } else {
        printf("\nНайденные контакты:\n");

        for (size_t i = 0; i < found; i++) {
            const Contact *contact =
                phonebook_get_at(tree, indices[i]);

            if (contact != NULL) {
                printf("№ %zu — %s, %s\n",
                       indices[i] + 1,
                       contact->last_name,
                       contact->first_name);
            }
        }
    }

    free(indices);
}

int main(void) {
    ContactTree tree;
    phonebook_init(&tree);

    int running = 1;

    while (running) {
        printf("\n=== Телефонная книга (AVL-дерево) ===\n");
        printf("1. Добавить контакт\n");
        printf("2. Показать контакты\n");
        printf("3. Редактировать контакт\n");
        printf("4. Удалить контакт\n");
        printf("5. Показать структуру дерева\n");
        printf("6. Найти контакты по началу фамилии\n");
        printf("0. Выход\n");

        long choice;

        if (!read_number("Ваш выбор: ", &choice)) {
            break;
        }

        switch (choice) {
            case 1: {
                Contact contact = {0};

                if (!read_contact_data(&contact)) {
                    running = 0;
                    break;
                }

                if (phonebook_add(&tree, &contact)) {
                    printf("Контакт добавлен.\n");
                } else {
                    printf("Ошибка: не удалось добавить контакт.\n");
                }

                break;
            }

            case 2:
                show_contacts(&tree);
                break;

            case 3: {
                size_t index;
                int result = read_contact_index(&tree, &index);

                if (result == -1) {
                    running = 0;
                    break;
                }

                if (result == 0) {
                    break;
                }

                const Contact *original = phonebook_get_at(&tree, index);

                if (original == NULL) {
                    printf("Ошибка: контакт не найден.\n");
                    break;
                }

                Contact updated_contact = *original;

                int edit_result = edit_contact_fields(&updated_contact);

                if (edit_result == 0) {
                    running = 0;
                    break;
                }

                if (edit_result == 2) {
                    break;
                }

                if (phonebook_update(&tree, index, &updated_contact)) {
                    printf("Контакт обновлён.\n");
                } else {
                    printf("Ошибка: не удалось обновить контакт.\n");
                }

                break;
            }

            case 4: {
                size_t index;
                int result = read_contact_index(&tree, &index);

                if (result == -1) {
                    running = 0;
                    break;
                }

                if (result == 0) {
                    break;
                }

                if (phonebook_remove(&tree, index)) {
                    printf("Контакт удалён.\n");
                } else {
                    printf("Ошибка: не удалось удалить контакт.\n");
                }

                break;
            }

            case 5:
                show_tree(&tree);
                break;

            case 6:
                search_contacts_by_prefix(&tree);
                break;

            case 0:
                running = 0;
                break;

            default:
                printf("Ошибка: неизвестный пункт меню.\n");
                break;
        }
    }

    phonebook_clear(&tree);

    printf("Телефонная книга закрыта.\n");

    return 0;
}