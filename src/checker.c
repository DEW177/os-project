#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "sandbox.h"

struct line_list {
    char **items;
    size_t count;
    size_t capacity;
};

static void free_lines(struct line_list *list)
{
    if (list == NULL) {
        return;
    }

    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }

    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void trim_line(char *line)
{
    size_t length;

    if (line == NULL) {
        return;
    }

    length = strlen(line);

    while (length > 0) {
        char c = line[length - 1];

        if (c == '\n' ||
            c == '\r' ||
            c == ' ' ||
            c == '\t') {
            length--;
        } else {
            break;
        }
    }

    line[length] = '\0';
}

static int append_line(struct line_list *list, const char *line)
{
    char *copy;

    if (list->count == list->capacity) {
        size_t new_capacity =
            list->capacity == 0 ? 32 : list->capacity * 2;

        char **new_items = realloc(
            list->items,
            new_capacity * sizeof(*new_items)
        );

        if (new_items == NULL) {
            return -1;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    copy = malloc(strlen(line) + 1);

    if (copy == NULL) {
        return -1;
    }

    strcpy(copy, line);
    list->items[list->count++] = copy;

    return 0;
}

static int read_lines(FILE *file, struct line_list *list)
{
    char *buffer = NULL;
    size_t buffer_size = 0;
    ssize_t length;

    while ((length = getline(&buffer, &buffer_size, file)) != -1) {
        (void)length;

        trim_line(buffer);

        if (append_line(list, buffer) < 0) {
            free(buffer);
            return -1;
        }
    }

    free(buffer);

    if (ferror(file)) {
        return -1;
    }

    /*
     * ไม่สนใจบรรทัดว่างท้ายไฟล์
     */
    while (list->count > 0 &&
           list->items[list->count - 1][0] == '\0') {
        free(list->items[list->count - 1]);
        list->count--;
    }

    return 0;
}

int check_output(
    const char *expected_file,
    const char *actual_file
)
{
    FILE *expected;
    FILE *actual;

    struct line_list expected_lines = {0};
    struct line_list actual_lines = {0};

    int result = 1;

    if (expected_file == NULL || actual_file == NULL) {
        return -1;
    }

    expected = fopen(expected_file, "r");

    if (expected == NULL) {
        return -1;
    }

    actual = fopen(actual_file, "r");

    if (actual == NULL) {
        fclose(expected);
        return -1;
    }

    if (read_lines(expected, &expected_lines) < 0 ||
        read_lines(actual, &actual_lines) < 0) {
        result = -1;
        goto cleanup;
    }

    if (expected_lines.count != actual_lines.count) {
        result = 0;
        goto cleanup;
    }

    for (size_t i = 0; i < expected_lines.count; i++) {
        if (strcmp(
                expected_lines.items[i],
                actual_lines.items[i]) != 0) {
            result = 0;
            goto cleanup;
        }
    }

cleanup:
    fclose(expected);
    fclose(actual);

    free_lines(&expected_lines);
    free_lines(&actual_lines);

    return result;
}