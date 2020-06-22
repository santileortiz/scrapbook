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

#define JPG_READER_API_JUMP_TO(name) \
    void name(struct jpg_reader_t *rdr, uint64_t offset)
typedef JPG_READER_API_JUMP_TO(jpg_reader_api_jump_to_t);

BINARY_TREE_NEW(int_to_str, int, char*, a-b);
struct jpg_reader_t {
    mem_pool_t pool;

    uint8_t *data;
    uint8_t *pos;

    int file;
    string_t buff;

    uint64_t file_size;
    uint64_t offset;

    enum jpg_reader_endianess_t endianess;


    struct int_to_str_tree_t marker_names;

    uint64_t exif_ifd_offset;
    uint64_t gps_ifd_offset;
    uint64_t interoperability_ifd_offset;

    bool error;
    string_t error_msg;

    jpg_reader_api_read_bytes_t *read_bytes;
    jpg_reader_api_advance_bytes_t *advance_bytes;
    jpg_reader_api_jump_to_t *jump_to;
};

void jpg_reader_destroy (struct jpg_reader_t *rdr)
{
    str_free (&rdr->error_msg);
    str_free (&rdr->buff);
    int_to_str_tree_destroy (&rdr->marker_names);

    if (rdr->file != 0) {
        close (rdr->file);
    }

    mem_pool_destroy (&rdr->pool);
}

//////////////////////////////
// JPEG constants

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

//////////////////////////////
// TIFF constants

#define TIFF_TYPE_TABLE \
    TIFF_TYPE_ROW(NONE,      0, 0, void) /*non-standard, used to detect errors*/ \
    TIFF_TYPE_ROW(BYTE,      1, 1, uint8_t)\
    TIFF_TYPE_ROW(ASCII,     2, 1, char*)\
    TIFF_TYPE_ROW(SHORT,     3, 2, uint16_t)\
    TIFF_TYPE_ROW(LONG,      4, 4, uint32_t)\
    TIFF_TYPE_ROW(RATIONAL,  5, 8, struct tiff_type_RATIONAL_t)\
    TIFF_TYPE_ROW(SBYTE,     6, 1, int8_t)\
    TIFF_TYPE_ROW(UNDEFINED, 7, 1, uint8_t)\
    TIFF_TYPE_ROW(SSHORT,    8, 2, int16_t)\
    TIFF_TYPE_ROW(SLONG,     9, 4, int32_t)\
    TIFF_TYPE_ROW(SRATIONAL,10, 8, struct tiff_type_SRATIONAL_t)\
    TIFF_TYPE_ROW(FLOAT,    11, 4, float)\
    TIFF_TYPE_ROW(DOUBLE,   12, 8, double)\

#define TIFF_TYPE_ROW(SYMBOL,VALUE,BYTES, C_TYPE) TIFF_TYPE_ ## SYMBOL = VALUE,
enum tiff_type_t {
    TIFF_TYPE_TABLE
};
#undef TIFF_TYPE_ROW

#define TIFF_TYPE_ROW(SYMBOL,VALUE,BYTES, C_TYPE) #SYMBOL,
char *tiff_type_names[] = {
    TIFF_TYPE_TABLE
};
#undef TIFF_TYPE_ROW

#define TIFF_TYPE_ROW(SYMBOL,VALUE,BYTES, C_TYPE) BYTES,
unsigned int tiff_type_sizes[] = {
    TIFF_TYPE_TABLE
};
#undef TIFF_TYPE_ROW

//#define TIFF_TYPE_ROW(SYMBOL,VALUE,BYTES, C_TYPE) BYTES,
//unsigned int tiff_type_sizes[] = {
//    TIFF_TYPE_TABLE
//};
//#undef TIFF_TYPE_ROW

struct tiff_type_RATIONAL_t {
    uint32_t num;
    uint32_t den;
};

struct tiff_type_SRATIONAL_t {
    int32_t num;
    int32_t den;
};


// NOTE: Count of -1 means 'Any' size, used for null terminated strings.
// TODO: Currently only contaíns those used by Exif.
// TODO: Count for StripOffsets and StripByteCounts depends on the image data
// format. This isn't implemented.
#define TIFF_TAG_TABLE \
    TIFF_TAG_ROW(NONE,  0, ARR(), 0) /*non-standard, used to detect errors*/ \
    TIFF_TAG_ROW(ImageWidth,                  0x100, ARR(SHORT, LONG), 1)    \
    TIFF_TAG_ROW(ImageLength,                 0x101, ARR(SHORT, LONG), 1)    \
    TIFF_TAG_ROW(BitsPerSample,               0x102, ARR(SHORT)      , 3)    \
    TIFF_TAG_ROW(Compression,                 0x103, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(PhotometricInterpretation,   0x106, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(Orientation,                 0x112, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(SamplesPerPixel,             0x115, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(PlanarConfiguration,         0x11C, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(YCbCrSubSampling,            0x212, ARR(SHORT)      , 2)    \
    TIFF_TAG_ROW(YCbCrPositioning,            0x213, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(XResolution,                 0x11A, ARR(RATIONAL)   , 1)    \
    TIFF_TAG_ROW(YResolution,                 0x11B, ARR(RATIONAL)   , 1)    \
    TIFF_TAG_ROW(ResolutionUnit,              0x128, ARR(SHORT)      , 1)    \
    TIFF_TAG_ROW(StripOffsets,                0x111, ARR(SHORT, LONG),-1)    \
    TIFF_TAG_ROW(RowsPerStrip,                0x116, ARR(SHORT, LONG), 1)    \
    TIFF_TAG_ROW(StripByteCounts,             0x117, ARR(SHORT, LONG),-1)    \
    TIFF_TAG_ROW(JPEGInterchangeFormat,       0x201, ARR(LONG)       , 1)    \
    TIFF_TAG_ROW(JPEGInterchangeFormatLength, 0x202, ARR(LONG)       , 1)    \
    TIFF_TAG_ROW(TransferFunction,            0x12D, ARR(SHORT)      , 3*256)\
    TIFF_TAG_ROW(WhitePoint,                  0x13E, ARR(RATIONAL)   , 2)    \
    TIFF_TAG_ROW(PrimaryChromaticities,       0x13F, ARR(RATIONAL)   , 6)    \
    TIFF_TAG_ROW(YCbCrCoefficients,           0x211, ARR(RATIONAL)   , 3)    \
    TIFF_TAG_ROW(ReferenceBlackWhite,         0x214, ARR(RATIONAL)   , 6)    \
    TIFF_TAG_ROW(DateTime,                    0x132, ARR(ASCII)      , 20)   \
    TIFF_TAG_ROW(ImageDescription,            0x10E, ARR(ASCII)      ,-1)    \
    TIFF_TAG_ROW(Make,                        0x10F, ARR(ASCII)      ,-1)    \
    TIFF_TAG_ROW(Model,                       0x110, ARR(ASCII)      ,-1)    \
    TIFF_TAG_ROW(Software,                    0x131, ARR(ASCII)      ,-1)    \
    TIFF_TAG_ROW(Artist,                      0x13B, ARR(ASCII)      ,-1)    \
    TIFF_TAG_ROW(Copyright,                  0x8298, ARR(ASCII)      ,-1)    \
                                                                             \
    /*These are specific to Exif, but are located in the TIFF IFD*/          \
    TIFF_TAG_ROW(ExifIFD,                    0x8769, ARR(LONG)       , 1)    \
    TIFF_TAG_ROW(GPSIFD,                     0x8825, ARR(LONG)       , 1)    \
    TIFF_TAG_ROW(InteroperabilityIFD,        0xA005, ARR(LONG)       , 1)    \

#define TIFF_TAG_ROW(SYMBOL,VALUE,TYPE,COUNT) TIFF_TAG_ ## SYMBOL = VALUE,
enum tiff_tag_t {
    TIFF_TAG_TABLE
};
#undef TIFF_TAG_ROW

struct tiff_entry_t {
    uint32_t tag;
    enum tiff_type_t type;
    uint32_t count;

    void *value;

    // DEBUG DATA
    bool is_value_in_offset;
    uint64_t value_offset;
};

struct tiff_ifd_t {
    uint32_t entries_len;
    struct tiff_entry_t *entries;

    struct tiff_ifd_t *next;

    // DEBUG DATA
    uint64_t ifd_offset;
};

//////////////////////////////
// Exif constants

#define EXIF_IFD_TAG_TABLE \
    EXIF_TAG_ROW(Exifversion,                        0x9000, ARR(UNDEFINED),  4) \
    EXIF_TAG_ROW(FlashpixVersion,                    0xA000, ARR(UNDEFINED),  4) \
                                                                                 \
    EXIF_TAG_ROW(ColorSpace,                         0xA001, ARR(SHORT),      1) \
    EXIF_TAG_ROW(Gamma,                              0xA500, ARR(RATIONAL),   1) \
                                                                                 \
    EXIF_TAG_ROW(ComponentsConfiguration,            0x9101, ARR(UNDEFINED),  4) \
    EXIF_TAG_ROW(CompressedBitsPerPixel,             0x9102, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(PixelXDimension,                    0xA002, ARR(SHORT,LONG), 1) \
    EXIF_TAG_ROW(PixelYDimension,                    0xA003, ARR(SHORT,LONG), 1) \
                                                                                 \
    EXIF_TAG_ROW(MakerNote,                          0x927C, ARR(UNDEFINED), -1) \
    EXIF_TAG_ROW(UserComment,                        0x9286, ARR(UNDEFINED), -1) \
                                                                                 \
    EXIF_TAG_ROW(RelatedSoundFile,                   0xA004, ARR(ASCII),      13)\
                                                                                 \
    EXIF_TAG_ROW(DateTimeOriginal,                   0x9003, ARR(ASCII),      20)\
    EXIF_TAG_ROW(DateTimeDigitized,                  0x9004, ARR(ASCII),      20)\
    EXIF_TAG_ROW(OffsetTime,                         0x9010, ARR(ASCII),      7) \
    EXIF_TAG_ROW(OffsetTimeOriginal,                 0x9011, ARR(ASCII),      7) \
    EXIF_TAG_ROW(OffsetTimeDigitized,                0x9012, ARR(ASCII),      7) \
    EXIF_TAG_ROW(SubSecTime,                         0x9290, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(SubSecTimeOriginal,                 0x9291, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(SubSecTimeDigitized,                0x9292, ARR(ASCII),     -1) \
                                                                                 \
    EXIF_TAG_ROW(ExposureTime,                       0x829A, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(FNumber,                            0x829D, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(ExposureProgram,                    0x8822, ARR(SHORT),      1) \
    EXIF_TAG_ROW(SpectralSensitivity,                0x8824, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(PhotographicSensitivity,            0x8827, ARR(SHORT),     -1) \
    EXIF_TAG_ROW(OECF,                               0x8828, ARR(UNDEFINED), -1) \
    EXIF_TAG_ROW(SensitivityType,                    0x8830, ARR(SHORT),      1) \
    EXIF_TAG_ROW(StandardOutputSensitivity,          0x8831, ARR(LONG),       1) \
    EXIF_TAG_ROW(RecommendedExposureIndex,           0x8832, ARR(LONG),       1) \
    EXIF_TAG_ROW(ISOSpeed,                           0x8833, ARR(LONG),       1) \
    EXIF_TAG_ROW(ISOSpeedLatitudeyyy,                0x8834, ARR(LONG),       1) \
    EXIF_TAG_ROW(ISOSpeedLatitudezzz,                0x8835, ARR(LONG),       1) \
    EXIF_TAG_ROW(ShutterSpeedValue,                  0x9201, ARR(SRATIONAL),  1) \
    EXIF_TAG_ROW(ApertureValue,                      0x9202, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(BrightnessValue,                    0x9203, ARR(SRATIONAL),  1) \
    EXIF_TAG_ROW(ExposureBiasValue,                  0x9204, ARR(SRATIONAL),  1) \
    EXIF_TAG_ROW(MaxApertureValue,                   0x9205, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(SubjectDistance,                    0x9206, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(MeteringMode,                       0x9207, ARR(SHORT),      1) \
    EXIF_TAG_ROW(LightSource,                        0x9208, ARR(SHORT),      1) \
    EXIF_TAG_ROW(Flash,                              0x9209, ARR(SHORT),      1) \
    EXIF_TAG_ROW(FocalLength,                        0x920A, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(SubjectArea,                        0x9214, ARR(SHORT),     -1 /*2 or 3 or 4*/) \
    EXIF_TAG_ROW(FlashEnergy,                        0xA20B, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(SpatialFrequencyResponse,           0xA20C, ARR(UNDEFINED), -1) \
    EXIF_TAG_ROW(FocalPlaneXResolution,              0xA20E, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(FocalPlaneYResolution,              0xA20F, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(FocalPlaneResolutionUnit,           0xA210, ARR(SHORT),      1) \
    EXIF_TAG_ROW(SubjectLocation,                    0xA214, ARR(SHORT),      2) \
    EXIF_TAG_ROW(ExposureIndex,                      0xA215, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(SensingMethod,                      0xA217, ARR(SHORT),      1) \
    EXIF_TAG_ROW(FileSource,                         0xA300, ARR(UNDEFINED),  1) \
    EXIF_TAG_ROW(SceneType,                          0xA301, ARR(UNDEFINED),  1) \
    EXIF_TAG_ROW(CFAPattern,                         0xA302, ARR(UNDEFINED), -1) \
    EXIF_TAG_ROW(CustomRendered,                     0xA401, ARR(SHORT),      1) \
    EXIF_TAG_ROW(ExposureMode,                       0xA402, ARR(SHORT),      1) \
    EXIF_TAG_ROW(WhiteBalance,                       0xA403, ARR(SHORT),      1) \
    EXIF_TAG_ROW(DigitalZoomRatio,                   0xA404, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(FocalLengthIn35mmFilm,              0xA405, ARR(SHORT),      1) \
    EXIF_TAG_ROW(SceneCaptureType,                   0xA406, ARR(SHORT),      1) \
    EXIF_TAG_ROW(GainControl,                        0xA407, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(Contrast,                           0xA408, ARR(SHORT),      1) \
    EXIF_TAG_ROW(Saturation,                         0xA409, ARR(SHORT),      1) \
    EXIF_TAG_ROW(Sharpness,                          0xA40A, ARR(SHORT),      1) \
    EXIF_TAG_ROW(DeviceSettingDescription,           0xA40B, ARR(UNDEFINED), -1) \
    EXIF_TAG_ROW(SubjectDistanceRange,               0xA40C, ARR(SHORT),      1) \
    EXIF_TAG_ROW(CompositeImage,                     0xA460, ARR(SHORT),      1) \
    EXIF_TAG_ROW(SourceImageNumberOfCompositeImage,  0xA461, ARR(SHORT),      2) \
    EXIF_TAG_ROW(SourceExposureTimesOfCompositeImage,0xA462, ARR(UNDEFINED), -1) \
                                                                                 \
    EXIF_TAG_ROW(Temperature,                        0x9400, ARR(SRATIONAL),  1) \
    EXIF_TAG_ROW(Humidity,                           0x9401, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(Pressure,                           0x9402, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(WaterDepth,                         0x9403, ARR(SRATIONAL),  1) \
    EXIF_TAG_ROW(Acceleration,                       0x9404, ARR(RATIONAL),   1) \
    EXIF_TAG_ROW(CameraElevationAngle,               0x9405, ARR(SRATIONAL),  1) \
                                                                                 \
    EXIF_TAG_ROW(ImageUniqueID,                      0xA420, ARR(ASCII),      33)\
    EXIF_TAG_ROW(CameraOwnerName,                    0xA430, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(BodySerialNumber,                   0xA431, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(LensSpecification,                  0xA432, ARR(RATIONAL),   4) \
    EXIF_TAG_ROW(LensMake,                           0xA433, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(LensModel,                          0xA434, ARR(ASCII),     -1) \
    EXIF_TAG_ROW(LensSerialNumber,                   0xA435, ARR(ASCII),     -1) \

#define EXIF_GPS_TAG_TABLE \
    EXIF_TAG_ROW(GPSVersionID,         0x00, ARR(BYTE),      4) \
    EXIF_TAG_ROW(GPSLatitudeRef,       0x01, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSLatitude,          0x02, ARR(RATIONAL),  3) \
    EXIF_TAG_ROW(GPSLongitudeRef,      0x03, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSLongitude,         0x04, ARR(RATIONAL),  3) \
    EXIF_TAG_ROW(GPSAltitudeRef,       0x05, ARR(BYTE),      1) \
    EXIF_TAG_ROW(GPSAltitude,          0x06, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSTimeStamp,         0x07, ARR(RATIONAL),  3) \
    EXIF_TAG_ROW(GPSSatellites,        0x08, ARR(ASCII),    -1) \
    EXIF_TAG_ROW(GPSStatus,            0x09, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSMeasureMode,       0x0A, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSDOP,               0x0B, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSSpeedRef,          0x0C, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSSpeed,             0x0D, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSTrackRef,          0x0E, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSTrack,             0x0F, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSImgDirectionRef,   0x10, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSImgDirection,      0x11, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSMapDatum,          0x12, ARR(ASCII),    -1) \
    EXIF_TAG_ROW(GPSDestLatitudeRef,   0x13, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSDestLatitude,      0x14, ARR(RATIONAL),  3) \
    EXIF_TAG_ROW(GPSDestLongitudeRef,  0x15, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSDestLongitude,     0x16, ARR(RATIONAL),  3) \
    EXIF_TAG_ROW(GPSDestBearingRef,    0x17, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSDestBearing,       0x18, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSDestDistanceRef,   0x19, ARR(ASCII),     2) \
    EXIF_TAG_ROW(GPSDestDistance,      0x1A, ARR(RATIONAL),  1) \
    EXIF_TAG_ROW(GPSProcessingMethod,  0x1B, ARR(UNDEFINED),-1) \
    EXIF_TAG_ROW(GPSAreaInformation,   0x1C, ARR(UNDEFINED),-1) \
    EXIF_TAG_ROW(GPSDateStamp,         0x1D, ARR(ASCII),     11)\
    EXIF_TAG_ROW(GPSDifferential,      0x1E, ARR(SHORT),     1) \
    EXIF_TAG_ROW(GPSHPositioningError, 0x1F, ARR(RATIONAL),  1) \

#define EXIF_TAG_ROW(SYMBOL,VALUE,TYPE,COUNT) EXIF_TAG_ ## SYMBOL = VALUE,
enum exif_tag_t {
    EXIF_IFD_TAG_TABLE
    EXIF_GPS_TAG_TABLE
};
#undef EXIF_TAG_ROW

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

JPG_READER_API_JUMP_TO(jpg_file_reader_jump_to)
{
    if (rdr->error == true) {
        return;
    }

    if (offset > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return;
    }

    if (lseek (rdr->file, offset, SEEK_SET) != -1) {
        rdr->offset = offset;
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

JPG_READER_API_JUMP_TO(jpg_memory_reader_jump_to)
{
    if (rdr->error == true) {
        return;
    }

    if (offset > rdr->file_size) {
        jpg_error (rdr, "Trying to read past EOF");
        return;
    }

    rdr->pos = rdr->data + offset;
    rdr->offset = offset;
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
        rdr->jump_to = jpg_file_reader_jump_to;

    } else {
        rdr->data = (uint8_t*)full_file_read_full (&rdr->pool, path, &rdr->file_size, false);
        rdr->pos = rdr->data;

        rdr->read_bytes = jpg_memory_reader_read_bytes;
        rdr->advance_bytes = jpg_memory_reader_advance_bytes;
        rdr->jump_to = jpg_memory_reader_jump_to;
    }

    if (success) {
#define JPG_MARKER_ROW(SYMBOL,VALUE) \
        int_to_str_tree_insert (&rdr->marker_names, VALUE, #SYMBOL);
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

JPG_READER_API_JUMP_TO(jpg_jump_to)
{
    rdr->jump_to(rdr, offset);
}

// NOTE: This returns constant strings, you shouldn't try writing to or freeing
// them.
char* marker_name (struct jpg_reader_t *rdr, enum marker_t marker)
{
    return int_to_str_get (&rdr->marker_names, marker);
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
#define DEFINE_BYTE_ARRAY_TO_VALUE(FUNC_NAME,C_TYPE) \
static inline                                                                              \
C_TYPE FUNC_NAME (uint8_t *bytes, int bytes_len, enum jpg_reader_endianess_t endianess)    \
{                                                                                          \
    assert (bytes_len <= sizeof(C_TYPE));                                                  \
                                                                                           \
    C_TYPE value = 0;                                                                      \
    if (endianess == BYTE_READER_LITTLE_ENDIAN) {                                          \
        for (int i=bytes_len-1; i>=0; i--) {                                               \
            value |= (C_TYPE)bytes[i];                                                     \
            if (i > 0) {                                                                   \
                value <<= 8;                                                               \
            }                                                                              \
        }                                                                                  \
    } else { /*endianess == BYTE_READER_BIG_ENDIAN*/                                       \
        for (int i=0; i<bytes_len; i++) {                                                  \
            value |= (C_TYPE)bytes[i];                                                     \
            if (i < bytes_len - 1) {                                                       \
                value <<= 8;                                                               \
            }                                                                              \
        }                                                                                  \
    }                                                                                      \
                                                                                           \
    return value;                                                                          \
}

DEFINE_BYTE_ARRAY_TO_VALUE(byte_array_to_value_u64, uint64_t);
DEFINE_BYTE_ARRAY_TO_VALUE(byte_array_to_value_u32, uint32_t);
DEFINE_BYTE_ARRAY_TO_VALUE(byte_array_to_value_u16, uint16_t);

// NOTE: If the passed array is not a multiple of the function's value type
// size, the array is created of the same size but the last value will most
// likely have garbage because the loop stops before reading the trailing bytes.
// TODO: Should we assert that the passed byte lenght is always the correct
// size?.
#define DEFINE_BYTE_ARRAY_TO_VALUE_ARRAY(FUNC_NAME,BIT_COUNT) \
static inline                                                                              \
void* FUNC_NAME (mem_pool_t *pool,                                                         \
                 uint8_t *bytes, int bytes_len, enum jpg_reader_endianess_t endianess)     \
{                                                                                          \
    uint ## BIT_COUNT ## _t *value_array =                                                 \
        mem_pool_push_array (pool, bytes_len/(BIT_COUNT/8), uint ## BIT_COUNT ## _t);      \
                                                                                           \
    uint64_t byte_count = 0;                                                               \
    uint64_t value_count = 0;                                                              \
    for (;                                                                                 \
         byte_count < bytes_len;                                                           \
         value_count++, byte_count+=(BIT_COUNT/8))                                         \
    {                                                                                      \
        value_array[value_count] =                                                         \
            byte_array_to_value_u ## BIT_COUNT (bytes+byte_count, BIT_COUNT/8, endianess); \
    }                                                                                      \
    return value_array;                                                                    \
}

DEFINE_BYTE_ARRAY_TO_VALUE_ARRAY(byte_array_to_value_array_u64, 64);
DEFINE_BYTE_ARRAY_TO_VALUE_ARRAY(byte_array_to_value_array_u32, 32);
DEFINE_BYTE_ARRAY_TO_VALUE_ARRAY(byte_array_to_value_array_u16, 16);

uint64_t jpg_reader_read_value (struct jpg_reader_t *rdr, int value_size)
{
    assert (value_size <= 8);

    uint64_t value = 0;
    uint8_t *data = jpg_read_bytes (rdr, value_size);
    if (!rdr->error) {
        value = byte_array_to_value_u64 (data, value_size, rdr->endianess);
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
            printf ("File seems to be Exif\n");
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

// This function takes a byte array representing a TIFF value and translates it
// into an array of C values, taking into account the endianess being used.
//
// NOTE: This is a compact reader, it creates an array of exactly the size
// necessary to store the passed type.
// TODO: It's possible that using data types smaller than the processor's
// register type is slower than making all arrays be for example uint64_t and
// wasting some memory on padding. For this we would need another kind of
// reader. I haven't done performance testing so I don't yet know if this could
// be really useful.
// @performance
void* tiff_read_value_data (mem_pool_t *pool,
                            uint8_t* value_data, uint32_t value_data_len,
                            enum jpg_reader_endianess_t endianess,
                            enum tiff_type_t type)
{
    void *res = NULL;

    switch (type) {
        case TIFF_TYPE_ASCII:
        case TIFF_TYPE_BYTE:
        case TIFF_TYPE_SBYTE:
        case TIFF_TYPE_UNDEFINED:
            res = pom_dup (pool, value_data, value_data_len);
            break;

        case TIFF_TYPE_SHORT:
        case TIFF_TYPE_SSHORT:
            res = byte_array_to_value_array_u16 (pool, value_data, value_data_len, endianess);
            break;

        case TIFF_TYPE_FLOAT:
        case TIFF_TYPE_LONG:
        case TIFF_TYPE_SLONG:
            res = byte_array_to_value_array_u32 (pool, value_data, value_data_len, endianess);
            break;

        case TIFF_TYPE_DOUBLE:
            res = byte_array_to_value_array_u64 (pool, value_data, value_data_len, endianess);
            break;

        case TIFF_TYPE_RATIONAL:
        case TIFF_TYPE_SRATIONAL: {
                struct tiff_type_RATIONAL_t *value_array =
                    mem_pool_push_array (pool, value_data_len/8, struct tiff_type_RATIONAL_t);
                for (uint32_t values_read = 0; values_read < value_data_len/8; values_read++, value_data+=8) {
                    value_array[values_read].num = byte_array_to_value_u32 (value_data, 4, endianess);
                    value_array[values_read].den = byte_array_to_value_u32 (value_data+4, 4, endianess);
                }
                res = value_array;
            }
            break;

        case TIFF_TYPE_NONE:
        default:
            break;
    }

    return res;
}

struct tiff_ifd_t* tiff_read_ifd (struct jpg_reader_t *rdr, mem_pool_t *pool,
                                  uint64_t tiff_data_start, uint64_t *next_ifd_offset)
{
    assert (rdr != NULL);

    mem_pool_marker_t mrkr = mem_pool_begin_temporary_memory (pool);

    struct tiff_ifd_t *ifd = mem_pool_push_struct (pool, struct tiff_ifd_t);
    *ifd = ZERO_INIT (struct tiff_ifd_t);

    ifd->ifd_offset = rdr->offset - tiff_data_start;
    ifd->entries_len = jpg_reader_read_value (rdr, 2);
    ifd->entries = mem_pool_push_array (pool, ifd->entries_len, struct tiff_entry_t);

    int directory_entry_count = 0;
    while (!rdr->error && directory_entry_count < ifd->entries_len) {
        struct tiff_entry_t *entry = &ifd->entries[directory_entry_count];
        *entry = ZERO_INIT (struct tiff_entry_t);

        directory_entry_count++;

        entry->tag = jpg_reader_read_value (rdr, 2);
        entry->type = jpg_reader_read_value (rdr, 2);
        entry->count = jpg_reader_read_value (rdr, 4);

        if (!rdr->error) {
            if (entry->type != TIFF_TYPE_NONE) {
                uint64_t byte_count = tiff_type_sizes[entry->type]*entry->count;
                if (byte_count <= 4) {
                    entry->is_value_in_offset = true;
                    entry->value_offset = rdr->offset - tiff_data_start;

                    uint8_t *value_data = jpg_read_bytes (rdr, 4);
                    entry->value = tiff_read_value_data (pool, value_data, byte_count,
                                                         rdr->endianess, entry->type);

                } else {
                    entry->value_offset = jpg_reader_read_value (rdr, 4);

                    uint64_t current_offset = rdr->offset;
                    jpg_jump_to (rdr, tiff_data_start + entry->value_offset);

                    uint8_t *value_data = jpg_read_bytes (rdr, byte_count);
                    entry->value = tiff_read_value_data (pool, value_data, byte_count,
                                                         rdr->endianess, entry->type);

                    jpg_jump_to (rdr, current_offset);
                }

            } else {
                // The value has an unknown type, we won't be able to read it.
                // We read the offset anyway, because we need to keep reading
                // the rest of the entries.
                entry->value_offset = jpg_reader_read_value (rdr, 4);
            }
        }
    }

    *next_ifd_offset = jpg_reader_read_value (rdr, 4);

    if (rdr->error) {
        mem_pool_end_temporary_memory (mrkr);
    }

    return ifd;
}

struct tiff_ifd_t* read_tiff_6 (struct jpg_reader_t *rdr, mem_pool_t *pool,
                                enum jpg_reader_endianess_t *endianess)
{
    struct tiff_ifd_t *tiff_data = NULL;
    struct tiff_ifd_t *tiff_data_end = NULL;

    uint64_t tiff_data_start = rdr->offset;
    enum jpg_reader_endianess_t original_endianess = rdr->endianess;

    uint8_t *byte_order = jpg_read_bytes (rdr, 2);
    if (!rdr->error) {
        if (memcmp (byte_order, "II", 2) == 0) {
            rdr->endianess = BYTE_READER_LITTLE_ENDIAN;
        } else if (memcmp (byte_order, "MM", 2) == 0) {
            rdr->endianess = BYTE_READER_BIG_ENDIAN;
        } else {
            jpg_error (rdr, "Invalid byte order, expected 'II' or 'MM', got ");
            str_cat_bytes (&rdr->error_msg, byte_order, 2);
        }
    }

    if (endianess != NULL) {
        *endianess = rdr->endianess;
    }

    uint64_t arbitraryliy_chosen_value = jpg_reader_read_value (rdr, 2);
    if (arbitraryliy_chosen_value != 42) {
        jpg_error (rdr, "Expected the arbitrary but carefully chosen number 42, but got %lu.",
                   arbitraryliy_chosen_value);
    }

    uint64_t next_ifd_offset = jpg_reader_read_value (rdr, 4);
    while (!rdr->error && next_ifd_offset != 0) {
        jpg_jump_to (rdr, tiff_data_start + next_ifd_offset);

        struct tiff_ifd_t *new_ifd = tiff_read_ifd (rdr, pool, tiff_data_start, &next_ifd_offset);
        LINKED_LIST_APPEND (tiff_data, new_ifd);
    }

    // Restore endianess
    rdr->endianess = original_endianess;

    return tiff_data;
}

struct tiff_tag_definitions_t {
    struct int_to_str_tree_t tiff_tag_names;

    // TODO: Still not sure if it's a good idea to split these. Did it so we
    // only lookup the corresponding tags in each IFD. This will then mark as
    // unknown a GPS IFD tag that shows up in the Exif IFD, and vice versa.
    // :split_exif_and_gps_tags
    struct int_to_str_tree_t exif_ifd_tag_names;
    struct int_to_str_tree_t gps_ifd_tag_names;
};

struct tiff_tag_definitions_t g_tiff_tag_names = {0};

void str_cat_tiff_entry_value (string_t *str, void *value, enum tiff_type_t type, uint32_t count)
{
    if (type != TIFF_TYPE_NONE) {
        str_cat_c (str, " = ");
        if (type == TIFF_TYPE_ASCII) {
            str_cat_printf (str, "\"%s\"", (char*)value);

        } else if (type == TIFF_TYPE_UNDEFINED) {
            for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                uint8_t byte = ((uint8_t*)value)[value_idx];

                // Print binary data as mix of ASCII and HEX.
                if (byte >= 0x20 && byte < 0x7F) {
                    str_cat_printf (str, " .%c", byte);
                } else {
                    str_cat_printf (str, " %.2X", byte);
                }
            }

        } else {
            str_cat_c (str, "{");
            if (type == TIFF_TYPE_BYTE) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%u", ((uint8_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_SHORT) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%u", ((uint16_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_LONG) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%u", ((uint32_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_RATIONAL) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    struct tiff_type_RATIONAL_t r = ((struct tiff_type_RATIONAL_t*)value)[value_idx];
                    str_cat_printf (str, "%u/%u", r.num, r.den);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }

            } else if (type == TIFF_TYPE_SBYTE) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%i", ((int8_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_SSHORT) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%i", ((int16_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_SLONG) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    str_cat_printf (str, "%i", ((int32_t*)value)[value_idx]);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            } else if (type == TIFF_TYPE_SRATIONAL) {
                for (uint32_t value_idx = 0; value_idx < count; value_idx++) {
                    struct tiff_type_SRATIONAL_t r = ((struct tiff_type_SRATIONAL_t*)value)[value_idx];
                    str_cat_printf (str, "%i/%i", r.num, r.den);
                    if (value_idx < count - 1) str_cat_c (str, ", ");
                }
            }
            str_cat_c (str, "}");
        }
    }
}

void str_cat_tiff_ifd (string_t *str, struct tiff_ifd_t *curr_ifd,
                       bool print_hex_values, bool print_offsets, struct int_to_str_tree_t *local_tag_names)
{
    for (int directory_entry_idx = 0; directory_entry_idx < curr_ifd->entries_len; directory_entry_idx++) {
        struct tiff_entry_t *entry = &curr_ifd->entries[directory_entry_idx];

        str_cat_c (str, "  "); // Indentation

        char* tag_name = int_to_str_get (&g_tiff_tag_names.tiff_tag_names, entry->tag);
        if (tag_name == NULL && local_tag_names != NULL) {
            tag_name = int_to_str_get (local_tag_names, entry->tag);
        }

        if (tag_name != NULL) {
            if (print_hex_values) {
                str_cat_printf (str, " %s (0x%X) :", tag_name, entry->tag);
            } else {
                str_cat_printf (str, " %s :", tag_name);
            }
        } else {
            str_cat_printf (str, " (unknown tag) 0x%X :", entry->tag);
        }

        if (entry->type <= TIFF_TYPE_DOUBLE) {
            if (print_hex_values) {
                str_cat_printf (str, " %s (0x%X)", tiff_type_names[entry->type], entry->type);
            } else {
                str_cat_printf (str, " %s", tiff_type_names[entry->type]);
            }
        } else {
            str_cat_printf (str, " (unknown type) 0x%X :", entry->type);
        }

        str_cat_printf (str, " [%u]", entry->count);

        str_cat_tiff_entry_value (str, entry->value, entry->type, entry->count);

        if (print_offsets) {
            str_cat_printf (str, " @%lu", entry->value_offset);
            if (entry->is_value_in_offset) {
                // An * besides an offset means the value wasn't in a
                // different location, instead the value_offset field was
                // used to store it. This means the offset is really the
                // offset of the value_offset field of this value.
                str_cat_c (str, "*");
            }
        }

        str_cat_c (str, "\n");
    }
}

void str_cat_tiff_ifd_offset (string_t *str, bool print_offsets, uint32_t offset)
{
    if (print_offsets) {
        str_cat_printf (str, " @%u\n", offset);
    } else {
        str_cat_c (str, "\n");
    }
}

void str_cat_tiff (string_t *str, struct tiff_ifd_t *tiff,
                   enum jpg_reader_endianess_t *endianess, struct int_to_str_tree_t *local_tag_names,
                   bool print_hex_values, bool print_offsets)
{
    str_cat_c (str, "TIFF data:\n");

    if (endianess != NULL) {
        if (*endianess == BYTE_READER_BIG_ENDIAN) {
            str_cat_c (str, " Byte order: MM (big endian)\n");
        } else {
            str_cat_c (str, " Byte order: II (little endian)\n");
        }
    }

    uint32_t ifd_count = 0;
    struct tiff_ifd_t *curr_ifd = tiff;
    while (curr_ifd != NULL) {
        str_cat_printf (str, " IFD %d", ifd_count);
        str_cat_tiff_ifd_offset (str, print_offsets, curr_ifd->ifd_offset);
        str_cat_tiff_ifd (str, curr_ifd, print_hex_values, print_offsets, local_tag_names);

        ifd_count++;
        curr_ifd = curr_ifd->next;
    }
}

void tiff_data_init ()
{
    if (g_tiff_tag_names.tiff_tag_names.num_nodes == 0) {
#define TIFF_TAG_ROW(SYMBOL,VALUE,TYPE,COUNT) \
        int_to_str_tree_insert (&g_tiff_tag_names.tiff_tag_names, VALUE, #SYMBOL);
        TIFF_TAG_TABLE
#undef TIFF_TAG_ROW

        // :split_exif_and_gps_tags
#define EXIF_TAG_ROW(SYMBOL,VALUE,TYPE,COUNT) \
        int_to_str_tree_insert (&g_tiff_tag_names.exif_ifd_tag_names, VALUE, #SYMBOL);
        EXIF_IFD_TAG_TABLE
#undef EXIF_TAG_ROW

        // :split_exif_and_gps_tags
#define EXIF_TAG_ROW(SYMBOL,VALUE,TYPE,COUNT) \
        int_to_str_tree_insert (&g_tiff_tag_names.gps_ifd_tag_names, VALUE, #SYMBOL);
        EXIF_GPS_TAG_TABLE
#undef EXIF_TAG_ROW
    }
}

void print_tiff_6 (struct jpg_reader_t *rdr)
{
    tiff_data_init ();

    mem_pool_t pool = {0};
    string_t out = {0};

    bool print_hex_values = false;
    bool print_offsets = true;

    enum jpg_reader_endianess_t endianess = BYTE_READER_BIG_ENDIAN;
    struct tiff_ifd_t *tiff = read_tiff_6 (rdr, &pool, &endianess);

    if (!rdr->error) {
        str_cat_tiff (&out, tiff, &endianess, NULL, print_hex_values, print_offsets);
        printf ("%s", str_data (&out));
    } else {
        printf (ECMA_RED("error:") " %s\n", str_data(&rdr->error_msg));
    }

    mem_pool_destroy (&pool);
    str_free (&out);
}

struct tiff_ifd_t* str_cat_tiff_ifd_at_offset (
    string_t *str,
    struct jpg_reader_t *rdr,
    mem_pool_t *pool,
    char *name, uint32_t tiff_data_start,
    bool print_offsets, uint32_t offset,
    bool print_hex_values,
    struct int_to_str_tree_t *local_tag_names)
{
    str_cat_c (str, name);
    str_cat_tiff_ifd_offset (str, print_offsets, offset);

    uint64_t next_ifd_offset;
    uint64_t current_offset = rdr->offset;
    jpg_jump_to (rdr, tiff_data_start + offset);
    struct tiff_ifd_t *ifd = tiff_read_ifd (rdr, pool, tiff_data_start, &next_ifd_offset);
    str_cat_tiff_ifd (str, ifd, print_hex_values, print_offsets, local_tag_names);
    jpg_jump_to (rdr, current_offset);

    return ifd;
}

void print_exif_as_tiff_data (struct jpg_reader_t *rdr)
{
    tiff_data_init ();

    mem_pool_t pool = {0};
    string_t out = {0};

    bool print_hex_values = false;
    bool print_offsets = true;

    uint64_t tiff_data_start = rdr->offset;
    enum jpg_reader_endianess_t endianess = BYTE_READER_BIG_ENDIAN;
    struct tiff_ifd_t *tiff = read_tiff_6 (rdr, &pool, &endianess);

    str_cat_c (&out, "TIFF data:\n");
    if (endianess == BYTE_READER_BIG_ENDIAN) {
        str_cat_c (&out, " Byte order: MM (big endian)\n");
    } else {
        str_cat_c (&out, " Byte order: II (little endian)\n");
    }

    // Lookup for the Exif specific tags.
    // @performance linear_search.
    uint32_t exif_ifd_offset = 0;
    uint32_t gps_ifd_offset = 0;
    uint32_t interoperability_ifd_offset = 0;
    {
        uint32_t ifd_count = 0;
        struct tiff_ifd_t *curr_ifd = tiff;
        while (curr_ifd != NULL) {
            str_cat_printf (&out, " IFD %d", ifd_count);
            str_cat_tiff_ifd_offset (&out, print_offsets, curr_ifd->ifd_offset);
            str_cat_tiff_ifd (&out, curr_ifd, print_hex_values, print_offsets, NULL);

            for (uint32_t entry_idx = 0; entry_idx < curr_ifd->entries_len; entry_idx++) {
                struct tiff_entry_t *entry = &curr_ifd->entries[entry_idx];

                if (entry->tag == TIFF_TAG_ExifIFD) {
                    exif_ifd_offset = *((uint32_t*)entry->value);
                } else if (entry->tag == TIFF_TAG_GPSIFD) {
                    gps_ifd_offset = *((uint32_t*)entry->value);
                } else if (entry->tag == TIFF_TAG_InteroperabilityIFD) {
                    interoperability_ifd_offset = *((uint32_t*)entry->value);
                }
            }

            ifd_count++;
            curr_ifd = curr_ifd->next;
        }
    }

    struct tiff_entry_t *maker_note = NULL;
    if (exif_ifd_offset != 0) {
        struct tiff_ifd_t *exif_ifd =
            str_cat_tiff_ifd_at_offset (&out, rdr, &pool, " Exif IFD", tiff_data_start,
                                        print_offsets, exif_ifd_offset, print_hex_values, &g_tiff_tag_names.exif_ifd_tag_names);

        // Lookup the MakerNote tag
        for (uint32_t entry_idx = 0; entry_idx < exif_ifd->entries_len; entry_idx++) {
            struct tiff_entry_t *entry = &exif_ifd->entries[entry_idx];
            if (entry->tag == EXIF_TAG_MakerNote) {
                maker_note = entry;
            }
        }
    }

    if (gps_ifd_offset != 0) {
        str_cat_tiff_ifd_at_offset (&out, rdr, &pool, " GPS IFD", tiff_data_start,
                                    print_offsets, gps_ifd_offset, print_hex_values, &g_tiff_tag_names.gps_ifd_tag_names);
    }

    if (interoperability_ifd_offset != 0) {
        str_cat_tiff_ifd_at_offset (&out, rdr, &pool, " Interoperability IFD", tiff_data_start,
                                    print_offsets, gps_ifd_offset, print_hex_values, NULL);
    }

    if (maker_note != NULL) {
        uint64_t current_offset = rdr->offset;
        jpg_jump_to (rdr, tiff_data_start + maker_note->value_offset);

        char name[10];
        uint32_t idx = 0;
        uint8_t *data = NULL;
        do {
            data = jpg_read_bytes (rdr, 1);
            name[idx] = *data;
            idx++;
        } while (!rdr->error && *data != '\0' && idx < ARRAY_SIZE(name));

        if (*data == '\0') {
            str_cat_printf (&out, " MakerNote (%s)\n", name);
            if (strcmp (name, "Apple iOS") == 0) {
                // TODO: Parse Apple's MakerNote
            }
        }

        jpg_jump_to (rdr, current_offset);
    }

    if (!rdr->error) {
        printf ("%s", str_data (&out));
    } else {
        printf (ECMA_RED("error:") " %s\n", str_data(&rdr->error_msg));
    }

    mem_pool_destroy (&pool);
    str_free (&out);
}

void print_tiff_value_data (struct jpg_reader_t *rdr,
                            uint8_t *value_data, uint64_t byte_count,
                            enum tiff_type_t type, uint64_t count)
{
    if (rdr->error || value_data == NULL) {
        return;
    }

    // TODO: Is this too slow? maybe create the string_t and pool in the caller
    // and reuse them on each call.
    // @performance
    string_t out = {0};
    mem_pool_t pool = {0};

    void *value_array = tiff_read_value_data (&pool, value_data, byte_count, rdr->endianess, type);
    str_cat_tiff_entry_value (&out, value_array, type, count);
    printf ("%s", str_data(&out));

    mem_pool_destroy (&pool);
    str_free (&out);
}

// Prints the IFD located at the current reader's position. Returns the offset
// of the next IFD
uint64_t print_tiff_ifd (struct jpg_reader_t *rdr,
                         uint64_t tiff_data_start, bool print_hex_values, bool print_offsets,
                         struct int_to_str_tree_t *local_tag_names)
{
    assert (rdr != NULL);

    int directory_entry_count = 0;
    int num_directory_entries = jpg_reader_read_value (rdr, 2);
    while (!rdr->error && directory_entry_count < num_directory_entries) {
        directory_entry_count++;
        printf ("  "); // Indentation

        uint64_t tag = jpg_reader_read_value (rdr, 2);
        char* tag_name = int_to_str_get (&g_tiff_tag_names.tiff_tag_names, tag);
        if (tag_name == NULL && local_tag_names != NULL) {
            tag_name = int_to_str_get (local_tag_names, tag);
        }

        if (tag_name != NULL) {
            if (print_hex_values) {
                printf (" %s (0x%lX) :", tag_name, tag);
            } else {
                printf (" %s :", tag_name);
            }
        } else {
            printf (" (unknown tag) 0x%lX :", tag);
        }

        // TODO: Check that this tag can be of this type
        uint64_t type = jpg_reader_read_value (rdr, 2);
        if (type <= TIFF_TYPE_DOUBLE) {
            if (print_hex_values) {
                printf (" %s (0x%lX)", tiff_type_names[type], type);
            } else {
                printf (" %s", tiff_type_names[type]);
            }
        } else {
            type = TIFF_TYPE_NONE;
            printf (" (unknown type) 0x%lX :", type);
        }

        // TODO: Check that this tag can have this count
        uint64_t count = jpg_reader_read_value (rdr, 4);
        printf (" [%lu]", count);

        int64_t value_offset = 0;
        bool is_value_in_offset = false;
        if (type != TIFF_TYPE_NONE) {
            uint64_t byte_count = tiff_type_sizes[type]*count;
            if (byte_count <= 4) {
                is_value_in_offset = true;
                value_offset = rdr->offset - tiff_data_start;

                uint8_t *value_data = jpg_read_bytes (rdr, 4);
                print_tiff_value_data (rdr, value_data, byte_count, type, count);

                if (rdr->exif_ifd_offset == 0 && tag == TIFF_TAG_ExifIFD) {
                    rdr->exif_ifd_offset = byte_array_to_value_u64 (value_data, 4, rdr->endianess);
                } else if (rdr->gps_ifd_offset == 0 && tag == TIFF_TAG_GPSIFD) {
                    rdr->gps_ifd_offset = byte_array_to_value_u64 (value_data, 4, rdr->endianess);
                } else if (rdr->interoperability_ifd_offset == 0 && tag == TIFF_TAG_InteroperabilityIFD) {
                    rdr->interoperability_ifd_offset = byte_array_to_value_u64 (value_data, 4, rdr->endianess);
                }

            } else {
                value_offset = jpg_reader_read_value (rdr, 4);

                uint64_t current_offset = rdr->offset;
                jpg_jump_to (rdr, tiff_data_start + value_offset);

                uint8_t *value_data = jpg_read_bytes (rdr, byte_count);
                print_tiff_value_data (rdr, value_data, byte_count, type, count);

                jpg_jump_to (rdr, current_offset);
            }

        } else {
            // The value has an unknown type, we won't be able to read it.
            // We read the offset anyway, because we need to keep reading
            // the rest of the entries.
            value_offset = jpg_reader_read_value (rdr, 4);
        }

        if (print_offsets) {
            printf (" @%lu", value_offset);
            if (is_value_in_offset) {
                // An * besides an offset means the value wasn't in a
                // different location, instead the value_offset field was
                // used to store it. This means the offset is really the
                // offset of the value_offset field of this value.
                printf ("*");
            }
        }

        printf ("\n");
    }

    return jpg_reader_read_value (rdr, 4);
}

void print_exif_as_tiff_data_no_ir (struct jpg_reader_t *rdr)
{
    tiff_data_init ();

    uint64_t tiff_data_start = rdr->offset;
    enum jpg_reader_endianess_t original_endianess = rdr->endianess;

    printf ("TIFF data:\n");
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

    bool print_hex_values = false;
    bool print_offsets = true;
    int ifd_count = 0;
    uint64_t next_ifd_offset = jpg_reader_read_value (rdr, 4);
    while (!rdr->error && next_ifd_offset != 0) {
        jpg_jump_to (rdr, tiff_data_start + next_ifd_offset);

        printf (" IFD %d", ifd_count);
        if (print_offsets) {
            printf (" @%lu\n", next_ifd_offset);
        } else {
            printf ("\n");
        }
        next_ifd_offset = print_tiff_ifd (rdr, tiff_data_start, print_hex_values, print_offsets, NULL);
        ifd_count++;
    }

    if (rdr->exif_ifd_offset != 0) {
        printf (" Exif IFD");
        if (print_offsets) {
            printf (" @%lu\n", rdr->exif_ifd_offset);
        } else {
            printf ("\n");
        }
        uint64_t current_offset = rdr->offset;
        jpg_jump_to (rdr, tiff_data_start + rdr->exif_ifd_offset);
        print_tiff_ifd (rdr, tiff_data_start, print_hex_values, print_offsets, &g_tiff_tag_names.exif_ifd_tag_names);
        jpg_jump_to (rdr, current_offset);
    }

    if (rdr->gps_ifd_offset != 0) {
        printf (" GPS IFD");
        if (print_offsets) {
            printf (" @%lu\n", rdr->gps_ifd_offset);
        } else {
            printf ("\n");
        }
        uint64_t current_offset = rdr->offset;
        jpg_jump_to (rdr, tiff_data_start + rdr->gps_ifd_offset);
        print_tiff_ifd (rdr, tiff_data_start, print_hex_values, print_offsets, &g_tiff_tag_names.gps_ifd_tag_names);
        jpg_jump_to (rdr, current_offset);
    }

    if (rdr->interoperability_ifd_offset != 0) {
        printf (" Interoperability IFD");
        if (print_offsets) {
            printf (" @%lu\n", rdr->interoperability_ifd_offset);
        } else {
            printf ("\n");
        }

        uint64_t current_offset = rdr->offset;
        jpg_jump_to (rdr, tiff_data_start + rdr->interoperability_ifd_offset);
        print_tiff_ifd (rdr, tiff_data_start, print_hex_values, print_offsets, NULL);
        jpg_jump_to (rdr, current_offset);

        // TODO: This is a very small IFD, it will only have a single tag 0x1
        // with a few possible values. I don't think we need to call the full
        // tff_ifd processing function.
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
#if 0
                print_exif_as_tiff_data_no_ir (rdr);
#else
                print_exif_as_tiff_data (rdr);
#endif
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
