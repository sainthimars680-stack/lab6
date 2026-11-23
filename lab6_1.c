/*
 * Лабораторна робота №6, завдання 1:
 * створення ієрархії файлів/папок та аналог команди tree.
 * Збірка: gcc -Wall -Wextra -o lab6_1 lab6_1.c
 *
 * Використання:
 *   ./lab6_1          створює lab6_<номер>/ і виводить дерево
 *   ./lab6_1 <шлях>   лише виводить дерево для вказаної директорії
 */
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#define JOURNAL_NO "16" /* <-- ваш номер за журналом */
#define ROOT_DIR   "lab6_" JOURNAL_NO

typedef struct {
    char name[NAME_MAX + 1];
    int  is_dir;
} Entry;

/* ---------- створення структури ---------- */

static void make_dir(const char *path)
{
    if (mkdir(path, 0755) != 0 && errno != EEXIST)
        fprintf(stderr, "mkdir %s: %s\n", path, strerror(errno));
}

static void make_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "fopen %s: %s\n", path, strerror(errno));
        return;
    }
    fputs(content, f);
    fclose(f);
}

static void create_structure(void)
{
    make_dir(ROOT_DIR);
    make_dir(ROOT_DIR "/.git");
    make_file(ROOT_DIR "/.git/config", "[core]\n\trepositoryformatversion = 0\n");
    make_dir(ROOT_DIR "/src");
    make_file(ROOT_DIR "/src/main.c",
              "#include <stdio.h>\n\nint main(void)\n{\n"
              "    printf(\"Hello, world!\\n\");\n    return 0;\n}\n");
    make_dir(ROOT_DIR "/bin");
    make_dir(ROOT_DIR "/docs");
    make_file(ROOT_DIR "/docs/manual.txt", "Керівництво користувача\n");
    make_file(ROOT_DIR "/docs/presentation.pdf", "%PDF-1.4\n% demo file\n");
    make_dir(ROOT_DIR "/docs/img");
    make_file(ROOT_DIR "/docs/img/cat.jpg", "JPEG demo data\n");
    make_file(ROOT_DIR "/docs/img/dog.png", "PNG demo data\n");
    make_file(ROOT_DIR "/README.md", "# lab6\n\nДемонстраційний проєкт.\n");
}

/* ---------- аналог tree ---------- */

static int cmp_entries(const void *a, const void *b)
{
    return strcasecmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

/*
 * Рекурсивний обхід: opendir/readdir/closedir + stat.
 * Приховані елементи (назва починається з '.') пропускаються.
 */
static void tree(const char *path, const char *prefix, int *dirs, int *files)
{
    DIR *d = opendir(path);
    if (d == NULL) {
        fprintf(stderr, "opendir %s: %s\n", path, strerror(errno));
        return;
    }

    Entry *list = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;

    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') /* ".", "..", приховані файли й папки */
            continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);

        struct stat st;
        if (stat(full, &st) != 0) {
            fprintf(stderr, "stat %s: %s\n", full, strerror(errno));
            continue;
        }

        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            list = realloc(list, cap * sizeof(Entry));
            if (list == NULL) {
                perror("realloc");
                closedir(d);
                return;
            }
        }
        snprintf(list[n].name, sizeof(list[n].name), "%s", e->d_name);
        list[n].is_dir = S_ISDIR(st.st_mode);
        n++;
    }
    closedir(d);

    qsort(list, n, sizeof(Entry), cmp_entries);

    for (size_t i = 0; i < n; i++) {
        int last = (i == n - 1);
        printf("%s%s%s\n", prefix, last ? "└── " : "├── ", list[i].name);

        if (list[i].is_dir) {
            (*dirs)++;
            char full[PATH_MAX], next[1024];
            snprintf(full, sizeof(full), "%s/%s", path, list[i].name);
            snprintf(next, sizeof(next), "%s%s", prefix, last ? "    " : "│   ");
            tree(full, next, dirs, files);
        } else {
            (*files)++;
        }
    }
    free(list);
}

int main(int argc, char *argv[])
{
    const char *root = ROOT_DIR;

    if (argc > 1) {
        root = argv[1];
    } else {
        create_structure();
    }

    int dirs = 0, files = 0;
    printf("%s\n", root);
    tree(root, "", &dirs, &files);
    printf("\n%d directories, %d files\n", dirs, files);
    return 0;
}