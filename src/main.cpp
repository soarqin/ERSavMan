#include "savefile.h"

#include <getopt.h>

int main(int argc, char *argv[]) {
    const option longOptions[] = {
        {"list", required_argument, nullptr, 'l'},
        {"file", required_argument, nullptr, 'f'},
        {"export", required_argument, nullptr, 'e'},
        {"import", no_argument, nullptr, 'i'},
        {"slot", required_argument, nullptr, 's'},
        {nullptr},
    };
    if (argc <= 1) {
        fprintf(stderr, "Usage: ERSavMan -f <filename> [-l | -e <export filename>] | -i <import filename> [-s <slot id>]\n");
        return 0;
    }
    char opt;
    int mode = 0;
    int slot = -1;
    std::string filename;
    std::string filename2;
    while ((opt = getopt_long(argc, argv, ":f:e:i:s:l", longOptions, nullptr)) != -1) {
        switch (opt) {
        case ':':
            fprintf(stderr, "mssing argument for %c\n", optopt);
            return -1;
        case '?':
            fprintf(stderr, "bad arument: %c\n", optopt);
            return -1;
        case 'l':
            mode = 1;
            break;
        case 'f':
            filename = optarg;
            break;
        case 'e':
            if (mode != 0) {
                fprintf(stderr, "-e cannot be used with other action options\n");
                return -1;
            }
            filename2 = optarg;
            mode = 2;
            break;
        case 'i':
            if (mode != 0) {
                fprintf(stderr, "-i cannot be used with other action options\n");
                return -1;
            }
            filename2 = optarg;
            mode = 3;
            break;
        case 's':
            slot = (int)strtol(optarg, nullptr, 0);
            break;
        default:
            break;
        }
    }
    SaveFile save(filename);
    switch (mode) {
    case 2:
        save.exportToFile(filename2, slot);
        break;
    case 3:
        save.importFromFile(filename2, slot);
        break;
    default:
        save.listSlots(slot);
        break;
    }
    return 0;
}
