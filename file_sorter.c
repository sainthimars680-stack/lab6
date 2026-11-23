/*
 * Лабораторна робота №6, завдання 2: утиліта сортування файлів.
 * Збірка: gcc -Wall -Wextra -o file_sorter file_sorter.c
 *
 * Використання: ./file_sorter --source=<директорія> --dest=<директорія>
 */
#define _DEFAULT_SOURCE
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LOG_FILE "sort_report.log"

static FILE *log_fp = NULL;
static char  dest_real[PATH_MAX];
static int   moved_count = 0;

/* Виводить повідомлення в консоль і в лог-файл. */
static void report(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');

    if (log_fp != NULL) {
        va_start(ap, fmt);
        vfprintf(log_fp, fmt, ap);
        va_end(ap);
        fputc('\n', log_fp);
        fflush(log_fp);
    }
}

/* Визначення категорії за розширенням. */
static const char *category(const char *name)
{
    static const char *docs[]    = {".txt", ".md", NULL};
    static const char *sources[] = {".c", ".h", ".go", NULL};
    static const char *images[]  = {".jpg", ".png", NULL};
    static const char *pdfs[]    = {".pdf", NULL};

    const char *dot = strrchr(name, '.');
    if (dot == NULL || dot == name)
        return "Other";

    struct { const char *dir; const char **exts; } map[] = {
        {"Documents", docs}, {"Sources", sources},
        {"Images", images},  {"PDFs", pdfs},
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        for (const char **e = map[i].exts; *e != NULL; e++)
            if (strcasecmp(dot, *e) == 0)
                return map[i].dir;
    return "Other";
}

/* Створює директорію з усіма проміжними (аналог mkdir -p). */
static int mkdir_p(const char *path)
{
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);

    for (char *p = tmp + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* Копіювання + видалення (на випадок різних файлових систем). */
static int copy_and_remove(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    if (in == NULL)
        return -1;
    FILE *out = fopen(dst, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    char buf[8192];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    if (ferror(in))
        ok = 0;
    fclose(in);
    if (fclose(out) != 0)
        ok = 0;

    if (!ok) {
        unlink(dst);
        return -1;
    }
    return unlink(src);
}

/* Якщо файл уже існує, додає суфікс _1, _2, ... */
static void unique_path(char *path, size_t size)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return;

    char base[PATH_MAX];
    snprintf(base, sizeof(base), "%s", path);
    char *slash = strrchr(base, '/');
    char *dot = strrchr(slash ? slash : base, '.');
    char ext[NAME_MAX] = "";
    if (dot != NULL) {
        snprintf(ext, sizeof(ext), "%s", dot);
        *dot = '\0';
    }
    for (int i = 1; i < 10000; i++) {
        snprintf(path, size, "%s_%d%s", base, i, ext);
        if (stat(path, &st) != 0)
            return;
    }
}

static void sort_file(const char *src, const char *name, const char *dest)
{
    const char *cat = category(name);
    char dir[PATH_MAX], dst[PATH_MAX + NAME_MAX + 2];

    snprintf(dir, sizeof(dir), "%s/%s", dest, cat);
    if (mkdir_p(dir) != 0) {
        report("Помилка: не вдалося створити папку %s: %s", dir, strerror(errno));
        return;
    }

    snprintf(dst, sizeof(dst), "%s/%s", dir, name);
    unique_path(dst, sizeof(dst));

    if (rename(src, dst) != 0) {
        if (errno != EXDEV || copy_and_remove(src, dst) != 0) {
            report("Помилка: не вдалося перемістити %s -> %s: %s",
                   src, dst, strerror(errno));
            return;
        }
    }
    report("Переміщено: %s -> %s", src, dst);
    moved_count++;
}

/* Рекурсивне сканування. Приховані елементи пропускаються. */
static void scan(const char *dir, const char *dest)
{
    DIR *d = opendir(dir);
    if (d == NULL) {
        report("Помилка: не вдалося відкрити %s: %s", dir, strerror(errno));
        return;
    }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;

        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);

        struct stat st;
        if (stat(full, &st) != 0) {
            report("Помилка: stat %s: %s", full, strerror(errno));
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            char real[PATH_MAX];
            /* не заходимо в цільову директорію, якщо вона всередині source */
            if (realpath(full, real) != NULL && strcmp(real, dest_real) == 0)
                continue;
            scan(full, dest);
        } else if (S_ISREG(st.st_mode)) {
            sort_file(full, e->d_name, dest);
        }
    }
    closedir(d);
}

static void usage(const char *prog)
{
    fprintf(stderr, "Використання: %s --source=<директорія> --dest=<директорія>\n",
            prog);
}

int main(int argc, char *argv[])
{
    static const struct option opts[] = {
        {"source", required_argument, NULL, 's'},
        {"dest",   required_argument, NULL, 'd'},
        {NULL, 0, NULL, 0}
    };
    char *source = NULL, *dest = NULL;
    int opt;

    while ((opt = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        if (opt == 's')
            source = optarg;
        else if (opt == 'd')
            dest = optarg;
        else {
            usage(argv[0]);
            return 1;
        }
    }
    if (source == NULL || dest == NULL) {
        usage(argv[0]);
        return 1;
    }

    /* прибираємо кінцевий '/' для охайного виводу */
    size_t len = strlen(source);
    while (len > 1 && source[len - 1] == '/')
        source[--len] = '\0';
    len = strlen(dest);
    while (len > 1 && dest[len - 1] == '/')
        dest[--len] = '\0';

    struct stat st;
    if (stat(source, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Помилка: '%s' не є директорією\n", source);
        return 1;
    }

    log_fp = fopen(LOG_FILE, "w");
    if (log_fp == NULL)
        perror("Не вдалося створити " LOG_FILE);

    report("--- Програма сортування запущена ---");

    if (mkdir_p(dest) != 0) {
        report("Помилка: не вдалося створити %s: %s", dest, strerror(errno));
        if (log_fp) fclose(log_fp);
        return 1;
    }
    if (realpath(dest, dest_real) == NULL)
        snprintf(dest_real, sizeof(dest_real), "%s", dest);

    scan(source, dest);

    report("Сортування завершено! Див. '" LOG_FILE "'.");
    if (log_fp != NULL)
        fclose(log_fp);
    return 0;
}