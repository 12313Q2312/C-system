#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#define TITLE_LENGTH 120

typedef struct {
    int id;
    char title[TITLE_LENGTH + 1];
} Record;

typedef struct {
    Record *items;
    size_t count;
    size_t capacity;
} Records;

static void usage(FILE *output) {
    fputs("Usage: c-system <data.tsv> <list|get|add|update|remove> [id] [title]\n"
          "IDs are positive integers. Quote titles that contain spaces.\n", output);
}

static int parse_id(const char *text) {
    if (!text[0] || strspn(text, "0123456789") != strlen(text)) {
        return 0;
    }
    errno = 0;
    long value = strtol(text, NULL, 10);
    if (errno || value < 1 || value > INT_MAX) {
        return 0;
    }
    return (int)value;
}

static int valid_title(const char *title) {
    if (strlen(title) > TITLE_LENGTH || strspn(title, " ") == strlen(title)) {
        return 0;
    }
    for (const unsigned char *ch = (const unsigned char *)title; *ch; ++ch) {
        if (*ch < 32 || *ch == 127) {
            return 0;
        }
    }
    return 1;
}

static size_t find_record(const Records *records, int id) {
    size_t index = 0;
    while (index < records->count && records->items[index].id != id) {
        ++index;
    }
    return index;
}

static int append_record(Records *records, int id, const char *title) {
    if (records->count == records->capacity) {
        size_t capacity = records->capacity ? records->capacity * 2 : 16;
        Record *items = realloc(records->items, capacity * sizeof(*items));
        if (!items) {
            return 0;
        }
        records->items = items;
        records->capacity = capacity;
    }
    Record *record = &records->items[records->count++];
    record->id = id;
    strcpy(record->title, title);
    return 1;
}

static int load_records(const char *path, Records *records) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        return errno == ENOENT;
    }
    char line[TITLE_LENGTH + 24];
    int ok = 1;
    while (fgets(line, sizeof(line), input)) {
        char *newline = strchr(line, '\n');
        char *title = strchr(line, '\t');
        if (!newline || !title || title >= newline) {
            ok = 0;
            break;
        }
        if (newline > line && newline[-1] == '\r') {
            --newline;
        }
        *newline = '\0';
        *title++ = '\0';
        int id = parse_id(line);
        if (!id || !valid_title(title) || find_record(records, id) < records->count ||
            !append_record(records, id, title)) {
            ok = 0;
            break;
        }
    }
    ok = ok && !ferror(input);
    fclose(input);
    return ok;
}

static int print_record(FILE *output, const Record *record) {
    return fprintf(output, "%d\t%s\n", record->id, record->title) >= 0;
}

static int replace_data(const char *path, const char *temporary) {
#ifdef _WIN32
    return MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, path) == 0;
#endif
}

static FILE *open_temporary(const char *path) {
#ifdef _WIN32
    int handle = _sopen(path, _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                        _SH_DENYRW, _S_IREAD | _S_IWRITE);
    if (handle < 0) {
        return NULL;
    }
    FILE *output = _fdopen(handle, "wb");
    if (!output) {
        _close(handle);
        remove(path);
    }
    return output;
#else
    return fopen(path, "wbx");
#endif
}

static int edit(Records *records, const char *command, int id, const char *title) {
    size_t index = find_record(records, id);
    if (!strcmp(command, "add")) {
        if (index < records->count) {
            fputs("Record ID already exists.\n", stderr);
            return 0;
        }
        if (!append_record(records, id, title)) {
            fputs("Could not allocate a record.\n", stderr);
            return 0;
        }
    } else if (index == records->count) {
        fputs("Record ID not found.\n", stderr);
        return 0;
    } else if (!strcmp(command, "update")) {
        strcpy(records->items[index].title, title);
    } else {
        --records->count;
        memmove(records->items + index, records->items + index + 1,
                (records->count - index) * sizeof(*records->items));
    }
    return 1;
}

static int edit_file(const char *path, const char *command, int id, const char *title) {
    char *temporary = malloc(strlen(path) + sizeof(".tmp"));
    if (!temporary) {
        fputs("Could not allocate a file name.\n", stderr);
        return 1;
    }
    sprintf(temporary, "%s.tmp", path);
    FILE *output = open_temporary(temporary);
    if (!output) {
        fputs("Could not start the update. Check the data directory and any existing .tmp file.\n", stderr);
        free(temporary);
        return 1;
    }

    Records records = {0};
    int ok = load_records(path, &records);
    if (!ok) {
        fputs("Could not read a valid data file.\n", stderr);
    } else {
        ok = edit(&records, command, id, title);
    }
    for (size_t index = 0; ok && index < records.count; ++index) {
        ok = print_record(output, &records.items[index]);
        if (!ok) {
            fputs("Could not write the updated data.\n", stderr);
        }
    }
    if (fclose(output)) {
        fputs("Could not finish writing the updated data.\n", stderr);
        ok = 0;
    }
    if (ok && !replace_data(path, temporary)) {
        fputs("Could not replace the data file.\n", stderr);
        ok = 0;
    }
    if (!ok) {
        remove(temporary);
    }
    free(records.items);
    free(temporary);
    return !ok;
}

static int read_file(const char *path, int id) {
    Records records = {0};
    int ok = load_records(path, &records);
    if (!ok) {
        fputs("Could not read a valid data file.\n", stderr);
    } else if (id) {
        size_t index = find_record(&records, id);
        if (index == records.count) {
            fputs("Record ID not found.\n", stderr);
            ok = 0;
        } else {
            ok = print_record(stdout, &records.items[index]);
        }
    } else {
        for (size_t index = 0; ok && index < records.count; ++index) {
            ok = print_record(stdout, &records.items[index]);
        }
    }
    free(records.items);
    return !ok;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        usage(stdout);
        return 0;
    }
    if (argc < 3 || !argv[1][0]) {
        usage(stderr);
        return 2;
    }
    const char *path = argv[1];
    const char *command = argv[2];
    if (!strcmp(command, "list") && argc == 3) {
        return read_file(path, 0);
    }
    int id = argc >= 4 ? parse_id(argv[3]) : 0;
    if (id && argc == 4) {
        if (!strcmp(command, "get")) {
            return read_file(path, id);
        }
        if (!strcmp(command, "remove")) {
            return edit_file(path, command, id, NULL);
        }
    }
    if (id && argc == 5 && valid_title(argv[4]) &&
        (!strcmp(command, "add") || !strcmp(command, "update"))) {
        return edit_file(path, command, id, argv[4]);
    }
    usage(stderr);
    return 2;
}
