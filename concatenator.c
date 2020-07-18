/*
 * Copiright (C) 2020 Santiago León O.
 */

#define DEFAULT_INITIAL_SIZE kilobyte(150)
#define DEFAULT_INDENT_SIZE 4

// The implementation of str_cat() does the following:
//
//  1. Allocate a new string of the new required size.
//  2. Copy the old value of the string into the new allocated array.
//  3. Write the data to be concatenated.
//
// To keep the implementation simple and understandable, we don't try to grow
// the underlying array to double the size. The only case in which an allocation
// is avoided is when the string is still small enough and we are using the
// small string optimized implementation.
//
// Concatenating strings is a very common thing we want to do. For example when
// creating the output of a CLI command or when creating configuration or data
// storage files. Using str_cat() can become O(n^2) in the worst case. This API
// is a way of performing this common string manipulation, but avoiding all the
// unnecessary copies on each call to catr_cat().
//
// The way to do this is by storing strings as a linked list of pointers to
// character arrays. Then the user calls a function that computes the total
// size, creates the memory and writes all the data into it.
//
// The point of this API is to be fast, so we try to make as few copies of the
// passed strings as possible. If a copy of the passed string is required the
// *_dup() set of functions should be used. This way the code will explicitly
// show when a copy is being created.

// Cases where we append a sequence of very small strings will be a very bad for
// this implementation.
// TODO: Don't create a full catr_string_t node for small strings to prevent a
// huge memory overhead.

struct catr_string_t {
    char *s;
    uint64_t len;

    struct catr_string_t *next;
};

struct concatenator_t {
    mem_pool_t pool;

    int32_t indent_size;
    int32_t current_indent;

    struct catr_string_t *strings;
    struct catr_string_t *strings_end;
};

void catr_init (struct concatenator_t *catr, uint64_t initial_size, uint32_t indent_size)
{
    catr->pool.min_bin_size = initial_size;
    catr->indent_size = indent_size;

    // HACK: Force allocation of the first (and hopefully last) bin.
    // We should abstract out bin creation in common.h so we can call this
    // without allocating an unnecessary byte.
    mem_pool_push_size (&catr->pool, 1);
}

void catr_push_indent (struct concatenator_t *catr)
{
    if (catr->indent_size == 0) {
        // Default indentation size in spaces.
        catr->indent_size = DEFAULT_INDENT_SIZE;
    }

    catr->current_indent += catr->indent_size;
}

void catr_pop_indent (struct concatenator_t *catr)
{
    if (catr->current_indent - catr->indent_size >= 0) {
        catr->current_indent -= catr->indent_size;
    }
}

GCC_PRINTF_FORMAT(2, 3)
void catr_cat (struct concatenator_t *catr, char *format, ...)
{
    struct catr_string_t *new_string = mem_pool_push_struct (&catr->pool, struct catr_string_t);
    *new_string = ZERO_INIT (struct catr_string_t);

    uint64_t str_len = 0;
    char *str = NULL;
    if (catr->current_indent == 0) {
        PRINTF_INIT (format, size, args);
        str = mem_pool_push_size (&catr->pool, size);
        PRINTF_SET (str, size, format, args);
        str_len = size - 1;

    } else {
        PRINTF_INIT (format, size, args);

        // In the case we are concatenating an indented string, we need to know
        // the resulting size after adding the indentation characters, so we can
        // allocate a string of the correct size. The straightforward algorithm
        // would be:
        //
        // 1. Allocate a _tmp_ buffer of length _size_, and use PRINTF_SET to
        //    write the unindented output string. Note that this intermediate
        //    buffer shouldn't be allocated in catr->pool because it's useless
        //    after this function completes. We either malloc it or put it in a
        //    local pool which will effectiveley call malloc every time too.
        //
        // 2. Count the number of '\n' characters that need indentation, save is
        //    as _indented_line_cnt_.
        //
        // 3. Allocate _res_ from catr->pool as a buffer of length
        //       size + _indented_line_cnt_*catr->current_indent
        //
        // 4. Iterate over _tmp_ while writing to _res_ the indented string.
        //
        // 5. Free memory allocated for _tmp_.
        //
        // In the end this algorithm does a few things I don't like: it
        // allocates/frees O(_size_), then it iterates _tmp_ twice, which is
        // 2*_size_ operations. When we don't indent, we can write directly to
        // the _res_ buffer, then we are only doing _size_ operations and not
        // calling malloc/free.
        //
        // One idea to avois some of this wor could be to allocate _tmp_ in the
        // stack. I don't like this because then concatenating a string would
        // cause a stack overflow, and we are still iterating _tmp_ twice.
        //
        // The following code implements another idea I came up with.

        // 1. Allocate a result buffer of length
        //        size + (catr->current_indent*size/2)
        //
        //    The indentation algorithm only adds indentation for non empty
        //    lines. This means that streams of consecutive '\n' characters will
        //    at most add catr->current_indent characters. Then the number of
        //    '\n' characters that cause indentation is at most _size_/2, then
        //    we know the resulting string can't be longer than that.
        //
        //    Note that this buffer is allocated directly into catr->pool.
        uint64_t used_bak = catr->pool.used;
        str = mem_pool_push_size (&catr->pool, size + (catr->current_indent*size)/2);

        // 2. Allocate a buffer of length _size_ and write to it the result of
        // the printf format. This will not call malloc() as long as catr->pool
        // is big enough.
        mem_pool_marker_t mrkr = mem_pool_begin_temporary_memory (&catr->pool);
        char *tmp = mem_pool_push_size (&catr->pool, size);
        PRINTF_SET (tmp, size, format, args);

        // 3. Perform indentation by reading _tmp_ and writing to _res_.
        uint32_t indented_line_cnt = 0;
        char *c = str;
        if (catr->strings_end != NULL && catr->strings_end->s[catr->strings_end->len-1] == '\n') {
            memset (str, ' ', catr->current_indent);
            c += catr->current_indent;
            indented_line_cnt++;
        }

        while (tmp && *tmp) {
            // NOTE: We can safely check thenext character because we know
            // *tmp != '\0', otherwise we wouldn't enter the while loop.
            if (*tmp == '\n' && *(tmp+1) != '\n' && *(tmp+1) != '\0') {
                indented_line_cnt++;
                *c = '\n';
                c++;

                memset (c, ' ', catr->current_indent);
                c += catr->current_indent;

            } else {
                *c = *tmp;
                c++;
            }
            tmp++;
        }

        // str_len doens't include the terminating null byte.
        str_len = size - 1/*null byte*/ + indented_line_cnt*catr->current_indent;

        // 4. Deallocate _tmp_ and resize _res_ to be just the correct size by
        //    manually manipulating the memory pool's 'used' value. Note that
        //    this will not call free() if catr->pool is big enough.
        //
        // TODO: Maybe it would be useful to have an API like
        //
        //               mem_pool_resize(mrkr, size)
        //
        // It frees everything up to the bin where mrkr was taken, but then
        // leaves that bin with size bytes allocated after the point mrkr was
        // taken. In order to be useful it would be an intrinsically unsafe API,
        // if size ends up putting usage past the end of the allocated bin, that
        // would be bad, but we can probably detect this error. What we won't be
        // able to detect is if the passed size will exceed the size of the
        // memory allocated just after the marker was taken, doing so could
        // corrupt other stuff in the pool, and trying to detect this would mean
        // we keep track of all allocated pointers we return and their requested
        // sizes, which is probably overkill?.
        mem_pool_end_temporary_memory (mrkr);
        catr->pool.used = used_bak + str_len + 1;

        // Although this algorithm will work for very long strings that have
        // lots of short lines, it's not ideal, as it will allocate a
        // potentially very long _tmp_ buffer, depending on the current
        // indentation level. In this case it would probably be better to do the
        // straightforward approach and just allocate another buffer of lenght
        // _size_, then iterate it twice and free it.
        //
        // I expect the most common call to this fucntion to be something like:
        //        catr_cat (catr, "Some value: %u = %u\n", var1, var2);
        //
        // Which should work fine with this algorithm. Something unexpected
        // would look like like
        //        catr_cat (catr, "%s", long_string);
        //
        // For that, it should be better to do
        //        catr_c_dup (catr, long_string, long_string_len);
        //
        // TODO: Another approach to this is to store indentation inside each
        // catr_string_t, and only actually print it when computing the
        // resulting string. This is probably a cleaner approach.
    }
    new_string->s = str;
    new_string->len = str_len;

    LINKED_LIST_APPEND(catr->strings, new_string);
}

// NOTE: This doesn't include the terminating NULL byte.
static inline
uint64_t catr_compute_len (struct concatenator_t *catr)
{
    uint64_t total_len = 0; // Doesn't count the terminating null byte.
    struct catr_string_t *curr_string = catr->strings;
    while (curr_string != NULL) {
        total_len += curr_string->len;

        curr_string = curr_string->next;
    }

    return total_len;
}

// NOTE: _dst_ MUST be of size at least catr_compute_len(_catr_) + 1
// TODO: Maybe receive an optional _len_ argument and check that _len_>0. If
// this isn't true we can either assert or just write until the buffer is full?.
static inline
void catr_write (struct concatenator_t *catr, char *dst)
{
    struct catr_string_t *curr_string = catr->strings;
    while (curr_string != NULL) {
        // TODO: What would happen if someone does this?
        //
        // struct concatenator_t catr = {0};
        // string_t s = str_new ("Something");
        // catr_cat_c (&catr, "Start", 5);
        // catr_cat_c (&catr, str_data(&s), str_len(&s));
        // str_catr (&s, &catr);
        //
        // The caller would probably expect s to have "StartSomething", but
        // using memcpy() here will probably break this because we are copying
        // overlapping memory areas. I'm not even sure using memmove() will fix
        // this, I need to try it out.
        memcpy (dst, curr_string->s, curr_string->len);
        dst += curr_string->len;

        curr_string = curr_string->next;
    }
    *dst = '\0';
}

void str_cat_catr (string_t *str, struct concatenator_t *catr)
{
    uint64_t total_len = catr_compute_len (catr);
    
    uint32_t original_len = str_len(str);
    str_maybe_grow (str, original_len + total_len, true);

    char *dst = str_data(str) + original_len;
    catr_write (catr, dst);
}

char* catr_write_c (mem_pool_t *pool, struct concatenator_t *catr)
{
    char *res = mem_pool_push_size (pool, catr_compute_len (catr) + 1);
    catr_write (catr, res);

    return res;
}

void print_catr (struct concatenator_t *catr)
{
    mem_pool_t pool = {0};
    char *str = catr_write_c (&pool, catr);
    printf ("%s", str);
    mem_pool_destroy (&pool);
}
