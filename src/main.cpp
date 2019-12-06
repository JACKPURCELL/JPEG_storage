/*
 * Takes two array of bits, and build the huffman table for size, and code
 * 
 * lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
 * code_size will be used to known how many bits this symbol is encoded.
 * slowtable will be used when the first lookup didn't give the result.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

struct jdec_private;

#define HUFFMAN_BITS_SIZE  256
#define HUFFMAN_HASH_NBITS 9
#define HUFFMAN_HASH_SIZE  (1UL<<HUFFMAN_HASH_NBITS)
#define HUFFMAN_HASH_MASK  (HUFFMAN_HASH_SIZE-1)

#define HUFFMAN_TABLES	   4
#define COMPONENTS	   3
#define JPEG_MAX_WIDTH	   2048
#define JPEG_MAX_HEIGHT	   2048

#define be16_to_cpu(x) (((x)[0]<<8)|(x)[1])
#define HUFFMAN_BITS_SIZE  256
#define HUFFMAN_HASH_NBITS 9
#define HUFFMAN_HASH_SIZE  (1UL<<HUFFMAN_HASH_NBITS)
#define HUFFMAN_HASH_MASK  (HUFFMAN_HASH_SIZE-1)

#define HUFFMAN_TABLES	   4

#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short uint16_t;
#endif /* _UINT16_T */

/* Global variable to return the last error found while deconding */
static char error_string[256];

struct huffman_table
{
    /* Fast look up table, using HUFFMAN_HASH_NBITS bits we can have directly the symbol,
     * if the symbol is <0, then we need to look into the tree table */
    short int lookup[HUFFMAN_HASH_SIZE];
    /* code size: give the number of bits of a symbol is encoded */
    unsigned char code_size[HUFFMAN_HASH_SIZE];
    /* some place to store value that is not encoded in the lookup table
     * FIXME: Calculate if 256 value is enough to store all values
     */
    uint16_t slowtable[16-HUFFMAN_HASH_NBITS][256];
};





enum std_markers {
    DQT  = 0xDB, /* Define Quantization Table */
    SOF  = 0xC0, /* Start of Frame (size information) */
    DHT  = 0xC4, /* Huffman Table */
    SOI  = 0xD8, /* Start of Image */
    SOS  = 0xDA, /* Start of Scan */
    RST  = 0xD0, /* Reset Marker d0 -> .. */
    RST7 = 0xD7, /* Reset Marker .. -> d7 */
    EOI  = 0xD9, /* End of Image */
    DRI  = 0xDD, /* Define Restart Interval */
    APP0 = 0xE0,
};

struct jdec_private
{
    /* Public variables */
    uint8_t *components[COMPONENTS];
    unsigned int width, height;	/* Size of the image */
    unsigned int flags;

    /* Private variables */
    const unsigned char *stream_begin, *stream_end;
    unsigned int stream_length;

    const unsigned char *stream;	/* Pointer to the current stream */
    unsigned int reservoir, nbits_in_reservoir;

   // struct component component_infos[COMPONENTS];
    float Q_tables[COMPONENTS][64];		/* quantization tables */
    struct huffman_table HTDC[HUFFMAN_TABLES];	/* DC huffman tables   */
    struct huffman_table HTAC[HUFFMAN_TABLES];	/* AC huffman tables   */
    int default_huffman_table_initialized;
    int restart_interval;
    int restarts_to_go;				/* MCUs left in this restart interval */
    int last_rst_marker_seen;			/* Rst marker is incremented each time */

    /* Temp space used after the IDCT to store each components */
    uint8_t Y[64*4], Cr[64], Cb[64];

   // jmp_buf jump_state;
    /* Internal Pointer use for colorspace conversion, do not modify it !!! */
    uint8_t *plane[COMPONENTS];

};

static void build_huffman_table(const unsigned char *bits, const unsigned char *vals, struct huffman_table *table)
{
    unsigned int i, j, code, code_size, val, nbits;
    unsigned char huffsize[HUFFMAN_BITS_SIZE+1], *hz;
    unsigned int huffcode[HUFFMAN_BITS_SIZE+1], *hc;
    int next_free_entry;

    /*
     * Build a temp array
     *   huffsize[X] => numbers of bits to write vals[X]
     */
    hz = huffsize;
    for (i=1; i<=16; i++)
    {
        for (j=1; j<=bits[i]; j++)
            *hz++ = i;
    }
    *hz = 0;

    memset(table->lookup, 0xff, sizeof(table->lookup));
    for (i=0; i<(16-HUFFMAN_HASH_NBITS); i++)
        table->slowtable[i][0] = 0;

    /* Build a temp array
     *   huffcode[X] => code used to write vals[X]
     */
    code = 0;
    hc = huffcode;
    hz = huffsize;
    nbits = *hz;
    while (*hz)
    {
        while (*hz == nbits)
        {
            *hc++ = code++;
            hz++;
        }
        code <<= 1;
        nbits++;
    }

    /*
     * Build the lookup table, and the slowtable if needed.
     */
    next_free_entry = -1;
    for (i=0; huffsize[i]; i++)
    {
        val = vals[i];
        code = huffcode[i];
        code_size = huffsize[i];

        printf("val=%2.2x code=%8.8x codesize=%2.2d\n", val, code, code_size);

        table->code_size[val] = code_size;
        if (code_size <= HUFFMAN_HASH_NBITS)
        {
            /*
             * Good: val can be put in the lookup table, so fill all value of this
             * column with value val
             */
            int repeat = 1UL<<(HUFFMAN_HASH_NBITS - code_size);
            code <<= HUFFMAN_HASH_NBITS - code_size;
            while ( repeat-- )
                table->lookup[code++] = val;

        }
        else
        {
            /* Perhaps sorting the array will be an optimization */
            uint16_t *slowtable = table->slowtable[code_size-HUFFMAN_HASH_NBITS-1];
            while(slowtable[0])
                slowtable+=2;
            slowtable[0] = code;
            slowtable[1] = val;
            slowtable[2] = 0;
            /* TODO: NEED TO CHECK FOR AN OVERFLOW OF THE TABLE */
        }

    }
}


static int parse_DHT(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int count, i,j;
    unsigned char huff_bits[17];
    int length, index;

    length = be16_to_cpu(stream) - 2;
    //跳过length字段
    stream += 2;	/* Skip length */


    while (length>0) {
        //跳过第1字节:
        //Huffman 表ID号和类型，高 4 位为表的类型，0：DC 直流；1：AC交流
        //低四位为 Huffman 表 ID。
        index = *stream++;

        /* We need to calculate the number of bytes 'vals' will takes */
        huff_bits[0] = 0;
        count = 0;

        //不同长度 Huffman 的码字数量：固定为 16 个字节，每个字节代表从长度为 1到长度为 16 的码字的个数
        for (i=1; i<17; i++) {
            huff_bits[i] = *stream++;
            //count记录码字的个数
            count += huff_bits[i];
        }

        if (index & 0xf0 )
        {build_huffman_table(huff_bits, stream, &priv->HTAC[index&0xf]);
            printf("......................\n");}

        else
        {build_huffman_table(huff_bits, stream, &priv->HTDC[index&0xf]);
            printf(";;;;;;;;;;;;;;;;;;;;;;;;\n");}

        length -= 1;
        length -= 16;
        length -= count;
        stream += count;
    }
    printf("< DHT marker\n");
    return 0;
}

/**
 * Allocate a new tinyjpeg decoder object.
 *
 * Before calling any other functions, an object need to be called.
 */
struct jdec_private *tinyjpeg_init(void)
{
    struct jdec_private *priv;

    priv = (struct jdec_private *)calloc(1, sizeof(struct jdec_private));
    if (priv == NULL)
        return NULL;
    return priv;
}

static int filesize(FILE *fp)
{
    long pos;
    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return pos;
}


//解各种不同的标签
static int parse_JFIF(struct jdec_private *priv, const unsigned char *stream)
{
    int chuck_len;
    int marker;
    int sos_marker_found = 0;
    int dht_marker_found = 0;
    const unsigned char *next_chunck;

    /* Parse marker */
    //在Start of scan标签之前
    while (!sos_marker_found)
    {
        if (*stream++ != 0xff){
            printf("emmmmmm!");
            exit(0);
        }

        /* Skip any padding ff byte (this is normal) */
        //跳过0xff字节
        while (*stream == 0xff)
            stream++;
        //marker是跳过0xff字节后1个字节的内容
        marker = *stream++;
        //chunk_len是marker后面2个字节的内容（大端模式需要转换）
        //包含自身，但不包含0xff+marker2字节
        chuck_len = be16_to_cpu(stream);
        //指向下一个chunk的指针
        next_chunck = stream + chuck_len;
        //各种不同的标签
        switch (marker)
        {
                //Define Huffman table
            case DHT:
                //开始解析DHT
                if (parse_DHT(priv, stream) < 0)
                    return -1;
                dht_marker_found = 1;
                break;
            default:
                break;
        }
        //解下一个segment
        stream = next_chunck;
    }
    return 0;

}
/**
 * Initialize the tinyjpeg object and prepare the decoding of the stream.
 *
 * Check if the jpeg can be decoded with this jpeg decoder.
 * Fill some table used for preprocessing.
 */
int tinyjpeg_parse_header(struct jdec_private *priv, const unsigned char *buf, unsigned int size)
{
    int ret;

    /* Identify the file */
    //0x FF D8
    //是否是JPEG格式文件？
    if ((buf[0] != 0xFF) || (buf[1] != SOI))
        printf("Not a JPG file ?\n");
    //JPEG格式文件固定的文件头
    //begin指针前移2字节
    priv->stream_begin = buf+2;
    priv->stream_length = size-2;
    priv->stream_end = priv->stream_begin + priv->stream_length;
    //开始解析JFIF
    ret = parse_JFIF(priv, priv->stream_begin);
    return ret;
}

const char *tinyjpeg_get_errorstring(struct jdec_private *priv)
{
    /* FIXME: the error string must be store in the context */
    priv = priv;
    return error_string;
}


void tinyjpeg_get_size(struct jdec_private *priv, unsigned int *width, unsigned int *height)
{
    *width = priv->width;
    *height = priv->height;
}

int tinyjpeg_get_components(struct jdec_private *priv, unsigned char **components)
{
    int i;
    for (i=0; priv->components[i] && i<COMPONENTS; i++)
        components[i] = priv->components[i];
    return 0;
}

int tinyjpeg_set_components(struct jdec_private *priv, unsigned char **components, unsigned int ncomponents)
{
    unsigned int i;
    if (ncomponents > COMPONENTS)
        ncomponents = COMPONENTS;
    for (i=0; i<ncomponents; i++)
        priv->components[i] = components[i];
    return 0;
}

int tinyjpeg_set_flags(struct jdec_private *priv, int flags)
{
    int oldflags = priv->flags;
    priv->flags = flags;
    return oldflags;
}



/**
 * Load one jpeg image, and decompress it, and save the result.
 */
int convert_one_image(const char *infilename)
{
    FILE *fp;
    unsigned int length_of_file;
    unsigned int width, height;
    unsigned char *buf;
    struct jdec_private *jdec;
    unsigned char *components[3];

    /* Load the Jpeg into memory */
    fp = fopen(infilename, "rb");
    if (fp == NULL)
        printf("Cannot open filename\n");
    length_of_file = filesize(fp);
    buf = (unsigned char *)malloc(length_of_file + 4);
    if (buf == NULL)
        printf("Not enough memory for loading file\n");
    fread(buf, length_of_file, 1, fp);
    fclose(fp);

    /* Decompress it */
    //分配内存
    jdec = tinyjpeg_init();
//    //传入句柄--------------
//    jdec->dlg=(CSpecialVIJPGDlg *)lparam;

    if (jdec == NULL)
        printf("Not enough memory to alloc the structure need for decompressing\n");
    //解头部
    if (tinyjpeg_parse_header(jdec, buf, length_of_file)<0)
        printf(tinyjpeg_get_errorstring(jdec));
    /* Get the size of the image */
    //获得图像长宽
    tinyjpeg_get_size(jdec, &width, &height);
//
//    snprintf(error_string, sizeof(error_string),"Decoding JPEG image...\n");
//    //解码实际数据
//    if (tinyjpeg_decode(jdec, output_format) < 0)
//        printf(tinyjpeg_get_errorstring(jdec));
//    /*
//     * Get address for each plane (not only max 3 planes is supported), and
//     * depending of the output mode, only some components will be filled
//     * RGB: 1 plane, YUV420P: 3 planes, GREY: 1 plane
//     */
//    tinyjpeg_get_components(jdec, components);
//
//    /* Save it */
//    switch (output_format)
//    {
//        case TINYJPEG_FMT_RGB24:
//        case TINYJPEG_FMT_BGR24:
//            write_tga(outfilename, output_format, width, height, components);
//            break;
//        case TINYJPEG_FMT_YUV420P:
//            //开始写入成YUV420P文件
//            write_yuv(outfilename, width, height, components);
//            break;
//        case TINYJPEG_FMT_GREY:
//            //开始写入成灰度文件
//            write_pgm(outfilename, width, height, components);
//            break;
//    }
//
//    /* Only called this if the buffers were allocated by tinyjpeg_decode() */
////modify by lei！  tinyjpeg_free(jdec);
//    /* else called just free(jdec); */
//
    free(buf);
    return 0;
}


int main(){

    char *fp="/Users/ljc/Documents/GitHub/JPEG_storage/logo.jpg";
    convert_one_image(fp);
    return 0;
}