#include "broker.h"
#include "publisher.h"
#include "subscriber.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *program_name)
{
    printf("Usage:\n");
    printf("  %s -b\n", program_name);
    printf("  %s -p <topic>\n", program_name);
    printf("  %s -s <topic> [topic ...]\n", program_name);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        if (argc != 2) {
            print_usage(argv[0]);
            return 1;
        }

        return run_broker();
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        return run_publisher(argv[2]);
    }

    if (strcmp(argv[1], "-s") == 0) {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }

        return run_subscriber(argc - 2, &argv[2]);
    }

    fprintf(stderr, "Unknown mode: %s\n", argv[1]);
    print_usage(argv[0]);

    return 1;
}
