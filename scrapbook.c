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

struct scrapbook_t {
    mem_pool_t pool;

    uint64_t total_size; 
};

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

        printf ("%lu\n", hash_64 (file_data, file_len));

        mem_pool_destroy (&pool_l);
    }
}

int main (int argc, char **argv)
{
    struct scrapbook_t scrapbook = {0};

    if (argc == 2) {
        char *path = abs_path (argv[1], &scrapbook.pool);

        printf ("Looking for duplicates in %s\n", path);

        iterate_dir (path, find_duplicates_by_hash, &scrapbook);

        printf ("Total size read: %lu bytes\n", scrapbook.total_size);
    } else {
        printf ("Usage:\nscrapbook <directory name>\n");
    }

    mem_pool_destroy (&scrapbook.pool);
    return 0;
}
