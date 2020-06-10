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
#include "scanner.c"
#include "cli_parser.c"

#include "jpg_utils.c"

// TODO: Move these into common.h? they seem quite useful.
void cli_progress_bar (float val, float total)
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

// TODO: Make these prinf-like
// :make_print_like
void cli_status (char *message, float val)
{
    fprintf (stderr, "\r%s%.2f", message, val);
}

// :make_print_like
void cli_status_end ()
{
    fprintf (stderr, "\r\e[KComplete.\n");
}

struct string_lst_t {
    string_t s;

    struct string_lst_t *next;
};

void file_name_compute_relevance_characteristics (char *fname, bool *has_copy_parenthesis, uint64_t *space_cnt)
{
    assert (has_copy_parenthesis != NULL && space_cnt != NULL);

    struct scanner_t _scnr = {0};
    struct scanner_t *scnr = &_scnr;
    scnr->pos = fname;

    *space_cnt = 0;
    *has_copy_parenthesis = false;
    while (!scnr->is_eof) {
        if (scanner_char (scnr, ' ')) {
            (*space_cnt)++;
        } else {
            int i;
            char *pos_backup = scnr->pos;
            if (scanner_char (scnr, '(') &&
                scanner_int (scnr, &i) &&
                scanner_str (scnr, ").") &&
                (scanner_strcase (scnr, "jpg") || scanner_strcase (scnr, "jpeg"))) {
                *has_copy_parenthesis = true;

            } else {
                scnr->pos = pos_backup;
                scanner_advance_char (scnr);
            }
        }
    }
}

void path_compute_relevance_characteristics (char *basename, uint64_t *depth)
{
    assert (depth != NULL);

    struct scanner_t _scnr = {0};
    struct scanner_t *scnr = &_scnr;
    scnr->pos = basename;

    *depth = 0;
    while (!scnr->is_eof) {
        if (scanner_char (scnr, '/')) {
            (*depth)++;
        } else {
            scanner_advance_char (scnr);
        }
    }
}

// This compares the relevance of the filename. It's used when we have identical
// duplicates, to decide which name should be the one that isn't removed.
// Compares two filenames and returns true if p1 is more relevant than p2.
//
// For now we only try to remove files with (n) in their name. Then we preffer
// names without spaces.
bool duplicate_file_name_cmp (struct string_lst_t *p1, struct string_lst_t *p2)
{
    mem_pool_t pool_l = {0};

    char *basename1 = NULL;
    char *fname1 = NULL;
    path_split (&pool_l, str_data(&p1->s), &basename1, &fname1);
    bool has_copy_parenthesis_1;
    uint64_t space_cnt_1;
    file_name_compute_relevance_characteristics (fname1, &has_copy_parenthesis_1, &space_cnt_1);
    uint64_t depth_1;
    path_compute_relevance_characteristics (basename1, &depth_1);

    char *basename2 = NULL;
    char *fname2 = NULL;
    path_split (&pool_l, str_data(&p2->s), &basename2, &fname2);
    bool has_copy_parenthesis_2;
    uint64_t space_cnt_2;
    file_name_compute_relevance_characteristics (fname2, &has_copy_parenthesis_2, &space_cnt_2);
    uint64_t depth_2;
    path_compute_relevance_characteristics (basename2, &depth_2);

    bool is_p1_lt_p2;
    if (has_copy_parenthesis_1 == true && has_copy_parenthesis_2 == false) {
        is_p1_lt_p2 = false;
    } else if (has_copy_parenthesis_1 == false && has_copy_parenthesis_2 == true) {
        is_p1_lt_p2 = true;
    } else if (space_cnt_1 < space_cnt_2) {
        is_p1_lt_p2 = true;
    } else {
        is_p1_lt_p2 = depth_1 < depth_2;
    }

    mem_pool_destroy (&pool_l);
    return is_p1_lt_p2;
}

templ_sort_ll (duplicate_relevance_sort, struct string_lst_t, duplicate_file_name_cmp(a, b));

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
    uint64_t processed_files;
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
        LINKED_LIST_PUSH_NEW (&app->hash_to_path.pool, struct string_lst_t, bucket->strings, str);
        str_set (&str->s, path);
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

struct collect_jpg_cb_clsr_t {
    mem_pool_t *pool;
    uint64_t count;
    struct string_lst_t *files;
};

ITERATE_DIR_CB (collect_jpg_cb)
{
    struct collect_jpg_cb_clsr_t *clsr = (struct collect_jpg_cb_clsr_t*) data;

    char *extension = get_extension (fname);
    if (!is_dir && extension != NULL && strncasecmp (extension, "jpg", 3) == 0) {
        LINKED_LIST_PUSH_NEW (clsr->pool, struct string_lst_t, clsr->files, new_node);
        str_set (&new_node->s, fname);
        clsr->count++;
    }
    cli_status ("Files collected: ", clsr->count);
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
    cli_status ("read files: ", sb->processed_files);
}

void test_relevance_characteristics (char *fname)
{
    bool has_copy_parenthesis;
    uint64_t space_cnt;
    file_name_compute_relevance_characteristics (fname, &has_copy_parenthesis, &space_cnt);
    printf ("copy_parenthesis: %d, space_cnt: %lu -> %s\n", has_copy_parenthesis, space_cnt, fname);
}

void fname_comparison_test ()
{
    // Have copy parenthesis
    test_relevance_characteristics ("hola (2).JPEG");
    test_relevance_characteristics ("(a parenthesis) hola (2).jpg");
    test_relevance_characteristics ("In the middle (a parenthesis) hola (2).JPG");
    test_relevance_characteristics ("(3) hola (2).jpeg");
    test_relevance_characteristics ("In the middle (10) hola(2).jpg.jpg");

    // No copy parenthesis
    test_relevance_characteristics ("hola.JPEG");
    test_relevance_characteristics ("hola(-1).JPEG");
    test_relevance_characteristics ("(a parenthesis) hola.jpg");
    test_relevance_characteristics ("In the middle (a parenthesis) hola.JPG");
    test_relevance_characteristics ("(3) hola.jpeg");
    test_relevance_characteristics ("In the middle (10) hola.jpg.jpg");
}

void print_bucket_list_fnames (struct string_bucket_t *bucket_lst)
{
    struct string_bucket_t *curr_bucket = bucket_lst;
    while (curr_bucket != NULL) {
        struct string_lst_t *curr_str = curr_bucket->strings;
        while (curr_str != NULL) {
            char *fname = NULL;
            path_split (NULL, str_data(&curr_str->s), NULL, &fname);
            printf ("'%s'", fname);
            if (curr_str->next != NULL) {
                printf (" ");
            }
            free (fname);

            curr_str = curr_str->next;
        }
        printf ("\n");

        curr_bucket = curr_bucket->next;
    }
}

void print_bucket_list_path (struct string_bucket_t *bucket_lst)
{
    struct string_bucket_t *curr_bucket = bucket_lst;
    while (curr_bucket != NULL) {
        struct string_lst_t *curr_str = curr_bucket->strings;
        while (curr_str != NULL) {
            printf ("'%s'", str_data(&curr_str->s));
            if (curr_str->next != NULL) {
                printf (" ");
            }

            curr_str = curr_str->next;
        }
        printf ("\n");

        curr_bucket = curr_bucket->next;
    }
}

// Looks up directory names passed as cli arguments and recursiveley collects
// all image files inside of them.
struct string_lst_t* collect_jpg_from_cli (mem_pool_t *pool, int argc, char **argv)
{
    struct collect_jpg_cb_clsr_t clsr = {0};
    clsr.pool = pool;

    printf ("Collecting images in:\n");
    for (int i=1; i<argc; i++) {
        char *path = abs_path_no_sh_expand (argv[i], NULL);
        if (dir_exists_no_sh_expand(path)) {
            printf ("  %s\n", path);
            iterate_dir (path, collect_jpg_cb, &clsr);
            cli_status_end ();
        }
        free (path);
    }
    printf ("\n");

    return clsr.files;
}

// Finds duplicates that are identical at file level.
void find_file_duplicates (struct scrapbook_t *sb, struct string_lst_t *files)
{
    struct string_lst_t *curr_str = files;
    while (curr_str != NULL) {
        mem_pool_t pool_l = {0};
        sb->processed_files++;

        uint64_t file_len = 0;
        char *fname = str_data(&curr_str->s);
        char *file_data = partial_file_read (&pool_l, fname, kilobyte(1), &file_len);
        sb->total_size += file_len;

        push_file_hash (sb, hash_64 (file_data, file_len), fname);

        mem_pool_destroy (&pool_l);
        curr_str = curr_str->next;
    }

    printf ("Total files read: %lu\n", sb->processed_files);
    printf ("Total size read: %lu bytes\n", sb->total_size);

    struct string_bucket_t *tentative_duplicates = NULL;
    uint32_t num_tentative_non_unique_files = 0;
    BINARY_TREE_FOR(uint_64_to_str, &sb->hash_to_path, curr_node) {
        struct string_bucket_t *bucket = curr_node->value;
        if (bucket->count > 1) {
            struct string_lst_t *curr_path = bucket->strings;
            while (curr_path != NULL) {
                num_tentative_non_unique_files++;

                curr_path = curr_path->next;
            }

            LINKED_LIST_PUSH (tentative_duplicates, bucket);
        }
    }
    printf ("Tentative non unique file count: %d\n", num_tentative_non_unique_files);

    struct string_bucket_t *exact_duplicates = NULL;
    uint64_t exact_duplicates_len = 0;
    struct string_bucket_t *non_duplicates = NULL;
    uint64_t non_duplicates_len = 0;
    if (num_tentative_non_unique_files > 0) {
        printf ("\n");
        printf ("Executing full comparison\n");

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

            cli_progress_bar (exact_duplicates_len+non_duplicates_len, num_tentative_non_unique_files);
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
    }

    if (exact_duplicates_len > 0) {
        struct string_lst_t *files_to_remove = NULL;
        struct string_bucket_t *curr_bucket = exact_duplicates;
        while (curr_bucket != NULL) {
            duplicate_relevance_sort (&curr_bucket->strings, curr_bucket->count);

            struct string_lst_t *curr_str = curr_bucket->strings->next;
            while (curr_str != NULL) {
                LINKED_LIST_PUSH_NEW (&sb->pool, struct string_lst_t, files_to_remove, new_string);
                str_set(&new_string->s, str_data(&curr_str->s));
                curr_str = curr_str->next;
            }

            curr_bucket = curr_bucket->next;
        }

        //print_bucket_list_fnames (exact_duplicates);
        //print_bucket_list_path (exact_duplicates);

        // This will print a huge rm command that will delete all duplicates.
        printf ("rm ");
        struct string_lst_t *curr_str = files_to_remove;
        while (curr_str != NULL) {
            printf ("'%s' ", str_data(&curr_str->s));
            curr_str = curr_str->next;
        }
        printf ("\n");
    }
}

// This duplicate detection will be based on the image data inside the jpg file.
// The idea is to be able to detect cases where the image data is the same but
// metadata (exif tags) have changed.
void find_image_duplicates (struct scrapbook_t *sb, struct string_lst_t *files)
{
    struct string_lst_t *curr_str = files;
    while (curr_str != NULL) {
        print_jpeg_structure (str_data(&curr_str->s));
        curr_str = curr_str->next;
    }
}

int main (int argc, char **argv)
{
    struct scrapbook_t scrapbook = {0};
    char *argument = NULL;
    if ((argument = get_cli_arg_opt ("--jpeg-structure", argv, argc)) != NULL) {
        print_jpeg_structure (argument);

    } else if ((argument = get_cli_arg_opt ("--find-duplicates-file", argv, argc)) != NULL) {
        struct string_lst_t *images = collect_jpg_from_cli (&scrapbook.pool, argc-1, argv+1);
        find_file_duplicates (&scrapbook, images);

    } else {
        printf ("Usage:\nscrapbook [--jpeg-structure <file> | --find-duplicates-file <directory>]\n");
    }

    mem_pool_destroy (&scrapbook.pool);
    return 0;
}
