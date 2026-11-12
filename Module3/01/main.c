#include <stdio.h>
#include <string.h>

#include "ipc.h"

int main(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s [-p fifo_name] file1 [file2 ...]\n",
                argv[0]);
        return 1;
    }

    const char *fifo_path = NULL;
    int first_file = 1;

    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4) {
            fprintf(stderr,
                    "Usage: %s -p fifo_name file1 [file2 ...]\n",
                    argv[0]);
            return 1;
        }

        fifo_path = argv[2];
        first_file = 3;
    }

    if (run_file_copy(argc - first_file, &argv[first_file], fifo_path) == -1) {
        fprintf(stderr, "File copying failed\n");
        return 1;
    }

    return 0;
}