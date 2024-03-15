/*
 * Copyright (C) 2020 Santiago León O.
 */
#ifdef __GNUC__
#define NOT_USED __attribute__ ((unused))
#else
#define NOT_USED
#endif

#include <limits.h>
#include "lib/meow_hash_x64_aesni.h"
// We don't use this but it causes a compiler warning.
static void MeowExpandSeed(meow_umm InputLen, void *Input, meow_u8 *SeedResult) NOT_USED;

#define _GNU_SOURCE // Used to enable strcasestr()
#define _XOPEN_SOURCE 700 // Required for strptime()
#include "common.h"
#include "concatenator.c"
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

enum file_header_status_t {
    FILE_HEADER_PATH,
    FILE_HEADER_LOADED
};

struct file_header_t {
    string_t path;

    enum file_header_status_t status;
    size_t size;
    char *data;

    struct file_header_t *next;
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
                scanner_str (scnr, ").")) {
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
// Compares two filenames and returns true if p1 is more relevant than p2 (i.e.
// p1 should be kept and p2 removed).
//
// For now we only try to remove files with (n) in their name. Then we preffer
// names without spaces.
bool duplicate_file_name_cmp (struct file_header_t *p1, struct file_header_t *p2, char *prefer_removal_if_substr)
{
    // If a substring is passed to match and prefer which one to remove, early
    // return because we don't need to allocate and it should be the common
    // case.
    if (prefer_removal_if_substr != NULL) {
        bool match1 = strstr(str_data(&p1->path), prefer_removal_if_substr) != NULL;
        bool match2 = strstr(str_data(&p2->path), prefer_removal_if_substr) != NULL;

        if (match1 == true && match2 == false) {
            return false;

        } else if (match1 == false && match2 == true) {
            return true;
        }
    }

    mem_pool_t pool_l = {0};

    char *basename1 = NULL;
    char *fname1 = NULL;
    path_split (&pool_l, str_data(&p1->path), &basename1, &fname1);
    bool has_copy_parenthesis_1;
    uint64_t space_cnt_1;
    file_name_compute_relevance_characteristics (fname1, &has_copy_parenthesis_1, &space_cnt_1);

    char *basename2 = NULL;
    char *fname2 = NULL;
    path_split (&pool_l, str_data(&p2->path), &basename2, &fname2);
    bool has_copy_parenthesis_2;
    uint64_t space_cnt_2;
    file_name_compute_relevance_characteristics (fname2, &has_copy_parenthesis_2, &space_cnt_2);

    bool is_p1_lt_p2;
    if (has_copy_parenthesis_1 == true && has_copy_parenthesis_2 == false) {
        is_p1_lt_p2 = false;
    } else if (has_copy_parenthesis_1 == false && has_copy_parenthesis_2 == true) {
        is_p1_lt_p2 = true;

    // I've seen file duplicates with .HEIC and .heif extensions, I don't know
    // what creates these .heif copies but they seem to be the odd ones because
    // the overwhelming majority of files is .HEIC.
    //
    // Also, downloads from the web browser's UI of Google Photos will rename
    // .HEIC files to .jpg. I didn't know these were replaceable. This is bad
    // because it seems that the web download provides the original files but
    // with a non-original filename. Using the REST API via rclone does restore
    // the original filename, but seems to return different data than the
    // original, sigh.
    //
    // TODO: Is there a similar preference between .jpeg and .jpg?... maybe we
    // should just prefer the most common extension?.
    } else if (strcasecmp(get_extension(fname1), "HEIC") == 0) {
        is_p1_lt_p2 = true;
    } else if (strcasecmp(get_extension(fname2), "HEIC") == 0) {
        is_p1_lt_p2 = false;

    } else if (space_cnt_1 < space_cnt_2) {
        is_p1_lt_p2 = true;
    } else {
        uint64_t depth_1, depth_2;
        path_compute_relevance_characteristics (basename1, &depth_1);
        path_compute_relevance_characteristics (basename2, &depth_2);

        is_p1_lt_p2 = depth_1 < depth_2;
    }

    mem_pool_destroy (&pool_l);
    return is_p1_lt_p2;
}
templ_sort_ll (duplicate_relevance_sort, struct file_header_t, duplicate_file_name_cmp(a, b, user_data));

bool full_file_compare (struct file_header_t *p1, struct file_header_t *p2)
{
    if (p1->size != p2->size) {
        return p1->size < p2->size;

    } else {
        return memcmp (p1->data, p2->data, p1->size) < 0;
    }
}
templ_sort_ll (file_equality_sort, struct file_header_t, full_file_compare(a, b));

struct file_bucket_t {
    uint32_t count;
    struct file_header_t *strings;

    struct file_bucket_t *next;
};

BINARY_TREE_NEW(uint64_to_str_list, uint64_t, struct file_bucket_t*,  a <= b ? (a == b ? 0 : -1) : 1);
BINARY_TREE_NEW(str_to_str_list, char*, struct file_bucket_t*, strcmp(a,b));

struct scrapbook_t {
    mem_pool_t pool;

    struct uint64_to_str_list_tree_t hash_to_path;

    uint64_t total_size; 
    uint64_t processed_files;
};

void push_file_hash (struct scrapbook_t *app, uint64_t hash, char *path)
{
    struct file_bucket_t *bucket = uint64_to_str_list_get (&app->hash_to_path, hash);
    if (bucket == NULL) {
        bucket = mem_pool_push_struct (&app->hash_to_path.pool, struct file_bucket_t);
        *bucket = ZERO_INIT (struct file_bucket_t);
        uint64_to_str_list_tree_insert (&app->hash_to_path, hash, bucket);
    }

    // TODO: This de-duplication feels wrong here because push_file_hash() is
    // called once per file, so this ends up being O(n^2). Don't we have the
    // guarantee that the collected list of files has unique paths?, if we do
    // then we should remove this and just insert at the end unconditionally.
    bool found = false;
    struct file_header_t *curr_str = bucket->strings;
    while (curr_str != NULL) {
        if (strcmp(path, str_data(&curr_str->path)) == 0) {
            found = true;
            break;
        }

        curr_str = curr_str->next;
    }

    if (!found) {
        LINKED_LIST_PUSH_NEW (&app->hash_to_path.pool, struct file_header_t, bucket->strings, str);
        str_set (&str->path, path);
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
    struct file_header_t *files;
    char *match_extension;
};

ITERATE_DIR_CB (collect_files_cb)
{
    struct collect_jpg_cb_clsr_t *clsr = (struct collect_jpg_cb_clsr_t*) data;

    char *extension = get_extension (fname);
    if (!is_dir) {
        if (clsr->match_extension == NULL || (extension != NULL && strncasecmp (extension, clsr->match_extension, 3) == 0)) {
            LINKED_LIST_PUSH_NEW (clsr->pool, struct file_header_t, clsr->files, new_node);
            str_set (&new_node->path, fname);
            clsr->count++;
        }
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

enum path_format_t{
    PATH_FORMAT_FNAME,
    PATH_FORMAT_ABSOLUTE,

    // TODO: Implement something like this and make print_bucket_list() recive
    // the base path.
    //PATH_FORMAT_RELATIVE_PATH
};

void print_bucket_list (struct file_bucket_t *bucket_lst, enum path_format_t format)
{
    struct file_bucket_t *curr_bucket = bucket_lst;
    while (curr_bucket != NULL) {
        struct file_header_t *curr_str = curr_bucket->strings;
        while (curr_str != NULL) {
            if (format == PATH_FORMAT_FNAME) {
                char *fname = NULL;
                path_split (NULL, str_data(&curr_str->path), NULL, &fname);
                printf ("'%s'", fname);
                free (fname);

            } else if (format == PATH_FORMAT_ABSOLUTE) {
                printf ("'%s'", str_data(&curr_str->path));
            }

            if (curr_str->next != NULL) {
                printf (" ");
            }

            curr_str = curr_str->next;
        }
        printf ("\n");

        curr_bucket = curr_bucket->next;
    }
}

void print_bucket_duplicates (char **paths, int paths_len,
                              struct file_bucket_t *bucket_lst, enum path_format_t format)
{
    assert (paths != NULL);

    printf ("Manually append to: ~/.weaver/data/deduplication.tsplx\n");

    // TODO: Also print this for name and image deduplication
    printf ("file-content-deduplication{\n");

    printf ("  path");
    for (int i=0; i<paths_len; i++) {
        printf (" \"%s\"", paths[i]);
    }
    printf (";\n");

    printf ("  duplicates {\n");
    struct file_bucket_t *curr_bucket = bucket_lst;
    while (curr_bucket != NULL) {
        struct file_header_t *curr_str = curr_bucket->strings;
        if (curr_bucket->strings->next != NULL) {
            bool is_first = true;
            while (curr_str != NULL) {
                if (is_first) {
                    printf ("    ");
                    is_first = false;
                }

                if (format == PATH_FORMAT_FNAME) {
                    char *fname = NULL;
                    path_split (NULL, str_data(&curr_str->path), NULL, &fname);
                    printf ("\"%s\"", fname);
                    free (fname);

                } else if (format == PATH_FORMAT_ABSOLUTE) {
                    printf ("\"%s\"", str_data(&curr_str->path));
                }

                if (curr_str->next != NULL) {
                    printf (" ");
                }

                curr_str = curr_str->next;
            }
            printf (";\n");
        }

        curr_bucket = curr_bucket->next;
    }
    printf ("  }\n");

    printf ("}\n");
}

// Looks up directory names passed as cli arguments and recursiveley collects
// all image files inside of them. If a file name is passsed, the absolute path
// to the file is appended to the resulting list.
struct file_header_t* collect_files_from_cli (mem_pool_t *pool, char *extension, char **paths, int paths_len)
{
    struct collect_jpg_cb_clsr_t clsr = {0};
    clsr.match_extension = extension;
    clsr.pool = pool;

    uint64_t file_cnt = 0;
    printf (ECMA_S_DEFAULT(1, "Creating file list\n"));
    for (int i=0; i<paths_len; i++) {
        char *path = abs_path (paths[i], NULL);
        printf ("PATH: %s\n", path);
        if (dir_exists (path)) {
            printf ("%s/**\n", path);
            iterate_dir_full (path, collect_files_cb, &clsr, true);
            cli_status_end ();

        } else if (path_exists (path)) {
            printf ("%s\n", path);
            LINKED_LIST_PUSH_NEW (pool, struct file_header_t, clsr.files, new_node);
            str_set (&new_node->path, path);
            file_cnt++;

        } else {
            printf ("%s (not found, ignoring)\n", path);
        }

        if (path != NULL) {
            free (path);
        }
    }
    printf ("Total files: %lu\n", file_cnt + clsr.count);
    printf ("\n");

    return clsr.files;
}

struct file_header_t* collect_jpg_from_cli (mem_pool_t *pool, char **paths, int paths_len)
{
    return collect_files_from_cli (pool, "jpg", paths, paths_len);
}

// Finds duplicates that are identical at file level.
//
// When a file has multiple duplicates we automatically decide which one to
// remove according to the criteria of duplicate_file_name_cmp(). Any path that
// contains remove_substr as substring will be prefered for removal over one
// that doesn't contaín it.
struct file_bucket_t* find_file_duplicates (struct scrapbook_t *sb, struct file_header_t *files)
{
    struct file_header_t *curr_str = files;
    while (curr_str != NULL) {
        mem_pool_t pool_l = {0};
        sb->processed_files++;

        uint64_t file_len = 0;
        char *fname = str_data(&curr_str->path);
        // TODO: The length of the initial partial read is currently hardcoded
        // to the smallest value I've found to work fo pictures such that we
        // don't group together files that should actually be separate. 1kB
        // worked fine for a while, but for HEIC/HEIF files we got lots of false
        // positives, so it's now been increased to 5kB. Should this be a CLI
        // parameter?.
        char *file_data = partial_file_read (&pool_l, fname, kilobyte(5), &file_len);
        sb->total_size += file_len;

        push_file_hash (sb, hash_64 (file_data, file_len), fname);

        mem_pool_destroy (&pool_l);
        curr_str = curr_str->next;
        cli_status ("Files processed: ", sb->processed_files);
    }
    cli_status_end ();

    printf ("Total files read: %lu\n", sb->processed_files);
    printf ("Total size read: %lu bytes\n", sb->total_size);

    struct file_bucket_t *tentative_duplicates = NULL;
    uint32_t num_tentative_non_unique_files = 0;
    BINARY_TREE_FOR(uint64_to_str_list, &sb->hash_to_path, curr_node) {
        struct file_bucket_t *bucket = curr_node->value;
        if (bucket->count > 1) {
            struct file_header_t *curr_path = bucket->strings;
            while (curr_path != NULL) {
                num_tentative_non_unique_files++;

                curr_path = curr_path->next;
            }

            LINKED_LIST_PUSH (tentative_duplicates, bucket);
        }
    }
    printf ("Tentative non unique file count: %d\n", num_tentative_non_unique_files);

    struct file_bucket_t *exact_duplicates = NULL;
    uint64_t exact_duplicates_len = 0;
    if (num_tentative_non_unique_files > 0) {
        printf ("\n");
        printf ("Executing full comparison\n");

        bool had_to_split_buckets = false;
        while (tentative_duplicates != NULL)
        {
            struct file_bucket_t *curr_bucket = LINKED_LIST_POP(tentative_duplicates);

            mem_pool_t pool_l = {0};

            // Load all files in the bucket into memory
            //
            // TODO: If the cumulative size of files in the bucket is too big
            // we will run out of memory. This can happen if for instance the
            // partial equality test is done with a short size and all files
            // pass as being equal (for example comparing less that 5 bytes of
            // JPG files). In such case, we should bail out of this algorithm
            // and move to one where files are sorted but only loaded into
            // memory for each comparison. Ideally we should have a bucket size
            // limit as a parameter and preemptively execute the correct
            // algorithm, not wait until we run out of memory.
            LINKED_LIST_FOR (struct file_header_t*, curr_file, curr_bucket->strings) {
                curr_file->data = full_file_read (&pool_l, str_data(&curr_file->path), &curr_file->size);
                curr_file->status = FILE_HEADER_LOADED;
            }

            // Sort files by comparing their content
            // This is O(n log(n)) on the size of the bucket and each performed
            // operation is a full file comparison, which can be slow.
            file_equality_sort (&curr_bucket->strings, curr_bucket->count);

            // Split bucket if there are non-equal files in it
            //
            // TODO: This makes O(n) full file comparisons which can get
            // expensive. Is there a way we can use comparisons made during
            // sorting and just require making O(n) fast comparisons here
            // instead of the full file?. Maybe using a stable sort and marking
            // when a comparison results in a difference we can get the points
            // at which a sequence of equal files ends. Still, stable sorting
            // requires 3-way comparison, so maybe it ends up being exactly as
            // fast as doing this?. On the other hand memcmp already makes a
            // 3-way compare, so maybe it would be faster because we would avoid
            // calling memcmp from 2 places.
            uint64_t equal_file_run_len = 1;
            curr_file = curr_bucket->strings;
            while (curr_file != NULL && curr_file->next != NULL) {
                uint64_t f1_len = curr_file->size;
                char *f1 = curr_file->data;

                uint64_t f2_len = curr_file->next->size;
                char *f2 = curr_file->next->data;

                struct file_header_t *new_first_file = curr_file->next;


                if (f1_len != f2_len || memcmp (f1, f2, f1_len) != 0) {
                    had_to_split_buckets = true;

                    struct file_bucket_t *new_bucket =
                        mem_pool_push_struct (&sb->hash_to_path.pool, struct file_bucket_t);
                    *new_bucket = ZERO_INIT (struct file_bucket_t);
                    new_bucket->strings = curr_bucket->strings;
                    new_bucket->count = equal_file_run_len;

                    curr_bucket->strings = new_first_file;
                    curr_bucket->count -= equal_file_run_len;
                    curr_file->next = NULL;
                    equal_file_run_len = 1;

                    LINKED_LIST_PUSH (exact_duplicates, new_bucket);
                    exact_duplicates_len += new_bucket->count;

                } else {
                    equal_file_run_len++;
                }

                // Mark current file as unloaded because we will destroy the
                // pool where it was loaded into.
                curr_file->status = FILE_HEADER_PATH;
                curr_file->data = NULL;
                curr_file = new_first_file;
            }

            // Mark last file header as unloaded.
            curr_file->status = FILE_HEADER_PATH;
            curr_file->data = NULL;

            LINKED_LIST_PUSH (exact_duplicates, curr_bucket);
            exact_duplicates_len += curr_bucket->count;

            mem_pool_destroy (&pool_l);

            cli_progress_bar (exact_duplicates_len, num_tentative_non_unique_files);
        }
        printf ("\n");

        if (had_to_split_buckets) {
            printf (ECMA_YELLOW("warning:") " non-equal files passed the partial equality test by hash. "
                    "Either there was a hash collision or the content of the file was "
                    "the same only up to a certain point.\n\n");
        }
    }

    return exact_duplicates;
}

void push_file_path (mem_pool_t *pool, struct str_to_str_list_tree_t *filename_tree, char *path)
{
    char *filename = path_basename (path);

    struct file_bucket_t *bucket = str_to_str_list_get (filename_tree, filename);
    if (bucket == NULL) {
        bucket = mem_pool_push_struct (pool, struct file_bucket_t);
        *bucket = ZERO_INIT (struct file_bucket_t);
        str_to_str_list_tree_insert (filename_tree, filename, bucket);
    }

    LINKED_LIST_PUSH_NEW (pool, struct file_header_t, bucket->strings, str);
    str_set (&str->path, path);
    bucket->count++;
}

// Finds files with the same name.
struct file_bucket_t* find_file_name_duplicates (struct scrapbook_t *sb, struct file_header_t *files)
{
    struct str_to_str_list_tree_t filename_to_path = {0};

    struct file_header_t *curr_str = files;
    while (curr_str != NULL) {
        sb->processed_files++;

        char *fname = str_data(&curr_str->path);
        push_file_path (&sb->pool, &filename_to_path, fname);

        curr_str = curr_str->next;
        cli_status ("Files processed: ", sb->processed_files);
    }
    cli_status_end ();

    printf ("Total files: %lu\n", sb->processed_files);

    // Collect buckets with more than 1 element
    struct file_bucket_t *name_duplicates = NULL;
    BINARY_TREE_FOR(str_to_str_list, &filename_to_path, curr_node) {
        struct file_bucket_t *bucket = curr_node->value;
        if (bucket->count > 1) {
            struct file_header_t *curr_path = bucket->strings;
            while (curr_path != NULL) {
                curr_path = curr_path->next;
            }

            LINKED_LIST_PUSH (name_duplicates, bucket);
        }
    }

    return name_duplicates;
}

// This duplicate detection will be based on the image data inside the jpg file.
// The idea is to be able to detect cases where the image data is the same but
// metadata (exif tags) have changed.
struct file_bucket_t* find_image_duplicates (struct scrapbook_t *sb, struct file_header_t *files)
{
    struct file_header_t *curr_str = files;
    while (curr_str != NULL) {
        mem_pool_t pool_l = {0};
        sb->processed_files++;

        uint64_t file_len = 0;
        char *fname = str_data(&curr_str->path);
        char *file_data = jpg_image_data_read (&pool_l, fname, kilobyte(1), &file_len);
        sb->total_size += file_len;

        push_file_hash (sb, hash_64 (file_data, file_len), fname);

        mem_pool_destroy (&pool_l);
        curr_str = curr_str->next;
        cli_status ("Files processed: ", sb->processed_files);
    }
    cli_status_end ();

    printf ("Total files read: %lu\n", sb->processed_files);
    printf ("Total size read: %lu bytes\n", sb->total_size);

    struct file_bucket_t *tentative_duplicates = NULL;
    uint32_t num_tentative_non_unique_files = 0;
    BINARY_TREE_FOR(uint64_to_str_list, &sb->hash_to_path, curr_node) {
        struct file_bucket_t *bucket = curr_node->value;
        if (bucket->count > 1) {
            struct file_header_t *curr_path = bucket->strings;
            while (curr_path != NULL) {
                num_tentative_non_unique_files++;

                curr_path = curr_path->next;
            }

            LINKED_LIST_PUSH (tentative_duplicates, bucket);
        }
    }
    printf ("Tentative non unique file count: %d\n", num_tentative_non_unique_files);

    print_bucket_list (tentative_duplicates, PATH_FORMAT_FNAME);
    print_bucket_list (tentative_duplicates, PATH_FORMAT_ABSOLUTE);

    struct file_bucket_t *exact_duplicates = NULL;
    uint64_t exact_duplicates_len = 0;
    struct file_bucket_t *non_duplicates = NULL;
    uint64_t non_duplicates_len = 0;
    if (num_tentative_non_unique_files > 0) {
        printf ("\n");
        printf ("Executing full comparison\n");

        while (tentative_duplicates != NULL)
        {
            struct file_bucket_t *curr_bucket = LINKED_LIST_POP(tentative_duplicates);

            bool all_equal = true;

            struct file_header_t *curr_str = curr_bucket->strings;
            while (all_equal && curr_str != NULL && curr_str->next != NULL) {
                mem_pool_t pool_l = {0};
                uint64_t f1_len;
                char *f1 = jpg_image_data_read (&pool_l, str_data(&curr_str->path), -1, &f1_len);

                uint64_t f2_len;
                char *f2 = jpg_image_data_read (&pool_l, str_data(&curr_str->next->path), -1, &f2_len);

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

    return exact_duplicates;
}

void remove_duplicates (struct scrapbook_t *sb,
                        struct file_bucket_t *bucket_list,
                        char *remove_substr, char *removal_filter, bool is_dry_run)
{
    if (bucket_list == NULL) return;

    // Sort each bucket and build a list of all files that will be removed.
    // NOTE: This mutates the order of each bucket in the input list.
    uint64_t num_buckets = 0;
    struct file_header_t *files_to_remove = NULL;
    uint64_t files_to_remove_len = 0;
    LINKED_LIST_FOR (struct file_bucket_t*, curr_bucket, bucket_list) {
        duplicate_relevance_sort_user_data (&curr_bucket->strings, curr_bucket->count, remove_substr);

        LINKED_LIST_FOR(struct file_header_t*, curr_str, curr_bucket->strings->next) {
            if (removal_filter == NULL ||
                strstr(str_data(&curr_str->path), removal_filter) == str_data(&curr_str->path))
            {
                LINKED_LIST_PUSH_NEW (&sb->pool, struct file_header_t, files_to_remove, new_string);
                files_to_remove_len++;
                str_set(&new_string->path, str_data(&curr_str->path));
            }
        }

        num_buckets++;
    }

    printf ("Unique files: %ld\n", num_buckets);
    printf ("Files to be removed: %ld\n", files_to_remove_len);
    printf ("\n");

    // Print the list of files to be removed
    if (files_to_remove_len > 0) {
        LINKED_LIST_FOR (struct file_header_t*, curr_str, files_to_remove) {
            printf ("D '%s'\n", str_data(&curr_str->path));
        }
        printf ("\n");
    }

    // Actually remove all duplicate files
    if (!is_dry_run) {
        LINKED_LIST_FOR (struct file_header_t*, curr_str, files_to_remove) {
            unlink (str_data(&curr_str->path));
        }
    }
}

void print_hex_bytes (void *data, uint64_t data_len)
{
    for (uint32_t value_idx = 0; value_idx < data_len; value_idx++) {
        uint8_t byte = ((uint8_t*)data)[value_idx];
        printf ("%.2X", byte);

        if (value_idx < data_len-1) {
            printf (" ");
        }
    }
}

// Debug procedure to test stuff in all images in a list of file names.
void testing_function (struct scrapbook_t *sb, struct file_header_t *files)
{
    struct file_header_t *curr_str = files;
    while (curr_str != NULL) {
        sb->processed_files++;

        string_t output = {0};
        char *fname = str_data(&curr_str->path);
        cat_jpeg_structure (&output, fname);
        printf ("%s", str_data(&output));

        curr_str = curr_str->next;
    }
}

int main (int argc, char **argv)
{
    struct scrapbook_t scrapbook = {0};
    int paths_count = argc - 2;
    char **paths = argv + 2;

    char *remove_substr = get_cli_arg_opt ("--prefer-removal-substr", argv, argc);
    if (remove_substr != NULL) {
        paths_count -= 2;
        paths += 2;
    }

    char *removal_filter = get_cli_arg_opt ("--removal-filter", argv, argc);
    if (removal_filter != NULL) {
        paths_count -= 2;
        paths += 2;
    }

    bool is_remove = get_cli_bool_opt ("--remove", argv, argc);
    if (is_remove) {
        paths_count -= 1;
        paths += 1;
    }
    bool is_dry_run = !is_remove;

    char *argument = NULL;
    if ((argument = get_cli_arg_opt ("--jpeg-structure", argv, argc)) != NULL) {
        print_jpeg_structure (argument);

    } else if ((argument = get_cli_arg_opt ("--exif", argv, argc)) != NULL) {
        print_exif (argument);

    } else if ((argument = get_cli_arg_opt ("--image-info", argv, argc)) != NULL) {
        mem_pool_t pool = {0};

        uint64_t file_len;
        char *file = full_file_read (&pool, argument, &file_len);
        printf ("file hash: ");
        printf ("%lu\n", hash_64 (file, file_len));

        uint64_t partial_file_len;
        char *partial_file = partial_file_read (&pool, argument, kilobyte(1), &partial_file_len);
        printf ("file partial hash: ");
        printf ("%lu\n", hash_64 (partial_file, partial_file_len));

        uint64_t image_data_len;
        char *image_data = jpg_image_data_read (&pool, argument, -1, &image_data_len);
        printf ("image data hash: ");
        printf ("%lu\n", hash_64 (image_data, image_data_len));

        printf ("image data partial hash: ");
        printf ("%lu\n", hash_64 (image_data, MIN(kilobyte(5), image_data_len)));

        printf ("image data: ");
        print_hex_bytes (image_data, 20);
        printf ("...\n");

        mem_pool_destroy (&pool);

    } else if ((argument = get_cli_arg_opt ("--debug", argv, argc)) != NULL) {
        struct file_header_t *images = collect_jpg_from_cli (&scrapbook.pool, argv+2, argc-2);
        testing_function (&scrapbook, images);

    } else if ((argument = get_cli_arg_opt ("--find-duplicates-file-name", argv, argc)) != NULL) {
        struct file_header_t *images = collect_files_from_cli (&scrapbook.pool, NULL, paths, paths_count);
        struct file_bucket_t *duplicates = find_file_name_duplicates (&scrapbook, images);
        remove_duplicates (&scrapbook, duplicates, remove_substr, removal_filter, is_dry_run);

    } else if ((argument = get_cli_arg_opt ("--find-duplicates-file", argv, argc)) != NULL) {
        struct file_header_t *images = collect_files_from_cli (&scrapbook.pool, NULL, paths, paths_count);
        struct file_bucket_t *duplicates = find_file_duplicates (&scrapbook, images);
        remove_duplicates (&scrapbook, duplicates, remove_substr, removal_filter, is_dry_run);

        //print_bucket_list (duplicates, PATH_FORMAT_FNAME);
        //printf ("\n");

        if (duplicates != NULL && duplicates->count > 0) {
            print_bucket_duplicates (paths, paths_count, duplicates, PATH_FORMAT_ABSOLUTE);
        }


    } else if ((argument = get_cli_arg_opt ("--find-duplicates-image", argv, argc)) != NULL) {
        struct file_header_t *images = collect_jpg_from_cli (&scrapbook.pool, argv+2, argc-2);
        struct file_bucket_t *duplicates = find_image_duplicates (&scrapbook, images);

        if (duplicates != NULL && duplicates->count > 0) {
            print_bucket_duplicates (paths, paths_count, duplicates, PATH_FORMAT_ABSOLUTE);
        }

    } else {
        printf ("Usage:\n");
        printf ("scrapbook --jpeg-structure FILE\n");
        printf ("scrapbook [--find-duplicates-file-name | --find-duplicates-file | --find-duplicates-image] [--remove] PATHS...\n");
    }

    mem_pool_destroy (&scrapbook.pool);
    return 0;
}
