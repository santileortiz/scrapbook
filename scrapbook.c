/*
 * Copiright (C) 2020 Santiago León O.
 */
#ifdef __GNUC__
#define NOT_USED __attribute__ ((unused))
#else
#define NOT_USED
#endif

#include "meow_hash_x64_aesni.h"
// We don't use this but it causes a compiler warning.
static void MeowExpandSeed(meow_umm InputLen, void *Input, meow_u8 *SeedResult) NOT_USED;

#define _GNU_SOURCE // Used to enable strcasestr()
#define _XOPEN_SOURCE 700 // Required for strptime()
#include "common.h"
#include "binary_tree.c"

struct string_lst_t {
    string_t s;

    struct string_lst_t *next;
};

struct string_bucket_t {
    uint32_t count;
    struct string_lst_t *strings;

    struct string_bucket_t *next;
};

BINARY_TREE_NEW(uint_64_to_str, uint64_t, struct string_bucket_t*,  a <= b ? (a == b ? 0 : -1) : 1);

struct scrapbook_t {
    mem_pool_t pool;

    struct uint_64_to_str_tree_t hash_to_path;

    uint64_t total_size; 
};

void push_file_hash (struct scrapbook_t *app, uint64_t hash, char *path)
{
    struct string_bucket_t *bucket = uint_64_to_str_get (&app->hash_to_path, hash);
    if (bucket == NULL) {
        bucket = mem_pool_push_struct (&app->hash_to_path.pool, struct string_bucket_t);
        *bucket = ZERO_INIT (struct string_bucket_t);
        uint_64_to_str_tree_insert (&app->hash_to_path, hash, bucket);
    }

    bool found = false;
    struct string_lst_t *curr_str = bucket->strings;
    while (curr_str != NULL) {
        if (strcmp(path, str_data(&curr_str->s)) == 0) {
            found = true;
            break;
        }

        curr_str = curr_str->next;
    }

    if (!found) {
        struct string_lst_t *str = mem_pool_push_struct (&app->hash_to_path.pool, struct string_lst_t);
        *str = ZERO_INIT (struct string_lst_t);
        str_set (&str->s, path);
        LINKED_LIST_PUSH (bucket->strings, str);

        bucket->count++;
    }
}

uint64_t hash_64 (void *ptr, size_t size)
{
    meow_u128 hash = MeowHash(MeowDefaultSeed, size, ptr);
    return MeowU64From(hash, 0);
}

char* partial_file_read (mem_pool_t *pool, const char *path, uint64_t max_size, uint64_t *size_read)
{
    bool success = true;

    mem_pool_marker_t mrk;
    if (pool != NULL) {
        mrk = mem_pool_begin_temporary_memory (pool);
    }

    char *loaded_data = NULL;
    struct stat st;
    if (stat(path, &st) == 0) {
        uint64_t size_to_read = MIN(st.st_size, max_size);
        loaded_data = (char*)pom_push_size (pool, size_to_read + 1);

        int file = open (path, O_RDONLY);
        if (file != -1) {
            int bytes_read = 0;
            do {
                int status = read (file, loaded_data+bytes_read, size_to_read-bytes_read);
                if (status == -1) {
                    success = false;
                    printf ("Error reading %s: %s\n", path, strerror(errno));
                    break;
                }
                bytes_read += status;
            } while (bytes_read != size_to_read);
            loaded_data[size_to_read] = '\0';

            if (size_read != NULL) {
                *size_read = size_to_read;
            }

            close (file);
        } else {
            success = false;
            printf ("Error opening %s: %s\n", path, strerror(errno));
        }

    } else {
        success = false;
        printf ("Could not read %s: %s\n", path, strerror(errno));
    }

    char *retval = NULL;
    if (success) {
        retval = loaded_data;
    } else if (loaded_data != NULL) {
        if (pool != NULL) {
            mem_pool_end_temporary_memory (mrk);
        } else {
            free (loaded_data);
        }
    }

    return retval;
}

ITERATE_DIR_CB (find_duplicates_by_hash)
{
    struct scrapbook_t *sb = (struct scrapbook_t*) data;

    char *extension = get_extension (fname);
    if (!is_dir && extension != NULL && strncasecmp (extension, "jpg", 3) == 0) {
        mem_pool_t pool_l = {0};

        uint64_t file_len = 0;
        char *file_data = partial_file_read (&sb->pool, fname, kilobyte(1), &file_len);
        sb->total_size += file_len;

        push_file_hash (sb, hash_64 (file_data, file_len), fname);
        mem_pool_destroy (&pool_l);
    }
}

void cli_status (float val, float total)
{
    float percent = MIN((val/(total-1))*100, 100);
    int length = 60;

    char str[length+30];

    static int prev = -1;
    static int idx;

    idx = length*percent/100;
    if (prev != idx) {
        prev = idx;
        int i;
        for (i=0; i<length; i++) {
            if (i < idx) {
                str[i] = '#';
            } else {
                str[i] = '-';
            }
        }
        str[i] = '\0';
        fprintf (stderr, "\r[%s] %.2f%%", str, percent);
        if (percent == 100) {
            fprintf (stderr, "\r\e[KComplete.\n");
        }
    }
}

int main (int argc, char **argv)
{
    struct scrapbook_t scrapbook = {0};

    if (argc >= 2) {

        printf ("Looking for duplicates in:\n");
        for (int i=1; i<argc; i++) {
            char *path = abs_path_no_sh_expand (argv[i], &scrapbook.pool);
            if (dir_exists_no_sh_expand(path)) {
                printf ("  %s\n", path);
                iterate_dir (path, find_duplicates_by_hash, &scrapbook);
            }
        }

        printf ("Total size read: %lu bytes\n", scrapbook.total_size);
        printf ("\n");

        printf ("Tentative non unique files:\n");
        struct string_bucket_t *tentative_duplicates = NULL;
        uint32_t num_tentative_non_unique_files = 0;
        BINARY_TREE_FOR(uint_64_to_str, &scrapbook.hash_to_path, curr_node) {
            struct string_bucket_t *bucket = curr_node->value;
            if (bucket->count > 1) {
                struct string_lst_t *curr_path = bucket->strings;
                while (curr_path != NULL) {
                    num_tentative_non_unique_files++;
                    printf ("%s ", str_data(&curr_path->s));

                    curr_path = curr_path->next;
                }
                printf ("\n");

                LINKED_LIST_PUSH (tentative_duplicates, bucket);
            }
        }
        printf ("Tentative non unique file count: %d\n", num_tentative_non_unique_files);
        printf ("\n");

        printf ("Executing full comparison:\n");
        struct string_bucket_t *exact_duplicates = NULL;
        uint64_t exact_duplicates_len = 0;
        struct string_bucket_t *non_duplicates = NULL;
        uint64_t non_duplicates_len = 0;

        while (tentative_duplicates != NULL)
        {
            struct string_bucket_t *curr_bucket = LINKED_LIST_POP(tentative_duplicates);

            bool all_equal = true;

            struct string_lst_t *curr_str = curr_bucket->strings;
            while (all_equal && curr_str != NULL && curr_str->next != NULL) {
                mem_pool_t pool_l = {0};
                uint64_t f1_len;
                char *f1 = full_file_read_full (&pool_l, str_data(&curr_str->s), &f1_len, false);

                uint64_t f2_len;
                char *f2 = full_file_read_full (&pool_l, str_data(&curr_str->next->s), &f2_len, false);

                if (f1_len != f2_len || memcmp (f1, f2, f1_len) != 0) {
                    all_equal = false;
                }

                mem_pool_destroy (&pool_l);

                curr_str = curr_str->next;
            }

            if (all_equal) {
                exact_duplicates_len += curr_bucket->count;
                LINKED_LIST_PUSH (exact_duplicates, curr_bucket);
            } else {
                non_duplicates_len += curr_bucket->count;
                LINKED_LIST_PUSH (non_duplicates, curr_bucket);
            }

            cli_status (exact_duplicates_len+non_duplicates_len, num_tentative_non_unique_files);
        }
        printf ("\n");

        printf ("Exact duplicates: %ld\n", exact_duplicates_len);
        printf ("Non duplicates: %ld\n", non_duplicates_len);

        if (non_duplicates_len != 0) {
            // TODO: Correctly handle this case. I think one approach could be
            // to do a full file comparison between all files in the bucket,
            // then put the real duplicates in the exact_duplicates bucket list.
            printf ("  Error: HASH COLLISIONS!!\n");
        }

    } else {
        printf ("Usage:\nscrapbook <directory name>\n");
    }

    mem_pool_destroy (&scrapbook.pool);
    return 0;
}
