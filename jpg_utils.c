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

void print_bytes (uint8_t *bytes, uint64_t len)
{
    while (len !=0) {
        printf ("%02X", *bytes);

        len--;
        if (len != 0) {
            printf (" ");
        }
        bytes++;
    }
    printf ("\n");
}

uint8_t* bin_data (string_t *str)
{
    return (uint8_t*)str_data(str);
}

BINARY_TREE_NEW(jpg_marker_to_str, int, char*, a-b);
struct jpg_reader_t {
    int file;
    string_t buff;

    struct jpg_marker_to_str_tree_t marker_names;

    bool error;
    string_t error_msg;
};

void jpg_reader_destroy (struct jpg_reader_t *rdr)
{
    str_free (&rdr->error_msg);
    str_free (&rdr->buff);
    jpg_marker_to_str_tree_destroy (&rdr->marker_names);

    if (rdr->file != 0) {
        close (rdr->file);
    }
}

#define JPG_MARKER_TABLE \
    JPG_MARKER_ROW(ERR,   0x0000) /* Non standard, used to identify errors.*/ \
                                   \
    JPG_MARKER_ROW(SOF0,  0xFFC0)  \
    JPG_MARKER_ROW(SOF1,  0xFFC1)  \
    JPG_MARKER_ROW(SOF2,  0xFFC2)  \
    JPG_MARKER_ROW(SOF3,  0xFFC3)  \
    JPG_MARKER_ROW(SOF4,  0xFFC4)  \
    JPG_MARKER_ROW(SOF5,  0xFFC5)  \
    JPG_MARKER_ROW(SOF6,  0xFFC6)  \
    JPG_MARKER_ROW(SOF7,  0xFFC7)  \
    JPG_MARKER_ROW(SOF8,  0xFFC8)  \
    JPG_MARKER_ROW(SOF9,  0xFFC9)  \
    JPG_MARKER_ROW(SOF10, 0xFFC10) \
    JPG_MARKER_ROW(SOF11, 0xFFC11) \
    JPG_MARKER_ROW(SOF12, 0xFFC12) \
    JPG_MARKER_ROW(SOF13, 0xFFC13) \
    JPG_MARKER_ROW(SOF14, 0xFFC14) \
    JPG_MARKER_ROW(SOF15, 0xFFC15) \
                                   \
    JPG_MARKER_ROW(DHT, 0xFFC4)    \
                                   \
    JPG_MARKER_ROW(DAC, 0xFFCC)    \
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
    JPG_MARKER_ROW(SOI, 0xFFD8)    \
    JPG_MARKER_ROW(EOI, 0xFFD9)    \
    JPG_MARKER_ROW(SOS, 0xFFDA)    \
    JPG_MARKER_ROW(DQT, 0xFFDB)    \
    JPG_MARKER_ROW(DNL, 0xFFDC)    \
    JPG_MARKER_ROW(DRI, 0xFFDD)    \
    JPG_MARKER_ROW(DHP, 0xFFDE)    \
    JPG_MARKER_ROW(EXP, 0xFFDF)    \
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
    JPG_MARKER_ROW(APP10, 0xFFE10) \
    JPG_MARKER_ROW(APP11, 0xFFE11) \
    JPG_MARKER_ROW(APP12, 0xFFE12) \
    JPG_MARKER_ROW(APP13, 0xFFE13) \
    JPG_MARKER_ROW(APP14, 0xFFE14) \
    JPG_MARKER_ROW(APP15, 0xFFE15) \
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
#define JPG_MARKER_SOF(marker) ((marker & 0xFFF0) == JPG_MARKER_SOF0)

bool jpg_reader_init (struct jpg_reader_t *rdr, char *path)
{
    bool success = true;

    struct stat st;
    if (stat(path, &st) != 0) {
        success = false;
        printf ("Could not stat %s: %s\n", path, strerror(errno));
    }

    if (success) {
        rdr->file = open (path, O_RDONLY);
        if (rdr->file == -1) {
            success = false;
            printf ("Error opening %s: %s\n", path, strerror(errno));
        }
    }

    if (success) {
#define JPG_MARKER_ROW(SYMBOL,VALUE) jpg_marker_to_str_tree_insert (&rdr->marker_names, VALUE, #SYMBOL);
        JPG_MARKER_TABLE
#undef JPG_MARKER_ROW
    }

    rdr->error = !success;
    return success;
}

void jpg_read_bytes (struct jpg_reader_t *rdr, uint64_t bytes_to_read)
{
    if (rdr->error == true) {
        return;
    }

    if (!file_read_bytes (rdr->file, bytes_to_read, &rdr->buff)) {
        rdr->error = true;
        printf ("Read error.\n");
    }
}

// NOTE: This returns constant strings, you shouldn't try writing to or freeing
// them.
char* jpg_get_marker_name (struct jpg_reader_t *rdr, enum marker_t marker)
{
    return jpg_marker_to_str_get (&rdr->marker_names, marker);
}

void jpg_expected_marker_error (struct jpg_reader_t *rdr, enum marker_t marker)
{
    if (rdr->error == false) {
        return;
    }

    rdr->error = true;
    printf ("Expected marker: %s\n", jpg_get_marker_name(rdr, marker));
}

// TODO: Make sure endianness is correct. Either make this function work for
// both endianesses or force the compiler to always use one endianess.
// :endianess_dependant
enum marker_t jpg_read_marker (struct jpg_reader_t *rdr)
{
    enum marker_t marker = JPG_MARKER_ERR;
    jpg_read_bytes (rdr, 2);
    if (!rdr->error) {
        uint8_t *data = bin_data(&rdr->buff);
        if (data[0] == 0xFF) {
            marker = ((int)data[0])<<8 | (int)data[1];
        }
    }

    return marker;
}

void jpg_expect_marker (struct jpg_reader_t *rdr, enum marker_t expected_marker)
{
    enum marker_t read_marker = jpg_read_marker (rdr);
    if (!rdr->error) {
        if (read_marker != expected_marker) {
            jpg_expected_marker_error (rdr, expected_marker);
        } else {
            printf ("Found expected marker: %s\n", jpg_get_marker_name(rdr, expected_marker));
        }
    }
}

void jpg_read (char *path)
{
    bool success = true;

    printf ("Reading: %s\n", path);

    struct jpg_reader_t rdr = {0};
    jpg_reader_init (&rdr, path);

    if (success) {
        jpg_expect_marker (&rdr, JPG_MARKER_SOI);

        // Read frame table-specification and miscellaneous marker segments
        enum marker_t marker = jpg_read_marker (&rdr);
        if (marker == JPG_MARKER_APP0) {
            printf ("JFIF\n");
        } else if (marker == JPG_MARKER_APP1) {
            printf ("EXIF\n");
        }

        //while (true) {
        //    if (marker == JPG_MARKER_DQT ||
        //        marker == JPG_MARKER_DHT ||
        //        marker == JPG_MARKER_DAC ||
        //        marker == JPG_MARKER_DRI ||
        //        marker == JPG_MARKER_COM ||
        //        JPG_MARKER_APP(marker))
        //    {
        //        // Skip marker segments
        //    } else {
        //        break;
        //    }

        //    marker = jpg_read_marker (&rdr);
        //}

        //if (JPG_MARKER_SOF(marker)) {
        //    jpg_expected_marker_error (&rdr, JPG_MARKER_SOF0);
        //} 

        // Read frame header

        // Read Scans

        //jpg_expect_marker (&rdr, JPG_MARKER_EOI);

        jpg_reader_destroy (&rdr);
    }
}
