/*
 * Copiright (C) 2020 Santiago León O.
 */

bool file_read_bytes (int file, uint64_t bytes_to_read, string_t *buffer)
{
    bool success = true;

    str_maybe_grow (buffer, bytes_to_read, false);
    char *dst = str_data(buffer);

    uint64_t bytes_read = 0;
    do {
        int status = read (file, dst, bytes_to_read-bytes_read);
        if (status == -1) {
            success = false;
            break;
        }
        bytes_read += status;
    } while (bytes_read != bytes_to_read);

    return success;
}

void str_cat_bytes (string_t *str, uint8_t *bytes, uint64_t len)
{
    if (bytes != NULL) {
        while (len != 0) {
            str_cat_printf (str, "0x%02X", *(uint8_t*)bytes);

            len--;
            if (len != 0) {
                str_cat_c (str, " ");
            }
            bytes++;
        }
    } else {
        str_cat_printf (str, "(null)");
    }

}

uint8_t* bin_data (string_t *str)
{
    return (uint8_t*)str_data(str);
}

struct jpg_reader_t;

enum jpg_reader_endianess_t {
    BYTE_READER_BIG_ENDIAN,
    BYTE_READER_LITTLE_ENDIAN,
};

#define JPG_READER_API_READ_BYTES(name) \
    uint8_t* name(struct jpg_reader_t *rdr, uint64_t bytes_to_read)
typedef JPG_READER_API_READ_BYTES(jpg_reader_api_read_bytes_t);

#define JPG_READER_API_ADVANCE_BYTES(name) \
    void name(struct jpg_reader_t *rdr, uint64_t length)
typedef JPG_READER_API_ADVANCE_BYTES(jpg_reader_api_advance_bytes_t);

BINARY_TREE_NEW(jpg_marker_to_str, int, char*, a-b);
struct jpg_reader_t {
    mem_pool_t pool;

    uint8_t *data;
    uint8_t *pos;

    int file;
    string_t buff;

    uint64_t file_size;
    uint64_t offset;

    enum jpg_reader_endianess_t endianess;

    struct jpg_marker_to_str_tree_t marker_names;

    bool error;
    string_t error_msg;

    jpg_reader_api_read_bytes_t *read_bytes;
    jpg_reader_api_advance_bytes_t *advance_bytes;
};

void jpg_reader_destroy (struct jpg_reader_t *rdr)
{
    str_free (&rdr->error_msg);
    str_free (&rdr->buff);
    jpg_marker_to_str_tree_destroy (&rdr->marker_names);

    if (rdr->file != 0) {
        close (rdr->file);
    }

    mem_pool_destroy (&rdr->pool);
}

#define JPG_MARKER_TABLE \
    JPG_MARKER_ROW(ERR,   0x0000) /* Non standard, used to identify errors.*/ \
                                   \
    JPG_MARKER_ROW(SOF0,  0xFFC0)  \
    JPG_MARKER_ROW(SOF1,  0xFFC1)  \
    JPG_MARKER_ROW(SOF2,  0xFFC2)  \
    JPG_MARKER_ROW(SOF3,  0xFFC3)  \
    JPG_MARKER_ROW(DHT,   0xFFC4)  \
    JPG_MARKER_ROW(SOF5,  0xFFC5)  \
    JPG_MARKER_ROW(SOF6,  0xFFC6)  \
    JPG_MARKER_ROW(SOF7,  0xFFC7)  \
    JPG_MARKER_ROW(JPG,   0xFFC8)  \
    JPG_MARKER_ROW(SOF9,  0xFFC9)  \
    JPG_MARKER_ROW(SOF10, 0xFFCA)  \
    JPG_MARKER_ROW(SOF11, 0xFFCB)  \
    JPG_MARKER_ROW(DAC,   0xFFCC)  \
    JPG_MARKER_ROW(SOF13, 0xFFCD)  \
    JPG_MARKER_ROW(SOF14, 0xFFCE)  \
    JPG_MARKER_ROW(SOF15, 0xFFCF)  \
                                   \
    JPG_MARKER_ROW(RST0, 0xFFD0)   \
    JPG_MARKER_ROW(RST1, 0xFFD1)   \
    JPG_MARKER_ROW(RST2, 0xFFD2)   \
    JPG_MARKER_ROW(RST3, 0xFFD3)   \
    JPG_MARKER_ROW(RST4, 0xFFD4)   \
    JPG_MARKER_ROW(RST5, 0xFFD5)   \
    JPG_MARKER_ROW(RST6, 0xFFD6)   \
    JPG_MARKER_ROW(RST7, 0xFFD7)   \
                                   \
    JPG_MARKER_ROW(SOI,  0xFFD8)   \
    JPG_MARKER_ROW(EOI,  0xFFD9)   \
    JPG_MARKER_ROW(SOS,  0xFFDA)   \
    JPG_MARKER_ROW(DQT,  0xFFDB)   \
    JPG_MARKER_ROW(DNL,  0xFFDC)   \
    JPG_MARKER_ROW(DRI,  0xFFDD)   \
    JPG_MARKER_ROW(DHP,  0xFFDE)   \
    JPG_MARKER_ROW(EXP,  0xFFDF)   \
                                   \
    JPG_MARKER_ROW(APP0,  0xFFE0)  \
    JPG_MARKER_ROW(APP1,  0xFFE1)  \
    JPG_MARKER_ROW(APP2,  0xFFE2)  \
    JPG_MARKER_ROW(APP3,  0xFFE3)  \
    JPG_MARKER_ROW(APP4,  0xFFE4)  \
    JPG_MARKER_ROW(APP5,  0xFFE5)  \
    JPG_MARKER_ROW(APP6,  0xFFE6)  \
    JPG_MARKER_ROW(APP7,  0xFFE7)  \
    JPG_MARKER_ROW(APP8,  0xFFE8)  \
    JPG_MARKER_ROW(APP9,  0xFFE9)  \
    JPG_MARKER_ROW(APP10, 0xFFEA)  \
    JPG_MARKER_ROW(APP11, 0xFFEB)  \
    JPG_MARKER_ROW(APP12, 0xFFEC)  \
    JPG_MARKER_ROW(APP13, 0xFFED)  \
    JPG_MARKER_ROW(APP14, 0xFFEE)  \
    JPG_MARKER_ROW(APP15, 0xFFEF)  \
                                   \
    JPG_MARKER_ROW(COM, 0xFFFE)    \
                                   \
    JPG_MARKER_ROW(TEM, 0xFF01)

#define JPG_MARKER_ROW(SYMBOL,VALUE) JPG_MARKER_ ## SYMBOL = VALUE,
enum marker_t {
    JPG_MARKER_TABLE
};
#undef JPG_MARKER_ROW

#define JPG_MARKER_APP(marker) ((marker & 0xFFF0) == JPG_MARKER_APP0)
#define JPG_MARKER_SOF(marker) (marker != JPG_MARKER_DHT && \
                                marker != JPG_MARKER_JPG && \
                                marker != JPG_MARKER_DAC && \
                                (marker & 0xFFF0) == JPG_MARKER_SOF0)
#define JPG_MARKER_RST(marker) ((marker & 0xFFF0) == JPG_MARKER_RST0 && (marker & 0x000F) <= 7)

GCC_PRINTF_FORMAT(2, 3)
void jpg_error (struct jpg_reader_t *rdr, const char *format, ...)
{
    if (rdr->error == true) {
        return;
    }
    rdr->error = true;

    PRINTF_INIT (format, size, args);
    // Here size includes NULL byte and str_maybe_grow() adds 1 to passed size
    // to ensure space for a NULL byte. Need to substract 1.
    str_maybe_grow (&rdr->error_msg, size - 1, false); 
    char *str = str_data (&rdr->error_msg);

    PRINTF_SET (str, size, format, args);
}

////////////////////////////////
// -----------
// File reader
// -----------
//
// A jpg reader that reads directly form the file system. It will be faster if
// we need to quickly process a lot of files of which we are reading a small
// part, like its metadata.
JPG_READER_API_READ_BYTES(jpg_file_reader_read_bytes)
{
    if (rdr->error == true) {
        return NULL;
    }

    if (rdr->offset + bytes_to_read > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return NULL;
    }

    uint8_t *data = NULL;
    if (file_read_bytes (rdr->file, bytes_to_read, &rdr->buff)) {
        data = bin_data(&rdr->buff);
        rdr->offset += bytes_to_read;
    } else {
        jpg_error (rdr, "File read error.");
    }

    return data;
}

JPG_READER_API_ADVANCE_BYTES(jpg_file_reader_advance_bytes)
{
    if (rdr->error == true) {
        return;
    }

    if (rdr->offset + length > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return;
    }

    if (lseek (rdr->file, length, SEEK_CUR) != -1) {
        rdr->offset += length;
    } else {
        jpg_error (rdr, "Failed call to lseek(): %s", strerror(errno));
    }
}

// -------------
// Memory reader
// -------------
//
// A jpg reader that reads loads the full file into memory. Will be faster in
// cases where we need to read the full file anyway. For example to print the
// full file structure.
JPG_READER_API_READ_BYTES(jpg_memory_reader_read_bytes)
{
    if (rdr->error == true) {
        return NULL;
    }

    if (rdr->offset + bytes_to_read > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return NULL;
    }

    uint8_t *curr_pos = rdr->pos;
    rdr->pos += bytes_to_read;
    rdr->offset += bytes_to_read;
    return curr_pos;
}

JPG_READER_API_ADVANCE_BYTES(jpg_memory_reader_advance_bytes)
{
    if (rdr->error == true) {
        return;
    }

    if (rdr->offset + length > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return;
    }

    rdr->pos += length;
    rdr->offset += length;
}
////////////////////////////////

bool jpg_reader_init (struct jpg_reader_t *rdr, char *path, bool from_file)
{
    bool success = true;

    if (from_file) {
        struct stat st;
        if (stat(path, &st) != 0) {
            success = false;
            printf ("Could not stat %s: %s\n", path, strerror(errno));
        }

        rdr->file_size = st.st_size;

        if (success) {
            rdr->file = open (path, O_RDONLY);
            if (rdr->file == -1) {
                success = false;
                printf ("Error opening %s: %s\n", path, strerror(errno));
            }
        }

        rdr->read_bytes = jpg_file_reader_read_bytes;
        rdr->advance_bytes = jpg_file_reader_advance_bytes;

    } else {
        rdr->data = (uint8_t*)full_file_read_full (&rdr->pool, path, &rdr->file_size, false);
        rdr->pos = rdr->data;

        rdr->read_bytes = jpg_memory_reader_read_bytes;
        rdr->advance_bytes = jpg_memory_reader_advance_bytes;
    }

    if (success) {
#define JPG_MARKER_ROW(SYMBOL,VALUE) jpg_marker_to_str_tree_insert (&rdr->marker_names, VALUE, #SYMBOL);
        JPG_MARKER_TABLE
#undef JPG_MARKER_ROW
    }

    rdr->error = !success;
    return success;
}

JPG_READER_API_READ_BYTES(jpg_read_bytes)
{
    return rdr->read_bytes(rdr, bytes_to_read);
}

JPG_READER_API_ADVANCE_BYTES(jpg_advance_bytes)
{
    rdr->advance_bytes(rdr, length);
}

// NOTE: This returns constant strings, you shouldn't try writing to or freeing
// them.
char* marker_name (struct jpg_reader_t *rdr, enum marker_t marker)
{
    return jpg_marker_to_str_get (&rdr->marker_names, marker);
}

// TODO: Make sure endianness is correct. Either make this function work for
// both endianesses or force the compiler to always use one endianess.
// :endianess_dependant
enum marker_t jpg_read_marker (struct jpg_reader_t *rdr)
{
    enum marker_t marker = JPG_MARKER_ERR;
    uint8_t *data = jpg_read_bytes (rdr, 2);
    if (!rdr->error) {
        if (data[0] == 0xFF &&
            ((data[1] & 0xF0) == 0xC0 || (data[1] & 0xF0) == 0xD0 || (data[1] & 0xF0) == 0xE0 ||
            data[1] == JPG_MARKER_COM || data[1] == JPG_MARKER_TEM)) {
            marker = ((int)data[0])<<8 | (int)data[1];
        } else {
            jpg_error (rdr, "Tried to read invalid marker '");
            str_cat_bytes (&rdr->error_msg, data, 2);
            str_cat_c (&rdr->error_msg, "'");
        }
    }

    return marker;
}

void jpg_expect_marker (struct jpg_reader_t *rdr, enum marker_t expected_marker)
{
    enum marker_t read_marker = jpg_read_marker (rdr);
    if (!rdr->error) {
        if (read_marker != expected_marker) {
            jpg_error (rdr, "Expected marker '%s' got: %s",
                       marker_name(rdr, expected_marker),
                       marker_name(rdr, read_marker));
        }
    }
}

// Hopefully in the optimized build these loops are unrolled?
uint64_t jpg_reader_read_value (struct jpg_reader_t *rdr, int value_size)
{
    assert (value_size < 8);

    uint64_t value = 0;
    uint8_t *data = jpg_read_bytes (rdr, value_size);
    if (rdr->endianess == BYTE_READER_LITTLE_ENDIAN) {
        if (!rdr->error) {
            for (int i=value_size-1; i>=0; i--) {
                value |= (uint64_t)data[i];
                if (i > 0) {
                    value <<= 8;
                }
            }
        }

    } else { // if (rdr->endianess == BYTE_READER_BIG_ENDIAN) {
        if (!rdr->error) {
            for (int i=0; i<value_size; i++) {
                value |= (uint64_t)data[i];
                if (i < value_size - 1) {
                    value <<= 8;
                }
            }
        }
    }

    return value;
}

// NOTE: Be careful not to call this after stand alone markers SOI, EOI and TEM.
// :endianess_dependant
int jpg_read_marker_segment_length (struct jpg_reader_t *rdr)
{
    assert (rdr->endianess == BYTE_READER_BIG_ENDIAN && "Attempting to read JPEG marker as little endian.");
    return jpg_reader_read_value (rdr, 2);
}

static inline
bool is_tables_misc_marker (enum marker_t marker)
{
    return marker == JPG_MARKER_DQT ||
        marker == JPG_MARKER_DHT ||
        marker == JPG_MARKER_DAC ||
        marker == JPG_MARKER_DRI ||
        marker == JPG_MARKER_COM ||
        JPG_MARKER_APP(marker);
}

static inline
bool is_scan_start (enum marker_t marker)
{
    return is_tables_misc_marker(marker) ||
        marker == JPG_MARKER_SOS;
}

void print_jpeg_structure (char *path)
{
    bool success = true;

    printf ("Reading: %s\n", path);

    struct jpg_reader_t _rdr = {0};
    struct jpg_reader_t *rdr = &_rdr;
    jpg_reader_init (rdr, path, false);

    if (success) {
        jpg_expect_marker (rdr, JPG_MARKER_SOI);

        // Frame table-specification and miscellaneous marker segments
        enum marker_t marker = jpg_read_marker (rdr);
        if (marker == JPG_MARKER_APP0) {
            printf ("File seems to be JFIF\n");
        } else if (marker == JPG_MARKER_APP1) {
            printf ("File seems to be EXIF\n");
        }
        printf ("\n");

        {
            bool first = true;
            while (is_tables_misc_marker(marker)) {
                if (first) {
                    printf ("Tables/misc.\n");
                    first = false;
                }

                printf (" %s\n", marker_name(rdr, marker));
                int marker_segment_length = jpg_read_marker_segment_length (rdr);
                jpg_advance_bytes (rdr, marker_segment_length - 2);

                marker = jpg_read_marker (rdr);
            }
        }

        // Frame header
        if (JPG_MARKER_SOF(marker)) {
            printf ("%s\n", marker_name(rdr, marker));
            int marker_segment_length = jpg_read_marker_segment_length (rdr);
            jpg_advance_bytes (rdr, marker_segment_length - 2);
        } else {
            jpg_error (rdr, "Expected SOF marker, got '%s'", marker_name(rdr, marker));
        } 

        // Read Scans
        // TODO: From here on we pretty much need to read the full file.
        // Probably ther fastest thing to do is to try to load the rest, so we
        // avoid making a lot of unnecessary system calls.
        uint64_t scan_count = 1;
        marker = jpg_read_marker (rdr);
        while (!rdr->error && is_scan_start(marker)) {
            printf ("Scan %lu\n", scan_count);

            {
                bool first = true;
                while (is_tables_misc_marker(marker))
                {
                    if (first) {
                        printf (" Tables/misc.\n");
                        first = false;
                    }

                    printf ("  %s\n", marker_name(rdr, marker));
                    int marker_segment_length = jpg_read_marker_segment_length (rdr);
                    jpg_advance_bytes (rdr, marker_segment_length - 2);

                    marker = jpg_read_marker (rdr);
                }
            }

            if (marker == JPG_MARKER_SOS) {
                printf (" %s\n", marker_name(rdr, marker));
                int marker_segment_length = jpg_read_marker_segment_length (rdr);
                jpg_advance_bytes (rdr, marker_segment_length - 2);
            } else {
                jpg_error (rdr, "Expected SOS marker, got '%s'", marker_name(rdr, marker));
            } 

            uint64_t ecs_count = 0;
            uint64_t rst_check = 0;
            uint64_t rst_errors_found = 0;
            while (!rdr->error) {
                // Process ECS block
                uint64_t buffer = 0x0;
                while (!rdr->error &&
                       ((buffer & 0xFF00) != 0xFF00 || (buffer & 0xFF) == 0x0))
                {
                    // @performance
                    // Avoiding the function pointer call makes
                    // processing ~4x faster.
                    //
                    // Equivalent to:
                    // data = jpg_read_bytes (rdr, 1);
                    uint8_t *data = rdr->pos;
                    rdr->pos++;

                    buffer <<= 8;
                    buffer |= *data;
                }
                ecs_count++;

                // Handle RST marker or end ECS segment processing.
                marker = 0xFFFF & buffer;
                if (JPG_MARKER_RST(marker)) {
                    if ((marker ^ JPG_MARKER_RST0) != rst_check) {
                        rst_errors_found++;
                    }
                } else {
                    break;
                }

                rst_check = (rst_check+1) % 8;
            }

            printf (" ECS (%lu)", ecs_count);
            if (rst_errors_found > 0) {
                printf ("- errors %lu\n", rst_errors_found);
            } else {
                printf ("\n");
            }

            if (marker == JPG_MARKER_EOI) {
                printf ("EOI\n");
                break;
            } 

            scan_count++;
        }

        if (marker != JPG_MARKER_EOI) {
            jpg_error (rdr, "Expected marker EOI got: %s", marker_name(rdr, marker));
        } 


        if (rdr->error) {
            printf (ECMA_RED("error:") " %s\n", str_data(&rdr->error_msg));
        }

        jpg_reader_destroy (rdr);
    }
}

#define TIFF_TYPE_TABLE \
    TIFF_TYPE_ROW(NONE,  0) /*non-standard, used to detect errors*/ \
    TIFF_TYPE_ROW(BYTE,  1)    \
    TIFF_TYPE_ROW(ASCII, 2)    \
    TIFF_TYPE_ROW(SHORT, 3)    \
    TIFF_TYPE_ROW(LONG,  4)    \
    TIFF_TYPE_ROW(RATIONAL, 5) \

#define TIFF_TYPE_ROW(SYMBOL,VALUE) TIFF_TYPE_ ## SYMBOL = VALUE,
enum tiff_type_t {
    TIFF_TYPE_TABLE
};
#undef TIFF_TYPE_ROW

#define TIFF_TYPE_ROW(SYMBOL,VALUE) #SYMBOL,
char *tiff_type_names[] = {
    TIFF_TYPE_TABLE
};
#undef TIFF_TYPE_ROW


void print_tiff_6 (struct jpg_reader_t *rdr)
{
    uint64_t tiff_data_start = rdr->offset;
    enum jpg_reader_endianess_t original_endianess = rdr->endianess;

    printf ("Reading TIFF data:\n");
    uint8_t *byte_order = jpg_read_bytes (rdr, 2);
    if (memcmp (byte_order, "II", 2) == 0) {
        printf (" Byte order: II (little endian)\n");
        rdr->endianess = BYTE_READER_LITTLE_ENDIAN;
    } else if (memcmp (byte_order, "MM", 2) == 0) {
        printf (" Byte order: MM (big endian)\n");
        rdr->endianess = BYTE_READER_BIG_ENDIAN;
    } else {
        jpg_error (rdr, "Invalid byte order, expected 'II' or 'MM', got ");
        str_cat_bytes (&rdr->error_msg, byte_order, 2);
    }

    uint64_t arbitraryliy_chosen_value = jpg_reader_read_value (rdr, 2);
    if (arbitraryliy_chosen_value != 42) {
        jpg_error (rdr, "Expected the arbitrary but carefully chosen number 42, but got %lu.",
                   arbitraryliy_chosen_value);
    }

    int ifd_count = 0;
    uint64_t directory_offset = jpg_reader_read_value (rdr, 4);
    while (!rdr->error && directory_offset != 0) {
        jpg_advance_bytes (rdr, directory_offset - (rdr->offset - tiff_data_start));
        printf (" IFD %d @%lu\n", ifd_count, directory_offset);

        int directory_entry_count = 0;
        int num_directory_entries = jpg_reader_read_value (rdr, 2);
        while (!rdr->error && directory_entry_count < num_directory_entries) {
            directory_entry_count++;
            printf ("  %d.", directory_entry_count);

            uint64_t tag = jpg_reader_read_value (rdr, 2);
            printf (" 0x%lX :", tag);

            uint64_t type = jpg_reader_read_value (rdr, 2);
            if (type > TIFF_TYPE_RATIONAL) {
                type = TIFF_TYPE_NONE;
            }
            printf (" %s (0x%lX)", tiff_type_names[type], type);

            uint64_t num_values = jpg_reader_read_value (rdr, 4);
            printf (" #%lu", num_values);

            uint64_t value_offset = jpg_reader_read_value (rdr, 4);
            printf (" @%lu", value_offset);

            printf ("\n");
        }

        directory_offset = jpg_reader_read_value (rdr, 4);
        ifd_count++;
    }

    // Restore endianess
    rdr->endianess = original_endianess;
}

void print_exif (char *path)
{
    bool success = true;

    printf ("Reading: %s\n", path);

    struct jpg_reader_t _rdr = {0};
    struct jpg_reader_t *rdr = &_rdr;
    jpg_reader_init (rdr, path, true);

    if (success) {
        jpg_expect_marker (rdr, JPG_MARKER_SOI);

        // Frame table-specification and miscellaneous marker segments
        bool is_exif = false;
        bool is_jfif = false;
        enum marker_t marker = jpg_read_marker (rdr);
        if (marker == JPG_MARKER_APP0) {
            is_jfif = true;
        } else if (marker == JPG_MARKER_APP1) {
            is_exif = true;
        }

        if (is_jfif || !is_exif) {
            // Do exhaustive search? Maybe some broken implementation put the
            // APP1 segment not at the beginning. Looks like the IJG's JPEG
            // implementation did this for a while [1].
            //
            // [1] http://sylvana.net/jpegcrop/exifpatch.html
        } else {
            int marker_segment_length = jpg_read_marker_segment_length (rdr);

            uint8_t *exif_id_code = jpg_read_bytes (rdr, 6);
            if (memcmp (exif_id_code, "Exif\0\0", 6) == 0) {
                printf ("Found Exif APP1 marker segment\n");
                print_tiff_6 (rdr);
            }
        }

        //// Ignore the rest of the Table/misc. marker segments
        //{
        //    while (is_tables_misc_marker(marker)) {
        //        int marker_segment_length = jpg_read_marker_segment_length (rdr);
        //        jpg_advance_bytes (rdr, marker_segment_length - 2);

        //        marker = jpg_read_marker (rdr);
        //    }
        //}

        //// Frame header
        //if (JPG_MARKER_SOF(marker)) {
        //    int marker_segment_length = jpg_read_marker_segment_length (rdr);
        //    jpg_advance_bytes (rdr, marker_segment_length - 2);
        //} else {
        //    jpg_error (rdr, "Expected SOF marker, got '%s'", marker_name(rdr, marker));
        //} 

        if (rdr->error) {
            printf (ECMA_RED("error:") " %s\n", str_data(&rdr->error_msg));
        }

        jpg_reader_destroy (rdr);
    }
}
