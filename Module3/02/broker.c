#include "broker.h"
#include "message.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

typedef struct Subscription {
    pid_t pid;
    char topic[TOPIC_LENGTH];
    struct Subscription *next;
} Subscription;

typedef struct Publisher {
    pid_t pid;
    struct Publisher *next;
} Publisher;

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int parse_pid(const char *text, pid_t *pid)
{
    char *end = NULL;

    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value <= 0 || (long)(pid_t)value != value) {
        return -1;
    }

    *pid = (pid_t)value;
    return 0;
}

static int add_publisher(Publisher **head, pid_t pid)
{
    for (Publisher *current = *head;
         current != NULL;
         current = current->next) {
        if (current->pid == pid) {
            return 0;
        }
    }

    Publisher *publisher = malloc(sizeof(*publisher));

    if (publisher == NULL) {
        return -1;
    }

    publisher->pid = pid;
    publisher->next = *head;
    *head = publisher;

    return 0;
}

static int add_subscription(
    Subscription **head,
    pid_t pid,
    const char *topic)
{
    for (Subscription *current = *head;
         current != NULL;
         current = current->next) {
        if (current->pid == pid &&
            strcmp(current->topic, topic) == 0) {
            return 0;
        }
    }

    Subscription *subscription = malloc(sizeof(*subscription));

    if (subscription == NULL) {
        return -1;
    }

    subscription->pid = pid;
    snprintf(subscription->topic,
             sizeof(subscription->topic),
             "%s",
             topic);

    subscription->next = *head;
    *head = subscription;

    return 0;
}

static void remove_subscription(
    Subscription **head,
    pid_t pid,
    const char *topic)
{
    Subscription **current = head;

    while (*current != NULL) {
        if ((*current)->pid == pid &&
            strcmp((*current)->topic, topic) == 0) {
            Subscription *removed = *current;
            *current = removed->next;
            free(removed);
            return;
        }

        current = &(*current)->next;
    }
}

static void free_subscriptions(Subscription *head)
{
    while (head != NULL) {
        Subscription *next = head->next;
        free(head);
        head = next;
    }
}

static void free_publishers(Publisher *head)
{
    while (head != NULL) {
        Publisher *next = head->next;
        free(head);
        head = next;
    }
}

static void notify_participants(
    Publisher *publishers,
    Subscription *subscriptions)
{
    for (Publisher *current = publishers;
         current != NULL;
         current = current->next) {
        if (kill(current->pid, SIGINT) == -1 && errno != ESRCH) {
            perror("kill publisher");
        }
    }

    for (Subscription *current = subscriptions;
         current != NULL;
         current = current->next) {
        int already_notified = 0;

        for (Subscription *previous = subscriptions;
             previous != current;
             previous = previous->next) {
            if (previous->pid == current->pid) {
                already_notified = 1;
                break;
            }
        }

        if (!already_notified &&
            kill(current->pid, SIGINT) == -1 &&
            errno != ESRCH) {
            perror("kill subscriber");
        }
    }
}

static int send_to_subscriber(
    int queue_id,
    pid_t pid,
    const char *topic,
    const char *payload)
{
    Message outgoing = {0};
    outgoing.mtype = (long)pid;

    int length = snprintf(
        outgoing.mtext,
        sizeof(outgoing.mtext),
        "message,%s,%s",
        topic,
        payload);

    if (length < 0 || (size_t)length >= sizeof(outgoing.mtext)) {
        fprintf(stderr, "Message for PID %ld is too long\n", (long)pid);
        return -1;
    }

    if (msgsnd(queue_id, &outgoing, (size_t)length + 1, IPC_NOWAIT) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            stop_requested = 1;
        } else if (errno == EAGAIN) {
            fprintf(stderr,
                    "Queue is full; message for PID %ld was not sent\n",
                    (long)pid);
        } else {
            perror("msgsnd to subscriber");
        }

        return -1;
    }

    return 0;
}

static void publish_message(
    int queue_id,
    Subscription *subscriptions,
    pid_t publisher_pid,
    const char *topic,
    const char *payload)
{
    (void)publisher_pid;

    for (Subscription *current = subscriptions;
         current != NULL && !stop_requested;
         current = current->next) {
        if (strcmp(current->topic, topic) == 0) {
            send_to_subscriber(
                queue_id,
                current->pid,
                topic,
                payload);
        }
    }
}

static void process_message(
    int queue_id,
    Message *message,
    ssize_t received,
    Publisher **publishers,
    Subscription **subscriptions)
{
    if (received == 0) {
        return;
    }

    message->mtext[received < MESSAGE_SIZE
                       ? (size_t)received
                       : MESSAGE_SIZE - 1] = '\0';

    char text[MESSAGE_SIZE];
    snprintf(text, sizeof(text), "%s", message->mtext);

    char *command_end = strchr(text, ',');
    if (command_end == NULL) {
        fprintf(stderr, "Invalid message format\n");
        return;
    }

    *command_end = '\0';
    char *pid_text = command_end + 1;

    char *pid_end = strchr(pid_text, ',');
    if (pid_end == NULL) {
        fprintf(stderr, "Invalid message format\n");
        return;
    }

    *pid_end = '\0';
    char *topic = pid_end + 1;

    pid_t pid;
    if (parse_pid(pid_text, &pid) == -1) {
        fprintf(stderr, "Invalid PID in message\n");
        return;
    }

    if (strcmp(text, "send") == 0) {
        char *payload = strchr(topic, ',');

        if (payload == NULL || topic[0] == '\0') {
            fprintf(stderr, "Invalid publish message\n");
            return;
        }

        *payload = '\0';
        payload++;

        if (strlen(topic) >= TOPIC_LENGTH) {
            fprintf(stderr, "Topic is too long\n");
            return;
        }

        if (add_publisher(publishers, pid) == -1) {
            perror("malloc publisher");
        }

        publish_message(
            queue_id,
            *subscriptions,
            pid,
            topic,
            payload);

    } else if (strcmp(text, "subscribe") == 0 ||
               strcmp(text, "unsubscribe") == 0) {
        if (topic[0] == '\0' || strchr(topic, ',') != NULL ||
            strlen(topic) >= TOPIC_LENGTH) {
            fprintf(stderr, "Invalid topic\n");
            return;
        }

        if (strcmp(text, "subscribe") == 0) {
            if (add_subscription(subscriptions, pid, topic) == -1) {
                perror("malloc subscription");
            } else {
                printf("PID %ld subscribed to '%s'\n",
                       (long)pid, topic);
            }
        } else {
            remove_subscription(subscriptions, pid, topic);
            printf("PID %ld unsubscribed from '%s'\n",
                   (long)pid, topic);
        }

    } else {
        fprintf(stderr, "Unknown command: %s\n", text);
    }
}

int run_broker(void)
{
    int queue_id = msgget(
        (key_t)QUEUE_KEY,
        IPC_CREAT | IPC_EXCL | 0600);

    if (queue_id == -1) {
        if (errno == EEXIST) {
            fprintf(stderr,
                    "Broker queue already exists. "
                    "Another broker may be running.\n");
        } else {
            perror("msgget");
        }

        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        msgctl(queue_id, IPC_RMID, NULL);
        return 1;
    }

    Subscription *subscriptions = NULL;
    Publisher *publishers = NULL;

    printf("Broker started. Queue ID: %d\n", queue_id);
    printf("Press Ctrl+C to stop the broker.\n");

    while (!stop_requested) {
        Message message = {0};

        ssize_t received = msgrcv(
            queue_id,
            &message,
            sizeof(message.mtext),
            BROKER_MESSAGE_TYPE,
            0);

        if (received == -1) {
            if (errno == EINTR && stop_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Message queue is unavailable\n");
                break;
            }

            perror("msgrcv");
            break;
        }

        process_message(
            queue_id,
            &message,
            received,
            &publishers,
            &subscriptions);
    }

    printf("Broker is shutting down...\n");

    notify_participants(publishers, subscriptions);

    if (msgctl(queue_id, IPC_RMID, NULL) == -1 &&
        errno != EIDRM && errno != EINVAL) {
        perror("msgctl IPC_RMID");
    }

    free_subscriptions(subscriptions);
    free_publishers(publishers);

    printf("Broker stopped.\n");

    return 0;
}
