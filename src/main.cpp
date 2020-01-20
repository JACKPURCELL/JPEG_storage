/*
 * Takes two array of bits, and build the huffman table for size, and code
 * 
 * lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
 * code_size will be used to known how many bits this symbol is encoded.
 * slowtable will be used when the first lookup didn't give the result.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <vector>
#include "tinyjpeg-internal.h"
#ifdef __APPLE__
#include <sys/uio.h>
#else
#include <sys/io.h>
#endif

using namespace std;

#include <dirent.h>
#include <unistd.h>

#if defined(__GNUC__) && (__GNUC__ > 3) && defined(__OPTIMIZE__)
#define __likely(x)       __builtin_expect(!!(x), 1)
#define __unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define __likely(x)       (x)
#define __unlikely(x)     (x)
#endif

typedef void (*decode_MCU_fct) (struct jdec_private *priv);
typedef void (*convert_colorspace_fct) (struct jdec_private *priv);

struct jdec_private;


int tinyjpeg_decode(struct jdec_private *priv);


struct component component_infos[COMPONENTS];





#ifndef _UINT16_T
#define _UINT16_T
typedef unsigned short uint16_t;
#endif /* _UINT16_T */

/* Global variable to return the last printf found while deconding */
static char printf_string[256];
char *JPEGFileNameRecord[32767];
long long JPEGFileNum=0;


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



//one huffman information
typedef struct one_huff{
    unsigned int val;
    unsigned int code;
    unsigned int code_size;
}ONE_HUFF;

//one huffman table
typedef struct one_huff_table{
    ONE_HUFF huff_table[1024];
    int one_huff_num;
}ONE_HUFF_TABLE;

//one jpeg four huffman table
typedef struct one_jpeg_huff{
    char FullPathName[1024];
    ONE_HUFF_TABLE jpeg_huff[4];//DC0 DC1 AC0 AC1
}ONE_JPEG_HUFF;

//one jpeg four huffman table
typedef struct dir_huff{
    ONE_JPEG_HUFF dir_huff[32767];
}DIR_HUFF;

DIR_HUFF *Dir_Record=(DIR_HUFF*)malloc(sizeof(DIR_HUFF));

//use for gather all the val
DIR_HUFF *All_record=(DIR_HUFF*)malloc(sizeof(DIR_HUFF));

/*
 * 4 functions to manage the stream
 *
 *  fill_nbits: put at least nbits in the reservoir of bits.
 *              But convert any 0xff,0x00 into 0xff
 *  get_nbits: read nbits from the stream, and put it in result,
 *             bits is removed from the stream and the reservoir is filled
 *             automaticaly. The result is signed according to the number of
 *             bits.
 *  look_nbits: read nbits from the stream without marking as read.
 *  skip_nbits: read nbits from the stream but do not return the result.
 *
 * stream: current pointer in the jpeg data (read bytes per bytes)
 * nbits_in_reservoir: number of bits filled into the reservoir
 * reservoir: register that contains bits information. Only nbits_in_reservoir
 *            is valid.
 *                          nbits_in_reservoir
 *                        <--    17 bits    -->
 *            Ex: 0000 0000 1010 0000 1111 0000   <== reservoir
 *                        ^
 *                        bit 1
 *            To get two bits from this example
 *                 result = (reservoir >> 15) & 3
 *
 */
#define fill_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
   while (nbits_in_reservoir<nbits_wanted) \
    { \
      unsigned char c; \
      if (stream >= priv->stream_end) \
        longjmp(priv->jump_state, -EIO); \
      c = *stream++; \
      reservoir <<= 8; \
      if (c == 0xff && *stream == 0x00) \
        stream++; \
      reservoir |= c; \
      nbits_in_reservoir+=8; \
    } \
}  while(0);

/* Signed version !!!! */
#define get_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
   result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
   nbits_in_reservoir -= (nbits_wanted);  \
   reservoir &= ((1U<<nbits_in_reservoir)-1); \
   if ((unsigned int)result < (1UL<<((nbits_wanted)-1))) \
       result += (0xFFFFFFFFUL<<(nbits_wanted))+1; \
}  while(0);

#define look_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do { \
   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
   result = ((reservoir)>>(nbits_in_reservoir-(nbits_wanted))); \
}  while(0);

/* To speed up the decoding, we assume that the reservoir have enough bit
 * slow version:
 * #define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
 *   fill_nbits(reservoir,nbits_in_reservoir,stream,(nbits_wanted)); \
 *   nbits_in_reservoir -= (nbits_wanted); \
 *   reservoir &= ((1U<<nbits_in_reservoir)-1); \
 * }  while(0);
 */
#define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do { \
   nbits_in_reservoir -= (nbits_wanted); \
   reservoir &= ((1U<<nbits_in_reservoir)-1); \
}  while(0);

long long workon=0;


static int find_next_rst_marker(struct jdec_private *priv)
{
    int rst_marker_found = 0;
    int marker;
    const unsigned char *stream = priv->stream;

    /* Parse marker */
    while (!rst_marker_found)
    {
        while (*stream++ != 0xff)
        {
            if (stream >= priv->stream_end)
                printf("EOF while search for a RST marker.");
        }
        /* Skip any padding ff byte (this is normal) */
        while (*stream == 0xff)
            stream++;

        marker = *stream++;
        if ((RST+priv->last_rst_marker_seen) == marker)
            rst_marker_found = 1;
        else if (marker >= RST && marker <= RST7)
            printf("Wrong Reset marker found, abording");
        else if (marker == EOI)
            return 0;
    }
    printf("RST Marker %d found at offset %d\n", priv->last_rst_marker_seen, stream - priv->stream_begin);

    priv->stream = stream;
    priv->last_rst_marker_seen++;
    priv->last_rst_marker_seen &= 7;

    return 0;
}


static void build_huffman_table(const unsigned char *bits, const unsigned char *vals, struct huffman_table *table,int type)
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

        int workonnum=Dir_Record->dir_huff[workon].jpeg_huff[type].one_huff_num;
        Dir_Record->dir_huff[workon].jpeg_huff[type].huff_table[workonnum].val=vals[i];
        Dir_Record->dir_huff[workon].jpeg_huff[type].huff_table[workonnum].code=huffcode[i];
        Dir_Record->dir_huff[workon].jpeg_huff[type].huff_table[workonnum].code_size=huffsize[i];
        Dir_Record->dir_huff[workon].jpeg_huff[type].one_huff_num++;
        printf("val=%2.2x,code=%8.8x,codesize=%2.2d\n", val, code, code_size);


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
    printf("start: DHT marker\n");

    //------------------------------------------
    char *temp;
    FILE *fp;
    //------------------------------------------

    length = be16_to_cpu(stream) - 2;
    //跳过length字段
    stream += 2;	/* Skip length */


    while (length>0) {
        //---------------------
        char temp_str1[100]={0};
        char temp_str2[100]={0};
        temp=(char *)stream;
        //fp = fopen("DHT.txt", "a+");
        //fwrite(temp, 16, 1, fp);
        for(j=0;j<16;j++){
            //fprintf(fp,"%d ",temp[j]);
            sprintf(temp_str2,"%02d ",temp[j]);
            strcat(temp_str1,temp_str2);
        }
        //fprintf(fp,"\n-----------------------\n");
        //fclose(fp);
        //-----------------------------------------------------

        //printf("DHT %s","定义霍夫曼表【交流系数表】%s",temp_str1,"Huffman表ID号和类型：1字节，高4位为表的类型，0：DC直流；1：AC交流 可以看出这里是直流表；低四位为Huffman表ID");
        printf("%s\n",temp_str1);




        int type;
        if(temp_str1[0]=='0') {
            if (temp_str1[1] == '0') type=0;
            else if (temp_str1[1] == '1') type=1;
        } else if (temp_str1[0]=='1'){
            if (temp_str1[1] == '6') type=2;
            else if (temp_str1[1] == '7') type=3;
        }
        //-----------------------------------------------------

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
        {
            printf("%d\n",type);
            build_huffman_table(huff_bits, stream, &priv->HTAC[index&0xf],type);
        }else{
            printf("%d\n",type);
            build_huffman_table(huff_bits, stream, &priv->HTDC[index&0xf],type);
        }

        length -= 1;
        length -= 16;
        length -= count;
        stream += count;
    }
    printf("end: DHT marker\n");
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

static void print_SOF(const unsigned char *stream)
{
    int width, height, nr_components, precision;

    precision = stream[2];
    height = be16_to_cpu(stream+3);
    width  = be16_to_cpu(stream+5);
    nr_components = stream[7];

    printf("start: SOF marker\n");
    printf("Size:%dx%d nr_components:%d  precision:%d\n",
          width, height,
          nr_components,
          precision);
}

static int parse_SOF(struct jdec_private *priv, const unsigned char *stream)
{
    int i, width, height, nr_components, cid, sampling_factor;
    int Q_table;
    struct component *c;

    print_SOF(stream);

    height = be16_to_cpu(stream+3);
    width  = be16_to_cpu(stream+5);
    nr_components = stream[7];

    stream += 8;
    for (i=0; i<nr_components; i++) {
        cid = *stream++;
        sampling_factor = *stream++;
        Q_table = *stream++;
        c = &priv->component_infos[i];

        c->Vfactor = sampling_factor&0xf;
        c->Hfactor = sampling_factor>>4;
        c->Q_table = priv->Q_tables[Q_table];
        printf("Component:%d  factor:%dx%d  Quantization table:%d\n",
              cid, c->Hfactor, c->Hfactor, Q_table );

    }
    priv->width = width;
    priv->height = height;

    printf("end: SOF marker\n");

    return 0;
}


static int parse_SOS(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int i, cid, table;
    unsigned int nr_components = stream[2];

    printf("start: SOS marker\n");

#if SANITY_CHECK
    if (nr_components != 3)
    printf("We only support YCbCr image\n");
#endif

    stream += 3;
    for (i=0;i<nr_components;i++) {
        cid = *stream++;
        table = *stream++;
#if SANITY_CHECK
        if ((table&0xf)>=4)
	printf("We do not support more than 2 AC Huffman table\n");
     if ((table>>4)>=4)
	printf("We do not support more than 2 DC Huffman table\n");
     if (cid != priv->component_infos[i].cid)
        printf("SOS cid order (%d:%d) isn't compatible with the SOF marker (%d:%d)\n",
	      i, cid, i, priv->component_infos[i].cid);
     printf("ComponentId:%d  tableAC:%d tableDC:%d\n", cid, table&0xf, table>>4);
#endif
        priv->component_infos[i].AC_table = &priv->HTAC[table&0xf];
        priv->component_infos[i].DC_table = &priv->HTDC[table>>4];
    }
    priv->stream = stream+3;
    printf("end: SOS marker\n");
    return 0;
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
            case SOF:
                if (parse_SOF(priv, stream) < 0)
                    return -1;
                break;
                //Define Huffman table
            case DHT:
                //开始解析DHT
                if (parse_DHT(priv, stream) < 0)
                    return -1;
                dht_marker_found = 1;
                break;
            case SOS:
                if (parse_SOS(priv, stream) < 0)
                    return -1;
                sos_marker_found = 1;
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

const char *tinyjpeg_get_printfstring(struct jdec_private *priv)
{
    /* FIXME: the printf string must be store in the context */
    priv = priv;
    return printf_string;
}


void tinyjpeg_get_size(struct jdec_private *priv, unsigned int *width, unsigned int *height)
{
    *width = priv->width;
    *height = priv->height;
}





/**
 * Load one jpeg image, and decompress it, and save the result.
 */
void convert_one_image(const char *infilename)
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
       { printf("Cannot open filename\n");
        exit(1);}
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
    //jdec->dlg=(CSpecialVIJPGDlg *)lparam;

    if (jdec == NULL)
        printf("Not enough memory to alloc the structure need for decompressing\n");
    //解头部
    if (tinyjpeg_parse_header(jdec, buf, length_of_file)<0)
        printf(tinyjpeg_get_printfstring(jdec));
    /* Get the size of the image */
    //获得图像长宽
    tinyjpeg_get_size(jdec, &width, &height);

    snprintf(printf_string, sizeof(printf_string),"Decoding JPEG image...\n");
    //解码实际数据
    if (tinyjpeg_decode(jdec) < 0)
        printf(tinyjpeg_get_printfstring(jdec));

    free(buf);

}

//——————————————————————————————————————————————————————————————————————
//use for record all jpeg files in the dir
int readFileList(char *basePath,FILE* picname)
{

    DIR *dir;
    struct dirent *ptr;
    char base[1000];
    int namelen = 0;

    if ((dir=opendir(basePath)) == NULL)
    {
        printf("Open dir printf...");
        exit(1);
    }

    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0)    ///current dir OR parrent dir
            continue;
        else if(ptr->d_type == 8)
        {
            namelen = strlen(ptr->d_name);if(ptr->d_name[namelen-4] == '.' && ( ptr->d_name[namelen-3] == 'j' || ptr->d_name[namelen-3] == 'J' ) && ( ptr->d_name[namelen-2] == 'p' || ptr->d_name[namelen-2] == 'P' ) && ( ptr->d_name[namelen-1] == 'g' || ptr->d_name[namelen-1] == 'G' ))    ///jpgfile
            {
                //ptr->d_name[namelen-4] = '\0';
            //    JPEGFileNameRecord[JPEGFileNum]=(char *)malloc(100*sizeof(char));
                char TempRecord[1000];
                memset(TempRecord, '\0', sizeof(TempRecord));
                strcat(TempRecord,basePath);
                strcat(TempRecord,"/");
                strcat(TempRecord,ptr->d_name);
               // strcpy(JPEGFileNameRecord[JPEGFileNum],TempRecord);

//                strcpy(JPEGFileNameRecord[JPEGFileNum++],ptr->d_name);
                //printf("%s\n",JPEGFileNameRecord[JPEGFileNum]);
                //fflush(stdout);
                //JPEGFileNum++;
                //one_j=(struct one_jpeg_huff*)malloc(sizeof(struct one_jpeg_huff));
                strcpy(Dir_Record->dir_huff[JPEGFileNum++].FullPathName,TempRecord);

                memset(TempRecord, 0, sizeof(TempRecord));
                fprintf(picname,"%s\n",ptr->d_name);
            }
        }
            //else if(ptr->d_type == 10)    ///link file
            //printf("d_name:%s/%s\n",basePath,ptr->d_name);
        else if(ptr->d_type == 4)    ///dir
        {
            memset(base,'\0',sizeof(base));
            strcpy(base,basePath);
            strcat(base,"/");
            strcat(base,ptr->d_name);
            readFileList(base,picname);
        }
    }
    closedir(dir);
    return 1;
}

void getFilelist(){
    FILE *names;
    DIR *dir;
    char basePath[1000];
    char trainpath[1000];
    //pause();
    ///get the current absoulte path
    memset(basePath, '\0', sizeof(basePath));
    // getcwd(basePath, 999);
    // printf("the current dir is : %s\n", basePath);
    strcpy(trainpath, basePath);
    strcat(trainpath, "/Users/ljc/Documents/GitHub/JPEG_storage/test/train.txt");
    strcat(basePath, "/Users/ljc/Documents/GitHub/JPEG_storage/test");
    printf("%s\n", basePath);
    fflush(stdout);

    ///get the file list
    //memset(basePath,'\0',sizeof(basePath));
    //strncpy(basePath,"./XL",-1);
    names = fopen(trainpath, "a+");
    readFileList(basePath, names);

    fclose(names);
}


/**
 * Get the next (valid) huffman code in the stream.
 *
 * To speedup the procedure, we look HUFFMAN_HASH_NBITS bits and the code is
 * lower than HUFFMAN_HASH_NBITS we have automaticaly the length of the code
 * and the value by using two lookup table.
 * Else if the value is not found, just search (linear) into an array for each
 * bits is the code is present.
 *
 * If the code is not present for any reason, -1 is return.
 */
static int get_next_huffman_code(struct jdec_private *priv, struct huffman_table *huffman_table)
{
    int value, hcode;
    unsigned int extra_nbits, nbits;
    uint16_t *slowtable;

    look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, HUFFMAN_HASH_NBITS, hcode);
    value = huffman_table->lookup[hcode];
    if (__likely(value >= 0))
    {
        unsigned int code_size = huffman_table->code_size[value];
        skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, code_size);
        return value;
    }

    /* Decode more bits each time ... */
    for (extra_nbits=0; extra_nbits<16-HUFFMAN_HASH_NBITS; extra_nbits++)
    {
        nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;

        look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits, hcode);
        slowtable = huffman_table->slowtable[extra_nbits];
        /* Search if the code is in this array */
        while (slowtable[0]) {
            if (slowtable[0] == hcode) {
                skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits);
                return slowtable[1];
            }
            slowtable+=2;
        }
    }
    return 0;
}


/**
 *
 * Decode a single block that contains the DCT coefficients.
 *
 */
static void process_Huffman_data_unit(struct jdec_private *priv, int component)
{
    unsigned char j;
    unsigned int huff_code;
    unsigned char size_val, count_0;

    struct component *c = &priv->component_infos[component];
    short int DCT[64];


    /* Initialize the DCT coef table */
    memset(DCT, 0, sizeof(DCT));

    /* DC coefficient decoding */
    huff_code = get_next_huffman_code(priv, c->DC_table);
//    if(huff_code!=0)

        switch (component){
            case 0:
                huff_val.DC0[huff_code]++;
                //printf("DC0 %02x\n", huff_code);//val
                break;
            case 1:
                huff_val.DC1[huff_code]++;
                //printf("DC1 %02x\n", huff_code);//val
                break;
            case 2:
                huff_val.DC1[huff_code]++;
                //printf("DC1 %02x\n", huff_code);//val
                break;
        }

    if (huff_code) {
        get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, huff_code, DCT[0]);//再读huffman个
        DCT[0] += c->previous_DC;//直流系数差分还原
        c->previous_DC = DCT[0];
    } else {
        DCT[0] = c->previous_DC;
    }

    /* AC coefficient decoding */
    j = 1;
    while (j<64)
    {
        huff_code = get_next_huffman_code(priv, c->AC_table);
//        if(huff_code!=0)

        switch (component){
            case 0:
                huff_val.AC0[huff_code]++;
                //printf("AC0 %02x\n", huff_code);//val
                break;
            case 1:
                huff_val.AC1[huff_code]++;
                //printf("AC1 %02x\n", huff_code);//val
                break;
            case 2:
                huff_val.AC1[huff_code]++;
                //printf("AC1 %02x\n", huff_code);//val
                break;
        }

        size_val = huff_code & 0xF;//低四位，读取n位
        count_0 = huff_code >> 4;//高四位，n个0

        if (size_val == 0)
        { /* RLE */
            if (count_0 == 0)
                break;	/* EOB found, go out */
            else if (count_0 == 0xF)
                j += 16;	/* skip 16 zeros */
        }
        else
        {
            j += count_0;	/* skip count_0 zeroes */
            if (__unlikely(j >= 64))
            {
                snprintf(printf_string, sizeof(printf_string), "Bad huffman data (buffer overflow)");
                break;
            }
            get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, size_val, DCT[j]);
            j++;
        }
    }


    for (j = 0; j < 64; j++)
        c->DCT[j] = DCT[j];
}


/*
 * Decode all the 3 components for 1x1
 */
static void decode_ONE_MCU(struct jdec_private *priv)
{
    // Y
    process_Huffman_data_unit(priv, cY);//DC0,AC0,0

    // Cb
    process_Huffman_data_unit(priv, cCb);//DC1,AC1,1

    // Cr
    process_Huffman_data_unit(priv, cCr);//DC1,AC1,2
}


static void resync(struct jdec_private *priv)
{
    int i;

    /* Init DC coefficients */
    for (i=0; i<COMPONENTS; i++)
        priv->component_infos[i].previous_DC = 0;

    priv->reservoir = 0;
    priv->nbits_in_reservoir = 0;
    if (priv->restart_interval > 0)
        priv->restarts_to_go = priv->restart_interval;
    else
        priv->restarts_to_go = -1;
}

/**
 * Decode and convert the jpeg image into @pixfmt@ image
 *
 * Note: components will be automaticaly allocated if no memory is attached.
 */

int tinyjpeg_decode(struct jdec_private *priv)
{
    unsigned int x, y, xstride_by_mcu, ystride_by_mcu;
    unsigned int bytes_per_blocklines[3], bytes_per_mcu[3];

    if (setjmp(priv->jump_state))
        return -1;

    /* To keep gcc happy initialize some array */
    bytes_per_mcu[1] = 0;
    bytes_per_mcu[2] = 0;
    bytes_per_blocklines[1] = 0;
    bytes_per_blocklines[2] = 0;


    xstride_by_mcu = ystride_by_mcu = 8;
    if ((priv->component_infos[cY].Hfactor | priv->component_infos[cY].Vfactor) == 1) {
        printf("Use decode 1x1 sampling\n");
    } else if (priv->component_infos[cY].Hfactor == 1) {
        ystride_by_mcu = 16;
        printf("Use decode 1x2 sampling (not supported)\n");
    } else if (priv->component_infos[cY].Vfactor == 2) {
        xstride_by_mcu = 16;
        ystride_by_mcu = 16;
        printf("Use decode 2x2 sampling\n");
    } else {
        xstride_by_mcu = 16;
        printf("Use decode 2x1 sampling\n");
    }

    resync(priv);



    /* Just the decode the image by macroblock (size is 8x8, 8x16, or 16x16) */
    for (y=0; y < priv->height/ystride_by_mcu; y++)
    {
        for (x=0; x < priv->width; x+=xstride_by_mcu)
        {
            decode_ONE_MCU(priv);

            if (priv->restarts_to_go>0)
            {
                priv->restarts_to_go--;
                if (priv->restarts_to_go == 0)
                {
                    priv->stream -= (priv->nbits_in_reservoir/8);
                    resync(priv);
                    if (find_next_rst_marker(priv) < 0)
                        return -1;
                }
            }
        }
    }

    printf("Input file size: %d\n", priv->stream_length+2);
    printf("Input bytes actually read: %d\n", priv->stream - priv->stream_begin + 2);

    return 0;
}



void compare(){
    long int count=0;
    long int temp=0;
    int i=0,j=0;
    for(i=0;i<256;i++){//对于一个code
        temp=0;
        for(j=0;j<Dir_Record->dir_huff[0].jpeg_huff[3].one_huff_num;j++){
            if(i==Dir_Record->dir_huff[0].jpeg_huff[3].huff_table[j].val) {
                temp = Dir_Record->dir_huff[0].jpeg_huff[3].huff_table[j].code_size;
                break;
            }
        }
        count+=(huff_val.AC1[i]*temp);
    }

    printf("%ld\n",count);
}

//____________________________________________________________

#define N 256//带权值的叶子节点数或者是需要编码的字符数
#define M 2*N-1//n个叶子节点构造的哈夫曼树有2n-1个结点
#define MAX 10000
typedef int TElemType;
//静态三叉链表存储结构
typedef struct{
    //TElemType data;
    unsigned int weight;//权值只能是正数
    int parent;
    int lchild;
    int rchild;
}HTNode;//, *HuffmanTree;
typedef HTNode HuffmanTree[M+1];//0号单元不使用

typedef char * HuffmanCode[N+1];//存储每个字符的哈夫曼编码表，是一个字符指针数组，每个数组元素是指向字符指针的指针


//在HT[1...k]里选择parent为0的且权值最小的2结点，其序号分别为s1,s2，parent不为0说明该结点已经参与构造了，故不许再考虑
void select(HuffmanTree HT, int k, int &s1, int &s2){
    //假设s1对应的权值总是<=s2对应的权值
    unsigned int tmp = MAX, tmpi = 0;
    for(int i = 1; i <= k; i++){
        if(!HT[i].parent){//parent必须为0
            if(tmp > HT[i].weight){
                tmp = HT[i].weight;//tmp最后为最小的weight
                tmpi = i;
            }
        }
    }
    s1 = tmpi;

    tmp = MAX;
    tmpi = 0;
    for(int i = 1; i <= k; i++){
        if((!HT[i].parent) && i!=s1){//parent为0
            if(tmp > HT[i].weight){
                tmp = HT[i].weight;
                tmpi = i;
            }
        }
    }
    s2 = tmpi;
}

//构造哈夫曼树
void createHuffmanTree(HuffmanTree &HT, int *w, int n){
    if(n <= 1)
        return;
    //对树赋初值
    int i;
    for(i = 1; i <= n; i++){//HT前n个分量存储叶子节点，他们均带有权值
        HT[i].weight = w[i];
        HT[i].lchild = 0;
        HT[i].parent = 0;
        HT[i].rchild = 0;
    }
    for(; i <=M; i++){//HT后m-n个分量存储中间结点，最后一个分量显然是整棵树的根节点
        HT[i].weight = 0;
        HT[i].lchild = 0;
        HT[i].parent = 0;
        HT[i].rchild = 0;
    }
    //开始构建哈夫曼树，即创建HT的后m-n个结点的过程，直至创建出根节点。用哈夫曼算法
    for(i = n+1; i <= M; i++){
        int s1, s2;
        select(HT, i-1, s1, s2);//在HT[1...i-1]里选择parent为0的且权值最小的2结点，其序号分别为s1,s2，parent不为0说明该结点已经参与构造了，故不许再考虑
        HT[s1].parent = i;
        HT[s2].parent = i;
        HT[i].lchild = s1;
        HT[i].rchild = s2;
        HT[i].weight = HT[s1].weight + HT[s2].weight;
    }
}



//打印哈夫曼满树
void printHuffmanTree(HuffmanTree HT, int ch[]){
    printf("\n");
    printf("data, weight, parent, lchild, rchild\n");
    for(int i = 1; i <= M; i++){
        if(i > N){
          //  printf("  -, %5d, %5d, %5d, %5d\n", HT[i].weight, HT[i].parent, HT[i].lchild, HT[i].rchild);
        }else{
         //   printf("  %2x, %5d, %5d, %5d, %5d\n", ch[i], HT[i].weight, HT[i].parent, HT[i].lchild, HT[i].rchild);
        }
    }
    printf("\n");
}


//为每个字符求解哈夫曼编码，从叶子到根逆向求解每个字符的哈夫曼编码
void encodingHuffmanCode(HuffmanTree HT, HuffmanCode &HC){
    //char *tmp = (char *)malloc(n * sizeof(char));//将每一个字符对应的编码放在临时工作空间tmp里，每个字符的编码长度不会超过n
    char tmp[N];
    tmp[N-1] = '\0';//编码的结束符
    int start, c, f;
    for(int i = 1; i <= N; i++){//对于第i个待编码字符即第i个带权值的叶子节点
        start = N-1;//编码生成以后，start将指向编码的起始位置
        c = i;
        f = HT[i].parent;

        while(f){//f!=0,即f不是根节点的父节点
            if(HT[f].lchild == c){
                tmp[--start] = '0';
            }else{//HT[f].rchild == c,注意:由于哈夫曼树中只存在叶子节点和度为2的节点，所以除开叶子节点，节点一定有左右2个分支
                tmp[--start] = '1';
            }
            c = f;
            f = HT[f].parent;
        }
        HC[i] = (char *)malloc((N-start)*sizeof(char));//每次tmp的后n-start个位置有编码存在
        strcpy(HC[i], &tmp[start]);//将tmp的后n-start个元素分给H[i]指向的的字符串
    }
}

//打印哈夫曼编码表
void printHuffmanCoding(HuffmanCode HC, int ch[]){
    printf("\n");
    long count=0;
    for(int i = 1; i <= N; i++){
        printf("%02x:%s\n", ch[i], HC[i]);
        count+=(huff_val.AC1[i-1]*strlen(HC[i]));
    }
    printf("%ld\n",count);
}


//解码过程：从哈夫曼树的根节点出发，按字符'0'或'1'确定找其左孩子或右孩子，直至找到叶子节点即可，便求得该字串相应的字符
void decodingHuffmanCode(HuffmanTree HT, char *ch, char testDecodingStr[], int len, char *result){
    int p = M;//HT的最后一个节点是根节点，前n个节点是叶子节点
    int i = 0;//指示测试串中的第i个字符
    //char result[30];//存储解码以后的字符串
    int j = 0;//指示结果串中的第j个字符
    while(i<len){
        if(testDecodingStr[i] == '0'){
            p = HT[p].lchild;
        }
        if(testDecodingStr[i] == '1'){
            p = HT[p].rchild;
        }

        if(p <= N){//p<=N则表明p为叶子节点,因为在构造哈夫曼树HT时，HT的m个节点中前n个节点为叶子节点
            result[j] = ch[p];
            j++;
            p = M;//p重新指向根节点
        }
        i++;
    }
    result[j] = '\0';//结果串的结束符
}


void compare1(){
    HuffmanTree HT;

    TElemType ch[N+1];//0号单元不使用，存储n个等待编码的字符
    int w[N+1];//0号单元不使用，存储n个字符对应的权值

    for(int i=i;i<=N;i++){
        ch[i]=i-1;
        w[i]=huff_val.AC1[i-1];
    }

    createHuffmanTree(HT, w , N);//构建哈夫曼树
    printHuffmanTree(HT, ch);

    HuffmanCode HC;//HC有n个元素，每个元素是一个指向字符串的指针，即每个元素是一个char *的变量
    encodingHuffmanCode(HT, HC);//为每个字符求解哈夫曼编码
    printHuffmanCoding(HC, ch);

}

//__________________________________________________________

int main(){

    //get the file fullpathname
    getFilelist();

    //get all the huffman table
    for(workon=0;workon<JPEGFileNum;workon++){

      //  char *fp="/Users/ljc/摄影照片LJC_0282.jpg";
        //strcpy(fp,strcat("/Users/ljc/摄影照片",JPEGFileNameRecord[i]));
        printf("\n%s\n",Dir_Record->dir_huff[workon].FullPathName);
        convert_one_image(Dir_Record->dir_huff[workon].FullPathName);
    }

compare();
compare1();


//    //compare the val
//
//
//    ONE_HUFF all_val[256];
//
//    for(int i=0;i<256;i++){
//        all_val[i].val=i;
//        all_val[i].code=0;
//        all_val[i].code_size=0;
//    }
//
//    for(int i=0;i<JPEGFileNum;i++){
//        for(int j=0;j<1;j++) {
//            int max_code=0;
//            int temp=Dir_Record->dir_huff[i].jpeg_huff[j].one_huff_num;
//            for(int k=0;k<temp;k++){
//                int temp_val=Dir_Record->dir_huff[i].jpeg_huff[j].huff_table[k].val;
//                int temp_code=Dir_Record->dir_huff[i].jpeg_huff[j].huff_table[k].code;
//                if(max_code<=temp_code) max_code=temp_code;
//                all_val[temp_val].code+=temp_code;
//            }
//        }
//    }
//    for(int i=0;i<256;i++){
//        printf("%2.2x ",all_val[i].val);
//        printf("%8.8x\n",all_val[i].code);
//    }
    //memset(JPEGFileNameRecord, 0, sizeof(JPEGFileNameRecord));



    return 0;
}
