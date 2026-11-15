#include "publisher.h"
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

int run_publisher(const char *topic)
{
    if (topic == NULL || topic[0] == '\0' ||
        strlen(topic) >= TOPIC_LENGTH ||
        strchr(topic, ',') != NULL) {
        fprintf(stderr,
                "Invalid topic: it must be non-empty, shorter than %d "
                "characters and contain no commas\n",
                TOPIC_LENGTH);
        return 1;
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

    printf("Publisher started. PID: %ld, topic: %s\n",
           (long)pid, topic);
    printf("Enter message text, or press Ctrl+D to finish.\n");

    char payload[MESSAGE_SIZE];

    while (!stop_requested) {
        printf("> ");
        fflush(stdout);

        if (fgets(payload, sizeof(payload), stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            break;
        }

        size_t length = strlen(payload);

        if (length > 0 && payload[length - 1] == '\n') {
            payload[length - 1] = '\0';
        } else if (length == sizeof(payload) - 1) {
            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF) {
                
            }

            fprintf(stderr, "Message is too long; skipped\n");
            continue;
        }

        Message message = {0};
        message.mtype = BROKER_MESSAGE_TYPE;

        int written = snprintf(
            message.mtext,
            sizeof(message.mtext),
            "send,%ld,%s,%s",
            (long)pid,
            topic,
            payload);

        if (written < 0 || (size_t)written >= sizeof(message.mtext)) {
            fprintf(stderr, "Message is too long; skipped\n");
            continue;
        }

        if (msgsnd(queue_id,
                   &message,
                   (size_t)written + 1,
                   0) == -1) {
            if (errno == EINTR && stop_requested) {
                break;
            }

            if (errno == EIDRM || errno == EINVAL) {
                fprintf(stderr, "Broker queue is no longer available\n");
                break;
            }

            perror("msgsnd");
            continue;
        }

        printf("Message sent to broker\n");
    }

    printf("Publisher stopped.\n");
    return 0;
}
