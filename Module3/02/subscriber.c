#include "subscriber.h"
#include "message.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int install_signal_handlers(void)
{
    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (sigaction(SIGINT, &action, NULL) == -1) {
        perror("sigaction SIGINT");
        return -1;
    }

    if (sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction SIGTERM");
        return -1;
    }

    return 0;
}

static int send_subscription_command(
    int queue_id,
    pid_t pid,
    const char *command,
    const char *topic)
{
    Message message = {0};
    message.mtype = BROKER_MESSAGE_TYPE;

    int written = snprintf(
        message.mtext,
        sizeof(message.mtext),
        "%s,%ld,%s",
        command,
        (long)pid,
        topic);

    if (written < 0 || (size_t)written >= sizeof(message.mtext)) {
        fprintf(stderr, "Subscription command is too long\n");
        return -1;
    }

    if (msgsnd(
            queue_id,
            &message,
            (size_t)written + 1,
            IPC_NOWAIT) == -1) {
        if (errno == EIDRM || errno == EINVAL) {
            fprintf(stderr, "Broker queue is no longer available\n");
        } else if (errno == EAGAIN) {
            fprintf(stderr, "Broker queue is full\n");
        } else {
            perror("msgsnd subscription command");
        }

        return -1;
    }

    return 0;
}

static void unsubscribe_all(
    int queue_id,
    pid_t pid,
    int topic_count,
    char *topics[])
{
    for (int i = 0; i < topic_count; i++) {
        if (send_subscription_command(
                queue_id,
                pid,
                "unsubscribe",
                topics[i]) == -1) {
            break;
        }
    }
}

int run_subscriber(int topic_count, char *topics[])
{
    if (topic_count <= 0 || topics == NULL) {
        fprintf(stderr, "At least one topic is required\n");
        return 1;
    }

    for (int i = 0; i < topic_count; i++) {
        if (topics[i] == NULL ||
            topics[i][0] == '\0' ||
            strlen(topics[i]) >= TOPIC_LENGTH ||
            strchr(topics[i], ',') != NULL) {
            fprintf(stderr, "Invalid topic: '%s'\n",
                    topics[i] != NULL ? topics[i] : "(null)");
            return 1;
        }
    }

    if (install_signal_handlers() == -1) {
        return 1;
    }

    int queue_id = msgget((key_t)QUEUE_KEY, 0);

    if (queue_id == -1) {
        if (errno == ENOENT) {
            fprintf(stderr, "Broker queue does not exist\n");
        } else {
            perror("msgget");
        }

        return 1;
    }

    pid_t pid = getpid();

    printf("Subscriber started. PID: %ld\n", (long)pid);

    int subscribed_count = 0;

    for (int i = 0; i < topic_count; i++) {
        if (send_subscription_command(
                queue_id,
                pid,
                "subscribe",
                topics[i]) == -1) {
            unsubscribe_all(queue_id, pid, subscribed_count, topics);
            return 1;
        }

        subscribed_count++;
        printf("Subscription requested: %s\n", topics[i]);
    }

    printf("Waiting for messages. Press Ctrl+C to stop.\n");

    while (!stop_requested) {
        Message message = {0};

        ssize_t received = msgrcv(
            queue_id,
            &message,
            sizeof(message.mtext),
            (long)pid,
            0);

        if (received == -1) {
            if (errno == EINTR) {
                if (stop_requested) {
                    break;
                }

                continue;
            }

            if (errno == EIDRM || errno == EINVAL) {
                printf("Broker queue is no longer available\n");
                break;
            }

            perror("msgrcv");
            break;
        }

        size_t end = received < (ssize_t)sizeof(message.mtext)
                         ? (size_t)received
                         : sizeof(message.mtext) - 1;

        message.mtext[end] = '\0';

        char *topic_separator = strchr(message.mtext, ',');
        if (topic_separator == NULL) {
            fprintf(stderr, "Invalid message received\n");
            continue;
        }

        *topic_separator = '\0';

        if (strcmp(message.mtext, "message") != 0) {
            fprintf(stderr, "Unknown message type received\n");
            continue;
        }

        char *topic = topic_separator + 1;
        char *payload_separator = strchr(topic, ',');

        if (payload_separator == NULL) {
            fprintf(stderr, "Invalid message format received\n");
            continue;
        }

        *payload_separator = '\0';
        char *payload = payload_separator + 1;

        printf("\n[Topic: %s] %s\n", topic, payload);
        printf("> ");
        fflush(stdout);
    }

    printf("\nSubscriber is stopping...\n");

    if (!stop_requested) {
        unsubscribe_all(queue_id, pid, subscribed_count, topics);
    } else {
        unsubscribe_all(queue_id, pid, subscribed_count, topics);
    }

    printf("Subscriber stopped.\n");

    return 0;
}
