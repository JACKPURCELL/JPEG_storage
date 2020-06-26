/*
 * Takes two array of bits, and build the huffman table for size, and code
 * 
 * lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
 * code_size will be used to known how many bits this symbol is encoded.
 * slowtable will be used when the first lookup didn't give the result.
 */

#define FOLDER "/home/ljc/testjpg"

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
char* tobin(long long n);
void write_head_data(const char *infilename,FILE*fpdat);

typedef void (*decode_MCU_fct) (struct jdec_private *priv);
typedef void (*WriteNewData_decode_MCU_fct) (struct jdec_private *priv,FILE *fp);
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




DIR_HUFF *Dir_Record=(DIR_HUFF*)malloc(sizeof(DIR_HUFF));



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
            printf("Wrong Reset marker found, abording\n");
        else if (marker == EOI)
            return 0;
    }
//    printf("RST Marker %d found at offset %d\n", priv->last_rst_marker_seen, stream - priv->stream_begin);

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
//        printf("val=%2.2x,code=%8.8x,codesize=%2.2d\n", val, code, code_size);


        table->code_size[val] = code_size;
        if (code_size <= HUFFMAN_HASH_NBITS)
        {
            /*
             * Good: val can be put in the lookup table, so fill all value of this
             * column with value val
             */
            int repeat = 1UL<<(HUFFMAN_HASH_NBITS - code_size);
            code <<= HUFFMAN_HASH_NBITS - code_size;//一定是9位
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

static int parse_DRI(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int length;


    length = be16_to_cpu(stream);

    priv->restart_interval = be16_to_cpu(stream+2);



    return 0;
}

static int parse_DHT(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int count, i,j;
    unsigned char huff_bits[17];
    int length, index;
//    printf("start: DHT marker\n");

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
        for(j=0;j<16;j++){
            sprintf(temp_str2,"%02d ",temp[j]);
            strcat(temp_str1,temp_str2);
        }
//        printf("%s\n",temp_str1);




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
//            printf("%d\n",type);
            build_huffman_table(huff_bits, stream, &priv->HTAC[index&0xf],type);
        }else{
//            printf("%d\n",type);
            build_huffman_table(huff_bits, stream, &priv->HTDC[index&0xf],type);
        }

        length -= 1;
        length -= 16;
        length -= count;
        stream += count;
    }
//    printf("end: DHT marker\n");
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

//    printf("start: SOF marker\n");
//    printf("Size:%dx%d nr_components:%d  precision:%d\n",
//           width, height,
//           nr_components,
//           precision);

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
//        printf("Component:%d  factor:%dx%d  Quantization table:%d\n",
//               cid, c->Hfactor, c->Hfactor, Q_table );

    }
    priv->width = width;
    priv->height = height;

//    printf("end: SOF marker\n");

    return 0;
}


static int parse_SOS(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int i, cid, table;
    unsigned int nr_components = stream[2];

//    printf("start: SOS marker\n");

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
//    printf("end: SOS marker\n");
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
    int count=0;
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
        count++;
        //各种不同的标签
        switch (marker)
        {
            case DRI:
                if (parse_DRI(priv, stream) < 0)
                    return -1;
                break;
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
    if (tinyjpeg_parse_header(jdec, buf, length_of_file)<0)//获取sos之后
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

char basePath[1000];
char trainpath[1000];

void getFilelist(){
    FILE *names;
    DIR *dir;

    memset(basePath, '\0', sizeof(basePath));
    strcpy(trainpath, basePath);
    strcat(trainpath, FOLDER);
    strcat(trainpath, "/train.txt");
    strcat(basePath, FOLDER);
    printf("%s\n", basePath);
    fflush(stdout);

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

//    look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, HUFFMAN_HASH_NBITS, hcode);
//    printf("beforefill %d\n",priv->nbits_in_reservoir);
    while (priv->nbits_in_reservoir<HUFFMAN_HASH_NBITS)
    {
        unsigned char c;
        if (priv->stream >= priv->stream_end)
            longjmp(priv->jump_state, -EIO);
        c = *priv->stream++;
        priv->reservoir <<= 8;
        if (c == 0xff && *priv->stream == 0x00)
            priv->stream++;
        priv->reservoir |= c;
        priv->nbits_in_reservoir+=8;
    }
    hcode = ((priv->reservoir)>>(priv->nbits_in_reservoir-(HUFFMAN_HASH_NBITS)));


    value = huffman_table->lookup[hcode];//val,在快速查找中
    if (__likely(value >= 0))
    {
        unsigned int code_size = huffman_table->code_size[value];
//        skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, code_size);//跳过val的codesize，即跳过他自己
        priv->nbits_in_reservoir -= (code_size);
        priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);



        return value;//返回val

    }

    /* Decode more bits each time ... */
    for (extra_nbits=0; extra_nbits<16-HUFFMAN_HASH_NBITS; extra_nbits++)
    {//val不在快速查找表中
        nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;

//        look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits, hcode);
        while (priv->nbits_in_reservoir<nbits)
        {
            unsigned char c;
            if (priv->stream >= priv->stream_end)
                longjmp(priv->jump_state, -EIO);
            c = *priv->stream++;
            priv->reservoir <<= 8;
            if (c == 0xff && *priv->stream == 0x00)
                priv->stream++;
            priv->reservoir |= c;
            priv->nbits_in_reservoir+=8;
        }
        hcode = ((priv->reservoir)>>(priv->nbits_in_reservoir-(nbits)));


        slowtable = huffman_table->slowtable[extra_nbits];
        /* Search if the code is in this array */
        while (slowtable[0]) {
            if (slowtable[0] == hcode) {
//                skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits);
                priv->nbits_in_reservoir -= (nbits);
                priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);


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
    huff_code = get_next_huffman_code(priv, c->DC_table);//返回val
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
//        get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, huff_code, DCT[0]);//读huff_code个数据
        while (priv->nbits_in_reservoir<huff_code)
        {
            unsigned char c;
            if (priv->stream >= priv->stream_end)
                longjmp(priv->jump_state, -EIO);
            c = *priv->stream++;
            priv->reservoir <<= 8;
            if (c == 0xff && *priv->stream == 0x00)
                priv->stream++;
            priv->reservoir |= c;
            priv->nbits_in_reservoir+=8;
        }
        DCT[0] = ((priv->reservoir)>>(priv->nbits_in_reservoir-(huff_code)));
        priv->nbits_in_reservoir -= (huff_code);
        priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);
        if ((unsigned int)DCT[0] < (1UL<<((huff_code)-1)))
            DCT[0] += (0xFFFFFFFFUL<<(huff_code))+1;




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
                break;	/* EOB found, go out 0x00 代表接下來所有的交流係數全爲 0 */
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
//            get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, size_val, DCT[j]);
            while (priv->nbits_in_reservoir<size_val)
            {
                unsigned char c;
                if (priv->stream >= priv->stream_end)
                    longjmp(priv->jump_state, -EIO);
                c = *priv->stream++;
                priv->reservoir <<= 8;
                if (c == 0xff && *priv->stream == 0x00)
                    priv->stream++;
                priv->reservoir |= c;
                priv->nbits_in_reservoir+=8;
            }
            DCT[j] = ((priv->reservoir)>>(priv->nbits_in_reservoir-(size_val)));
            priv->nbits_in_reservoir -= (size_val);
            priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);
            if ((unsigned int)DCT[j] < (1UL<<((size_val)-1)))
                DCT[j] += (0xFFFFFFFFUL<<(size_val))+1;



            j++;
        }
    }


    for (j = 0; j < 64; j++)
        c->DCT[j] = DCT[j];
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



/*
 * Decode all the 3 components for 1x1
 */
static void decode_MCU_1x1_3planes(struct jdec_private *priv)
{
    // Y
    process_Huffman_data_unit(priv, cY);

    // Cb
    process_Huffman_data_unit(priv, cCb);

    // Cr
    process_Huffman_data_unit(priv, cCr);
}



/*
 * Decode a 2x1
 *  .-------.
 *  | 1 | 2 |
 *  `-------'
 */
static void decode_MCU_2x1_3planes(struct jdec_private *priv)
{
    // Y
    process_Huffman_data_unit(priv, cY);
    process_Huffman_data_unit(priv, cY);

    // Cb
    process_Huffman_data_unit(priv, cCb);

    // Cr
    process_Huffman_data_unit(priv, cCr);
}



/*
 * Decode a 2x2
 *  .-------.
 *  | 1 | 2 |
 *  |---+---|
 *  | 3 | 4 |
 *  `-------'
 */
static void decode_MCU_2x2_3planes(struct jdec_private *priv)
{
    // Y
    process_Huffman_data_unit(priv, cY);
    process_Huffman_data_unit(priv, cY);
    process_Huffman_data_unit(priv, cY);
    process_Huffman_data_unit(priv, cY);

    // Cb
    process_Huffman_data_unit(priv, cCb);

    // Cr
    process_Huffman_data_unit(priv, cCr);
}


/*
 * Decode a 1x2 mcu
 *  .---.
 *  | 1 |
 *  |---|
 *  | 2 |
 *  `---'
 */
static void decode_MCU_1x2_3planes(struct jdec_private *priv)
{
    // Y
    process_Huffman_data_unit(priv, cY);
    process_Huffman_data_unit(priv, cY);

    // Cb
    process_Huffman_data_unit(priv, cCb);

    // Cr
    process_Huffman_data_unit(priv, cCr);
}

static const decode_MCU_fct decode_mcu_3comp_table[4] = {
        decode_MCU_1x1_3planes,
        decode_MCU_1x2_3planes,
        decode_MCU_2x1_3planes,
        decode_MCU_2x2_3planes,
};





/**
 * Decode and convert the jpeg image into @pixfmt@ image
 *
 * Note: components will be automaticaly allocated if no memory is attached.
 */

int tinyjpeg_decode(struct jdec_private *priv)
{
    unsigned int x, y, xstride_by_mcu, ystride_by_mcu;
    unsigned int bytes_per_blocklines[3], bytes_per_mcu[3];
    decode_MCU_fct decode_MCU;
    const decode_MCU_fct *decode_mcu_table;

    if (setjmp(priv->jump_state))
        return -1;

    /* To keep gcc happy initialize some array */
    bytes_per_mcu[1] = 0;
    bytes_per_mcu[2] = 0;
    bytes_per_blocklines[1] = 0;
    bytes_per_blocklines[2] = 0;

    decode_mcu_table = decode_mcu_3comp_table;
    xstride_by_mcu = ystride_by_mcu = 8;
    if ((priv->component_infos[cY].Hfactor | priv->component_infos[cY].Vfactor) == 1) {
        decode_MCU = decode_mcu_table[0];
        printf("Use decode 1x1 sampling\n");
    } else if (priv->component_infos[cY].Hfactor == 1) {
        decode_MCU = decode_mcu_table[1];
        ystride_by_mcu = 16;
        printf("Use decode 1x2 sampling (not supported)\n");
        printf("Not supported now!!!!!!!!\n");
        exit(0);
    } else if (priv->component_infos[cY].Vfactor == 2) {
        decode_MCU = decode_mcu_table[3];
        xstride_by_mcu = 16;
        ystride_by_mcu = 16;
        printf("Use decode 2x2 sampling\n");
        Dir_Record->dir_huff[workon].type=1;
//        printf("Not supported now!!!!!!!!\n");
//        exit(0);
    } else {
        decode_MCU = decode_mcu_table[2];
        xstride_by_mcu = 16;
        printf("Use decode 2x1 sampling\n");
        printf("Not supported now!!!!!!!!\n");
        exit(0);
    }

    resync(priv);


    int temp_height;
    if(priv->height%ystride_by_mcu!=0){
        temp_height=priv->height/ystride_by_mcu+1;
    } else{
        temp_height=priv->height/ystride_by_mcu;
    }

    /* Just the decode the image by macroblock (size is 8x8, 8x16, or 16x16) */
    for (y=0; y < temp_height; y++)
    {
        for (x=0; x < priv->width+1; x+=xstride_by_mcu)
        {
            decode_MCU(priv);

            const unsigned char *streamtest = priv->stream;

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

//    printf("Input file size: %d\n", priv->stream_length+2);
//    printf("Input bytes actually read: %d\n", priv->stream - priv->stream_begin + 2);

    return 0;
}



//____________________________________________________________

#include <climits>
#include <bitset>





HUFF_VAL_USEFUL *huff_val_useful=(HUFF_VAL_USEFUL*)malloc(sizeof(HUFF_VAL_USEFUL));

void build_huff_val_useful_test(){
    int a=0,b=0,c=0,d=0;

    for(int i=0;i<Dir_Record->dir_huff[0].jpeg_huff[0].one_huff_num;i++) {
        huff_val_useful->DC0.val[a] = Dir_Record->dir_huff[0].jpeg_huff[0].huff_table[i].val;
        huff_val_useful->DC0.code[a] = (char*)malloc(sizeof(char)*32);
        strcpy(huff_val_useful->DC0.code[a], tobin(Dir_Record->dir_huff[0].jpeg_huff[0].huff_table[i].code));
        a++;
        huff_val_useful->DC0.count = a;
    }
    for(int i=0;i<Dir_Record->dir_huff[0].jpeg_huff[1].one_huff_num;i++) {
        huff_val_useful->DC1.val[b] = Dir_Record->dir_huff[0].jpeg_huff[1].huff_table[i].val;
        huff_val_useful->DC1.code[b] = (char*)malloc(sizeof(char)*32);
        strcpy(huff_val_useful->DC1.code[b], tobin(Dir_Record->dir_huff[0].jpeg_huff[1].huff_table[i].code));
        b++;
        huff_val_useful->DC1.count = b;
    }
    for(int i=0;i<Dir_Record->dir_huff[0].jpeg_huff[2].one_huff_num;i++) {
        huff_val_useful->AC0.val[c] = Dir_Record->dir_huff[0].jpeg_huff[2].huff_table[i].val;
        huff_val_useful->AC0.code[c] = (char*)malloc(sizeof(char)*32);
        strcpy(huff_val_useful->AC0.code[c], tobin(Dir_Record->dir_huff[0].jpeg_huff[2].huff_table[i].code));
        c++;
        huff_val_useful->AC0.count = c;
    }
    for(int i=0;i<Dir_Record->dir_huff[0].jpeg_huff[3].one_huff_num;i++) {
        huff_val_useful->AC1.val[d]=Dir_Record->dir_huff[0].jpeg_huff[3].huff_table[i].val;
        huff_val_useful->AC1.code[d]=(char*)malloc(sizeof(char)*32);
        strcpy(huff_val_useful->AC1.code[d],tobin(Dir_Record->dir_huff[0].jpeg_huff[3].huff_table[i].code));
        d++;
        huff_val_useful->AC1.count=d;
    }




}

void build_huff_val_useful(){
    int a=0,b=0,c=0,d=0,e=0;
    for(int i=0;i<256;i++){
        huff_val.RECORD[i]=huff_val.DC0[i]+huff_val.DC1[i]+huff_val.AC0[i]+huff_val.AC1[i];
    }

    for(int i=0;i<256;i++){
        if(huff_val.DC0[i]!=0){
            huff_val_useful->DC0.val[a]=i;
            huff_val_useful->DC0.num[a]=huff_val.DC0[i];
            a++;
            huff_val_useful->DC0.count=a;
        }
        if(huff_val.DC1[i]!=0){
            huff_val_useful->DC1.val[b]=i;
            huff_val_useful->DC1.num[b]=huff_val.DC1[i];
            b++;
            huff_val_useful->DC1.count=b;
        }
        if(huff_val.AC0[i]!=0){
            huff_val_useful->AC0.val[c]=i;
            huff_val_useful->AC0.num[c]=huff_val.AC0[i];
            c++;
            huff_val_useful->AC0.count=c;
        }
        if(huff_val.AC1[i]!=0){
            huff_val_useful->AC1.val[d]=i;
            huff_val_useful->AC1.num[d]=huff_val.AC1[i];
            d++;
            huff_val_useful->AC1.count=d;
        }
        if(huff_val.RECORD[i]!=0){
            huff_val_useful->RECORD.val[e]=i;
            huff_val_useful->RECORD.num[e]=huff_val.RECORD[i];
            e++;
            huff_val_useful->RECORD.count=e;
        }
    }
}


void BubbleSort(long long *p, int length, int * index)
{
    for (int m = 0; m < length; m++)
    {
        index[m] = m;
    }

    for (int i = 0; i < length; i++)
    {
        for (int j = 0; j < length- i - 1; j++)
        {
            if (p[j] < p[j + 1])
            {
                float temp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = temp;

                int ind_temp = index[j];
                index[j] = index[j + 1];
                index[j + 1] = ind_temp;
            }
        }
    }
}

//
////int one_size_max[17]={0,1,0,1,3,3,2,4,2,7,4,8,4,0,0,0,256};
//int one_size_max[17]={0,1,0,1,3,3,2,4,2,7,4,8,4,0,0,0,256};
////int one_size_max_temp[17]= {0};
//int one_size_max_record[17]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
//
//
//int cal_one_size_max(int size){
//    if(size==1){
//        return 2;
//    }else{
//        return (cal_one_size_max(size-1)-one_size_max[size-1])*2;
//    }
//}
//
//void cal_best_huffman(){
//    for(int i=1;i<17;i++){
//        one_size_max[i]=cal_one_size_max(i);
//        huff_val_useful->RECORD.count-=one_size_max[i];
//        if(huff_val_useful->RECORD.count>0){
//            one_size_max_record[i]++;//记录退1
//            one_size_max[i]-=one_size_max_record[i];//当前位置退1
//            huff_val_useful->RECORD.count+=one_size_max_record[i];
//        }
//    }
//
//    if(huff_val_useful->RECORD.count>0){
//        for(int j=1;j<17;j++){
//            one_size_max[j]=0;
//            for(int i=j+1;i<17;i++){
//                one_size_max_record[i]=0;
//                one_size_max[i]=cal_one_size_max(i);
//                huff_val_useful->RECORD.count-=one_size_max[i];
//                if(huff_val_useful->RECORD.count>0){
//                    one_size_max_record[i]++;//记录退1
//                    one_size_max[i]-=one_size_max_record[i];//当前位置退1
//                    huff_val_useful->RECORD.count+=one_size_max_record[i];
//                }
//            }
//
//            for(int k=1;k<17;k++){
//                printf("%d ",one_size_max[k]);
//            }
//            printf("\n");
//        }
//
//    }
//
//}


//void cal_new_huffman(char* arr[],int length){
//
//    long long huffcode=0;
//    int huffcode_length=1;
//    int size=1;
//    int size_num=0;
////    strcpy(arr[0],"00");
////    strcpy(arr[1],"01");
//    cal_best_huffman();
//    char* str;
//    str=(char*)malloc(sizeof(char)*32);
//
//    for(int i=0;i<length;i++){
//        if(size_num<one_size_max[size]){
//            if(size_num!=0){
//                huffcode++;
//            }
//            strcpy(str,"");
//            strcpy(str,tobin(huffcode));
//            if(strlen(str)<size){
//                int temp_len=strlen(str);
//                for(int j=temp_len;j>=size-temp_len;j--){
//                    str[j]=str[j-1];
//                }
//                for(int j=size-temp_len-1;j>-1;j--){
//                    str[j]='0';
//                }
//            }
//            strcpy(arr[i],str);
//            size_num++;
//        }else{//超过当前数量
//            size_num=0;
//            if(one_size_max[size]!=0){
//                huffcode++;
//            }
//            size++;
//            huffcode*=2;
//            i--;
//        }
//
//    }
//
////    for(int i=2;i<length;i++){
////        if(size_num<one_size_max[size]){
////            huffcode++;
////            strcpy(str,"");
////            strcpy(str,tobin(huffcode));
////        } else{//超过当前数量
////            size_num=0;
////            size++;
////            huffcode++;
////            huffcode*=2;
////            strcpy(str,"");
////            strcpy(str,tobin(huffcode));
////        }
////        strcpy(arr[i],str);
////        size_num++;
////    }
//
//
////    long long huffcode=1;
////    int huffcode_length=3;
////    int size=2;
////    int size_num=2;
////    strcpy(arr[0],"00");
////    strcpy(arr[1],"01");
////
////    char* str;
////    str=(char*)malloc(sizeof(char)*32);
////    for(int i=2;i<length;i++){
////
////        if(size_num<one_size_max[size]){
////            if(size_num!=0){
////                huffcode++;
////            }
////            strcpy(str,"");
////            strcpy(str,tobin(huffcode));
////            strcpy(arr[i],str);
////            size_num++;
////        }else{//超过当前数量
////            size_num=0;
////            if(one_size_max[size]!=0){
////                huffcode++;
////            }
////            size++;
////            huffcode*=2;
////            i--;
////        }
////
////    }
//
//    free(str);
//}



//int one_size_max[17]={0,1,0,1,3,3,2,4,2,7,4,8,4,0,0,0,256};
//int one_size_max[17]={0,1,1,1,3,3,2,4,2,0,0,0,0,0,0,0,256};
int one_size_max[17]={0,1,0,1,3,3,2,4,7,0,0,0,0,0,0,0,256};

void cal_new_huffman(char* arr[],int length){
    long long huffcode=0;
    int huffcode_length=1;
    int size=1;
    int size_num=0;
    char* str;
    str=(char*)malloc(sizeof(char)*32);

    for(int i=0;i<length;i++){
        if(size_num<one_size_max[size]){
            if(size_num!=0){
                huffcode++;
            }
            strcpy(str,"");
            strcpy(str,tobin(huffcode));
            if(strlen(str)<size){
                int temp_len=strlen(str);
                for(int j=temp_len;j>=size-temp_len;j--){
                    str[j]=str[j-1];
                }
                for(int j=size-temp_len-1;j>-1;j--){
                    str[j]='0';
                }
            }
            strcpy(arr[i],str);
            size_num++;
        }else{//超过当前数量
            size_num=0;
            if(one_size_max[size]!=0){
                huffcode++;
            }
            size++;
            huffcode*=2;
            i--;
        }

    }

    free(str);
}

void move_array(int arr[],int index[],int length){
    int *temp=(int*)malloc(sizeof(int)*length);
    for(int i=0;i<length;i++){
        temp[i]=arr[index[i]];
    }
    for(int i=0;i<length;i++){
        arr[i]=temp[i];
    }
}

void build_DC0_tree(){
    printf("\nDC0\n");
    const int length=huff_val_useful->DC0.count;
    for (int i = 0; i < length; ++i) {
        huff_val_useful->DC0.code[i]=(char*)malloc(sizeof(char)*32);
    }
    int *index=(int*)malloc(sizeof(int)*length);
    BubbleSort(huff_val_useful->DC0.num,length,index);
    move_array(huff_val_useful->DC0.val,index,length);
    cal_new_huffman(huff_val_useful->DC0.code,length);
    for (int i = 0; i < length; ++i) {
        printf("%02x,%s,%ld\n",huff_val_useful->DC0.val[i],huff_val_useful->DC0.code[i],strlen(huff_val_useful->DC0.code[i]));
    }
}

void build_DC1_tree(){
    printf("\nDC1\n");
    const int length=huff_val_useful->DC1.count;
    for (int i = 0; i < length; ++i) {
        huff_val_useful->DC1.code[i]=(char*)malloc(sizeof(char)*32);
    }
    int *index=(int*)malloc(sizeof(int)*length);
    BubbleSort(huff_val_useful->DC1.num,length,index);
    move_array(huff_val_useful->DC1.val,index,length);
    cal_new_huffman(huff_val_useful->DC1.code,length);
    for (int i = 0; i < length; ++i) {
        printf("%02x,%s,%ld\n",huff_val_useful->DC1.val[i],huff_val_useful->DC1.code[i],strlen(huff_val_useful->DC1.code[i]));
    }
}

void build_AC0_tree(){
    printf("\nAC0\n");
    const int length=huff_val_useful->AC0.count;
    for (int i = 0; i < length; ++i) {
        huff_val_useful->AC0.code[i]=(char*)malloc(sizeof(char)*32);
    }
    int *index=(int*)malloc(sizeof(int)*length);
    BubbleSort(huff_val_useful->AC0.num,length,index);
    move_array(huff_val_useful->AC0.val,index,length);
    cal_new_huffman(huff_val_useful->AC0.code,length);
    for (int i = 0; i < length; ++i) {
        printf("%02x,%s,%ld\n",huff_val_useful->AC0.val[i],huff_val_useful->AC0.code[i],strlen(huff_val_useful->AC0.code[i]));
    }
}

void build_AC1_tree(){
    printf("\nAC1\n");
    const int length=huff_val_useful->AC1.count;
    for (int i = 0; i < length; ++i) {
        huff_val_useful->AC1.code[i]=(char*)malloc(sizeof(char)*32);
    }
    int *index=(int*)malloc(sizeof(int)*length);
    BubbleSort(huff_val_useful->AC1.num,length,index);
    move_array(huff_val_useful->AC1.val,index,length);
    cal_new_huffman(huff_val_useful->AC1.code,length);
    for (int i = 0; i < length; ++i) {
        printf("%02x,%s,%ld\n",huff_val_useful->AC1.val[i],huff_val_useful->AC1.code[i],strlen(huff_val_useful->AC1.code[i]));
        if(strlen(huff_val_useful->AC1.code[i])>16){
            printf("size>16 You have to check the one_size_max\n");
            exit(0);
        }
    }
}


//__________________________________________________________


bitset<8> full_string_8(char* str){
    int code_size=strlen(str);
    bitset<8> return_code("00000000");
    char a;
    for(int i=0;i<code_size;i++){
        a= (str[code_size - 1-i]);
        return_code[i]= a-'0';
    }
    return return_code;
}

bitset<8> head_string(char* write_code, int bits){
    int code_bits=strlen(write_code);
    bitset<8> return_code("00000000");
    int code_size=strlen(write_code);
    char a;
    for(int i=0;i<code_bits-bits;i++){
        a= (write_code[code_size - 1 - i - bits]);
        return_code[i]= a-'0';
    }
    return return_code;
}

bitset<8> use8_string(char* write_code,int bits){
    bitset<8> return_code("00000000");
    int code_size=strlen(write_code);
    char a;

    for(int i=0;i<8;i++){
        a= (write_code[code_size - 1 - i - bits]);
        return_code[i]= a-'0';
    }
    return return_code;
}

bitset<8> tail_string(char* write_code,int bits){
    bitset<8> return_code("00000000");
    int code_size=strlen(write_code);
    char a;

    for(int i=0;i<bits;i++){
        a= (write_code[code_size - 1 - i]);
        return_code[i]= a-'0';
    }
    return return_code;
}


bitset<8> WriteNewData_Stream("00000000");

int WriteNewData_Stream_Bits=0;
char lastbyte;


void Write_Stream_To_File(char* write_code,FILE *fp){
    unsigned char byte;
//    unsigned char last_byte;
    unsigned long temp;
//    unsigned long last_temp;
    int code_bits=strlen(write_code);

    int notwrite_code_bits=code_bits;
    bitset<8> temp_code("00000000");
    if(code_bits>(8-WriteNewData_Stream_Bits)){//不能塞下，则先补满
        WriteNewData_Stream.operator<<=(8-WriteNewData_Stream_Bits);
        notwrite_code_bits=code_bits-(8-WriteNewData_Stream_Bits);
        temp_code.operator|=(head_string((write_code), (notwrite_code_bits)));
        WriteNewData_Stream.operator|=(temp_code);
        temp=WriteNewData_Stream.to_ulong();//转换为long类型
        byte=temp;//转换为char类型

        if(byte==0xff){
            fprintf(fp,"%c",byte);
            fprintf(fp,"%c",0x00);
            lastbyte=0x00;
        }else{
            if(lastbyte==0xb9||byte==0x00){
//                printf("aaa\n");
            }
            fprintf(fp,"%c",byte);
            lastbyte=byte;
        }

        WriteNewData_Stream.operator<<=(8);
        temp_code.operator<<=(8);
        WriteNewData_Stream_Bits=0;


        if(notwrite_code_bits>=8){//补满后剩下的大于8位，则再次补满
            notwrite_code_bits=notwrite_code_bits-8;//3
            temp_code.operator|=(use8_string((write_code),(notwrite_code_bits)));
            WriteNewData_Stream.operator|=(temp_code);
            temp=WriteNewData_Stream.to_ulong();//转换为long类型
            byte=temp;//转换为char类型

            if(byte==0xff){
                fprintf(fp,"%c",byte);
                fprintf(fp,"%c",0x00);
                lastbyte=0x00;
            }else{
                fprintf(fp,"%c",byte);
                lastbyte=byte;
            }

            WriteNewData_Stream.operator<<=(8);
            temp_code.operator<<=(8);
            WriteNewData_Stream_Bits=0;

        }
        if(notwrite_code_bits>=0){//不大于8位，则先填入，或补满后还有，则先填入
            temp_code.operator|=(tail_string(write_code,notwrite_code_bits));
            WriteNewData_Stream.operator|=(temp_code);
            WriteNewData_Stream_Bits=notwrite_code_bits;
            temp_code.operator<<=(8);
        }
    }else{//直接可以塞下
        WriteNewData_Stream.operator<<=(code_bits);
        WriteNewData_Stream.operator|=(tail_string(write_code,notwrite_code_bits));
        WriteNewData_Stream_Bits+=code_bits;
        temp_code.operator<<=(8);
    }
}


static int WriteNewData_get_next_huffman_code(struct jdec_private *priv, struct huffman_table *huffman_table,FILE *fp)
{
    int value, hcode;
    unsigned int extra_nbits, nbits;
    uint16_t *slowtable;

//    look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, HUFFMAN_HASH_NBITS, hcode);
    while (priv->nbits_in_reservoir<HUFFMAN_HASH_NBITS)
    {
        unsigned char c;
        if (priv->stream >= priv->stream_end)
            longjmp(priv->jump_state, -EIO);
        c = *priv->stream++;
        priv->reservoir <<= 8;
        if (c == 0xff && *priv->stream == 0x00)
            priv->stream++;
        priv->reservoir |= c;
        priv->nbits_in_reservoir+=8;
    }
    hcode = ((priv->reservoir)>>(priv->nbits_in_reservoir-(HUFFMAN_HASH_NBITS)));


    value = huffman_table->lookup[hcode];//val,在快速查找中

    if (__likely(value >= 0))
    {
        unsigned int code_size = huffman_table->code_size[value];
//        skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, code_size);//跳过val的codesize，即跳过他自己

        priv->nbits_in_reservoir -= (code_size);
        priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);



        return value;//返回val

    }

    /* Decode more bits each time ... */
    for (extra_nbits=0; extra_nbits<16-HUFFMAN_HASH_NBITS; extra_nbits++)
    {//val不在快速查找表中
        nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;

//        look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits, hcode);
        while (priv->nbits_in_reservoir<nbits)
        {
            unsigned char c;
            if (priv->stream >= priv->stream_end)
                longjmp(priv->jump_state, -EIO);
            c = *priv->stream++;
            priv->reservoir <<= 8;
            if (c == 0xff && *priv->stream == 0x00)
                priv->stream++;
            priv->reservoir |= c;
            priv->nbits_in_reservoir+=8;
        }
        hcode = ((priv->reservoir)>>(priv->nbits_in_reservoir-(nbits)));


        slowtable = huffman_table->slowtable[extra_nbits];
        /* Search if the code is in this array */
        while (slowtable[0]) {
            if (slowtable[0] == hcode) {
//                skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits);
                priv->nbits_in_reservoir -= (nbits);
                priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);


                return slowtable[1];
            }
            slowtable+=2;
        }
    }
    return 0;
}


char* itoa (long long n)
{


    long long i,sign;
    if((sign=n)<0)//记录符号
        n=-n;//使n成为正数
    i=0;
    char *s;
    char *str;
    s=(char*)malloc(sizeof(char)*32);

    str=(char*)malloc(sizeof(char)*32);
    do{
        s[i++]=n%10+'0';//取下一个数字
    }
    while ((n/=10)>0);//删除该数字
    s[i]='\0';
    int len_s=strlen(s);
    for(int k=0;k<len_s;k++){
        str[k]=s[len_s-1-k];
        if(sign<0){
            if(str[k]=='1'){
                str[k]='0';
            } else {
                str[k]='1';
            }
        }
    }
    str[strlen(s)]='\0';
    memset(s,'\0', sizeof(s));
    return str;
}


char* tobin(long long n)
{
    long long binaryNumber = 0;
    long long remainder, i = 1, step = 1;
    char *str=(char*)malloc(sizeof(char)*32);
    while (n!=0)
    {
        remainder = n%2;
//        printf("Step %d: %d/2, 余数 = %d, 商 = %d\n", step++, n, remainder, n/2);
        n /= 2;
        binaryNumber += remainder*i;
        i *= 10;
    }
    strcpy(str,itoa(binaryNumber));
    return str;

}


char*  Find_New_code(int val,int type){
    switch (type){
        case 0:
            for(int i=0;i<256;i++){
                if(huff_val_useful->DC0.val[i]==val){
                    return huff_val_useful->DC0.code[i];
                }
            }
            break;
        case 1:
            for(int i=0;i<256;i++){
                if(huff_val_useful->DC1.val[i]==val){
                    return huff_val_useful->DC1.code[i];
                }
            }
            break;
        case 2:
            for(int i=0;i<256;i++){
                if(huff_val_useful->AC0.val[i]==val){
                    return huff_val_useful->AC0.code[i];
                }
            }
            break;
        case 3:
            for(int i=0;i<256;i++){
                if(huff_val_useful->AC1.val[i]==val){
                    return huff_val_useful->AC1.code[i];
                }
            }
            break;
        default:
            break;
    }

}




static void WriteNewData_process_Huffman_data_unit(struct jdec_private *priv, int component,FILE *fp)
{
    unsigned char j;
    unsigned int huff_code;
    unsigned char size_val, count_0;

    struct component *c = &priv->component_infos[component];
    short int DCT[64];


    /* Initialize the DCT coef table */
    memset(DCT, 0, sizeof(DCT));

    char* write_code;

    write_code=(char*)malloc(sizeof(char)*16);

    /* DC coefficient decoding */
    huff_code = WriteNewData_get_next_huffman_code(priv, c->DC_table,fp);
//    if(huff_code!=0)
    switch (component){
        case 0:
//            huff_val_useful->DC0.code[i-1]= & HC[i];
            strcpy(write_code,"");
            strcpy(write_code,Find_New_code(huff_code,0));
//            printf("%s\n",*write_code);
            break;
        case 1:
            strcpy(write_code,"");
            strcpy(write_code,Find_New_code(huff_code,1));
//            printf("%s\n",*write_code);
            break;
        case 2:
            strcpy(write_code,"");
            strcpy(write_code,Find_New_code(huff_code,1));
//            printf("%s\n",*write_code);
            break;
    }
    Write_Stream_To_File(write_code,fp);


    if (huff_code) {
//        get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, huff_code, DCT[0]);//再读huffman个
        while (priv->nbits_in_reservoir<huff_code)
        {
            unsigned char c;
            if (priv->stream >= priv->stream_end)
                longjmp(priv->jump_state, -EIO);
            c = *priv->stream++;
            priv->reservoir <<= 8;
            if (c == 0xff && *priv->stream == 0x00)
                priv->stream++;
            //爲了與標記碼做區別，在讀取壓縮圖像數據時，若讀取到一個 byte 爲 0xFF ，則後面會緊接一個 0x00 的 byte，但這個 0x00 的 byte 不是數據，必須忽略。
            priv->reservoir |= c;
            priv->nbits_in_reservoir+=8;
        }
        DCT[0] = ((priv->reservoir)>>(priv->nbits_in_reservoir-(huff_code)));
        priv->nbits_in_reservoir -= (huff_code);
        priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);
        if ((unsigned int)DCT[0] < (1UL<<((huff_code)-1)))
            DCT[0] += (0xFFFFFFFFUL<<(huff_code))+1;
//        HuffmanCode write_code;
        strcpy(write_code,"");
        strcpy(write_code,tobin(DCT[0]));
        Write_Stream_To_File(write_code,fp);
//        fclose(fp);

        DCT[0] += c->previous_DC;//直流系数差分还原
        c->previous_DC = DCT[0];
    } else {
        DCT[0] = c->previous_DC;
    }

    /* AC coefficient decoding */
    j = 1;
    while (j<64)
    {
        huff_code = WriteNewData_get_next_huffman_code(priv, c->AC_table,fp);
//        if(huff_code!=0)

        switch (component){
            case 0:
//            huff_val_useful->DC0.code[i-1]= & HC[i];
                strcpy(write_code,"");
                strcpy(write_code,Find_New_code(huff_code,2));
//            printf("%s\n",*write_code);
                break;
            case 1:
                strcpy(write_code,"");
                strcpy(write_code,Find_New_code(huff_code,3));
//            printf("%s\n",*write_code);
                break;
            case 2:
                strcpy(write_code,"");
                strcpy(write_code,Find_New_code(huff_code,3));
//            printf("%s\n",*write_code);
                break;
        }
        Write_Stream_To_File(write_code,fp);


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
//            get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, size_val, DCT[j]);
            while (priv->nbits_in_reservoir<size_val)
            {
                unsigned char c;
                if (priv->stream >= priv->stream_end)
                    longjmp(priv->jump_state, -EIO);
                c = *priv->stream++;
                priv->reservoir <<= 8;
                if (c == 0xff && *priv->stream == 0x00)
                    priv->stream++;
                priv->reservoir |= c;
                priv->nbits_in_reservoir+=8;
            }
            DCT[j] = ((priv->reservoir)>>(priv->nbits_in_reservoir-(size_val)));
            priv->nbits_in_reservoir -= (size_val);
            priv->reservoir &= ((1U<<priv->nbits_in_reservoir)-1);
            if ((unsigned int)DCT[j] < (1UL<<((size_val)-1)))
                DCT[j] += (0xFFFFFFFFUL<<(size_val))+1;//转为负数
            strcpy(write_code,"");
            strcpy(write_code,tobin(DCT[j]));
            Write_Stream_To_File(write_code,fp);
            j++;
        }
    }

    for (j = 0; j < 64; j++)
        c->DCT[j] = DCT[j];

    free(write_code);
}

/*
 * Decode all the 3 components for 1x1
 */
static void WriteNewData_decode_MCU_1x1_3planes(struct jdec_private *priv,FILE *fp)
{
    // Y
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);

    // Cb
    WriteNewData_process_Huffman_data_unit(priv, cCb,fp);

    // Cr
    WriteNewData_process_Huffman_data_unit(priv, cCr,fp);
}



/*
 * Decode a 2x1
 *  .-------.
 *  | 1 | 2 |
 *  `-------'
 */
static void WriteNewData_decode_MCU_2x1_3planes(struct jdec_private *priv,FILE *fp)
{
    // Y
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);


    // Cb
    WriteNewData_process_Huffman_data_unit(priv, cCb,fp);

    // Cr
    WriteNewData_process_Huffman_data_unit(priv, cCr,fp);
}



/*
 * Decode a 2x2
 *  .-------.
 *  | 1 | 2 |
 *  |---+---|
 *  | 3 | 4 |
 *  `-------'
 */
static void WriteNewData_decode_MCU_2x2_3planes(struct jdec_private *priv,FILE *fp)
{
    // Y
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);


    // Cb
    WriteNewData_process_Huffman_data_unit(priv, cCb,fp);

    // Cr
    WriteNewData_process_Huffman_data_unit(priv, cCr,fp);
}


/*
 * Decode a 1x2 mcu
 *  .---.
 *  | 1 |
 *  |---|
 *  | 2 |
 *  `---'
 */
static void WriteNewData_decode_MCU_1x2_3planes(struct jdec_private *priv,FILE *fp)
{
    // Y
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);
    WriteNewData_process_Huffman_data_unit(priv, cY,fp);


    // Cb
    WriteNewData_process_Huffman_data_unit(priv, cCb,fp);

    // Cr
    WriteNewData_process_Huffman_data_unit(priv, cCr,fp);
}

static const WriteNewData_decode_MCU_fct WriteNewData_decode_mcu_3comp_table[4] = {
        WriteNewData_decode_MCU_1x1_3planes,
        WriteNewData_decode_MCU_1x2_3planes,
        WriteNewData_decode_MCU_2x1_3planes,
        WriteNewData_decode_MCU_2x2_3planes,
};



static int Write_RST(struct jdec_private *priv,FILE *fp)
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
//    printf("RST Marker %d found at offset %d\n", priv->last_rst_marker_seen, stream - priv->stream_begin);

    priv->stream = stream;




    int move=8-WriteNewData_Stream_Bits;

    if(move<8&&move>0){
        WriteNewData_Stream.operator<<=(move);
        for(int i=0;i<move;i++){
            WriteNewData_Stream[i]=1;
        }
        unsigned char byte;
        unsigned long temp;

        temp=WriteNewData_Stream.to_ulong();//转换为long类型
        byte=temp;//转换为char类型

        fprintf(fp,"%c",byte);
    } else if (move==0){
        unsigned char byte;
        unsigned long temp;

        temp=WriteNewData_Stream.to_ulong();//转换为long类型
        byte=temp;//转换为char类型

        fprintf(fp,"%c",byte);
    }



    fprintf(fp,"%c",0xff);

    fprintf(fp,"%c",RST+priv->last_rst_marker_seen);
    priv->last_rst_marker_seen++;
    priv->last_rst_marker_seen &= 7;
    WriteNewData_Stream.operator<<=(8);
    WriteNewData_Stream_Bits=0;

}

int WriteNewData_workon;
int Write_New_Body(struct jdec_private *priv, FILE *fp)
{
    printf("Write New Data Start!\n");
    fprintf(fp,"%c",0xff);
    fprintf(fp,"%c",0xda);
    fprintf(fp,"%c",0x00);
    fprintf(fp,"%c",0x0c);
    fprintf(fp,"%c",0x03);
    if(Dir_Record->dir_huff[WriteNewData_workon].type==0){
        fprintf(fp,"%c",0x00);
        fprintf(fp,"%c",0x00);
        fprintf(fp,"%c",0x01);
        fprintf(fp,"%c",0x11);
        fprintf(fp,"%c",0x02);
        fprintf(fp,"%c",0x11);
    } else{
        fprintf(fp,"%c",0x01);
        fprintf(fp,"%c",0x00);
        fprintf(fp,"%c",0x02);
        fprintf(fp,"%c",0x11);
        fprintf(fp,"%c",0x03);
        fprintf(fp,"%c",0x11);
    }

    fprintf(fp,"%c",0x00);
    fprintf(fp,"%c",0x3f);
    fprintf(fp,"%c",0x00);

    unsigned int x, y, xstride_by_mcu, ystride_by_mcu;
    unsigned int bytes_per_blocklines[3], bytes_per_mcu[3];
    WriteNewData_decode_MCU_fct WriteNewData_decode_MCU;
    const WriteNewData_decode_MCU_fct *WriteNewData_decode_mcu_table;
    if (setjmp(priv->jump_state))
        return -1;

    /* To keep gcc happy initialize some array */
    bytes_per_mcu[1] = 0;
    bytes_per_mcu[2] = 0;
    bytes_per_blocklines[1] = 0;
    bytes_per_blocklines[2] = 0;
    WriteNewData_decode_mcu_table = WriteNewData_decode_mcu_3comp_table;
    xstride_by_mcu = ystride_by_mcu = 8;
    if ((priv->component_infos[cY].Hfactor | priv->component_infos[cY].Vfactor) == 1) {
        WriteNewData_decode_MCU = WriteNewData_decode_mcu_table[0];
        printf("Use decode 1x1 sampling\n");
    } else if (priv->component_infos[cY].Hfactor == 1) {
        WriteNewData_decode_MCU = WriteNewData_decode_mcu_table[1];
        ystride_by_mcu = 16;
        printf("Use decode 1x2 sampling (not supported)\n");
    } else if (priv->component_infos[cY].Vfactor == 2) {
        WriteNewData_decode_MCU = WriteNewData_decode_mcu_table[3];
        xstride_by_mcu = 16;
        ystride_by_mcu = 16;
        printf("Use decode 2x2 sampling\n");
    } else {
        WriteNewData_decode_MCU = WriteNewData_decode_mcu_table[2];
        xstride_by_mcu = 16;
        printf("Use decode 2x1 sampling\n");
    }

    resync(priv);


    int temp_height;
    if(priv->height%ystride_by_mcu!=0){
        temp_height=priv->height/ystride_by_mcu+1;
    } else{
        temp_height=priv->height/ystride_by_mcu;
    }

    /* Just the decode the image by macroblock (size is 8x8, 8x16, or 16x16) */
    for (y=0; y < temp_height; y++)
    {
        for (x=0; x < priv->width; x+=xstride_by_mcu)
        {
            WriteNewData_decode_MCU(priv,fp);


            if (priv->restarts_to_go>0)
            {
                priv->restarts_to_go--;
                if (priv->restarts_to_go == 0)
                {
                    priv->stream -= (priv->nbits_in_reservoir/8);
                    resync(priv);
                    Write_RST(priv,fp);
                }
            }
        }
    }
    printf("Write New Data Finish!\n");
    return 0;
}
void Write_Huffman_Table(FILE *fp);
/**
 * Load one jpeg image, and decompress it, and save the result.
 */
void WriteNewData_convert_one_image(const char *infilename)
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


    char path[1000];
    memset(path, '\0', sizeof(path));
    strcpy(path,Dir_Record->dir_huff[WriteNewData_workon].FullPathName);
    strcat(path,".jpg");
    FILE *fpdat=fopen(path,"w+");

    write_head_data(infilename,fpdat);

    Write_Huffman_Table(fpdat);

    //解码实际数据
    if (Write_New_Body(jdec, fpdat) < 0)
        printf(tinyjpeg_get_printfstring(jdec));
//写完数据后清空存储区

    int move=8-WriteNewData_Stream_Bits;

    if(move<8&&move>0){
        WriteNewData_Stream.operator<<=(move);
        for(int i=0;i<move;i++){
            WriteNewData_Stream[i]=1;
        }
        unsigned char byte;
        unsigned long temp;

        temp=WriteNewData_Stream.to_ulong();//转换为long类型
        byte=temp;//转换为char类型

        fprintf(fp,"%c",byte);
        if(byte==0xff){
            byte=0x00;
            fprintf(fp,"%c",byte);
        }
    } else if (move==0){
        unsigned char byte;
        unsigned long temp;

        temp=WriteNewData_Stream.to_ulong();//转换为long类型
        byte=temp;//转换为char类型

        fprintf(fp,"%c",byte);
        if(byte==0xff){
            byte=0x00;
            fprintf(fp,"%c",byte);
        }
    }
    WriteNewData_Stream.operator<<=(8);
    WriteNewData_Stream_Bits=0;


    //写入ffd9
    fprintf(fp,"%c",0xff);
    fprintf(fp,"%c",0xd9);

    fclose(fpdat);
    free(buf);

}

void Write_One_Huffman_Table(char* str,FILE *fp){
    unsigned char byte;
    unsigned long temp;
    WriteNewData_Stream.operator|=(full_string_8(str));
    temp=WriteNewData_Stream.to_ulong();//转换为long类型
    byte=temp;//转换为char类型
    fprintf(fp,"%c",byte);
    WriteNewData_Stream.operator<<=(8);
    WriteNewData_Stream_Bits=0;
}

void Write_Huffman_Table(FILE *fp){


    char* str;
    str=(char*)malloc(sizeof(str)*32);
//    fprintf(fp,"%c",0xff);
//    fprintf(fp,"%c",0xc4);
    fprintf(fp,"%c",0x00);
    if(Dir_Record->dir_huff[WriteNewData_workon].type==0){
        int table_length=70+huff_val_useful->DC0.count+huff_val_useful->DC1.count+huff_val_useful->AC0.count+huff_val_useful->AC1.count;
        strcpy(str,"");
        strcpy(str,tobin(table_length));
        Write_One_Huffman_Table(str,fp);
    } else{
        int table_length=19+huff_val_useful->DC0.count;
        strcpy(str,"");
        strcpy(str,tobin(table_length));
        Write_One_Huffman_Table(str,fp);
    }



    long long count[17];

    //DC0
    memset(count,0, sizeof(count));
    fprintf(fp,"%c",0x00);
    for(int i=1;i<17;i++){
        for(int j=0;j<huff_val_useful->DC0.count;j++){
            if(strlen(huff_val_useful->DC0.code[j])==i){
                count[i]++;
            }
        }
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(count[i]));
        Write_One_Huffman_Table(str,fp);
    }

    for(int i=0;i<huff_val_useful->DC0.count;i++){
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(huff_val_useful->DC0.val[i]));
        Write_One_Huffman_Table(str,fp);
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
    }




    if(Dir_Record->dir_huff[WriteNewData_workon].type==1){
        fprintf(fp,"%c",0xff);
        fprintf(fp,"%c",0xc4);
        fprintf(fp,"%c",0x00);
        int table_length=19+huff_val_useful->AC0.count;
        strcpy(str,"");
        strcpy(str,tobin(table_length));
        Write_One_Huffman_Table(str,fp);
    }
    //AC0
    memset(count,0, sizeof(count));
    fprintf(fp,"%c",0x10);
    for(int i=1;i<17;i++){
        for(int j=0;j<huff_val_useful->AC0.count;j++){
            if(strlen(huff_val_useful->AC0.code[j])==i){
                count[i]++;
            }
        }
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(count[i]));
        Write_One_Huffman_Table(str,fp);
    }
    for(int i=0;i<huff_val_useful->AC0.count;i++){
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(huff_val_useful->AC0.val[i]));
        Write_One_Huffman_Table(str,fp);
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
    }



    if(Dir_Record->dir_huff[WriteNewData_workon].type==1){
        fprintf(fp,"%c",0xff);
        fprintf(fp,"%c",0xc4);
        fprintf(fp,"%c",0x00);
        int table_length=19+huff_val_useful->DC1.count;
        strcpy(str,"");
        strcpy(str,tobin(table_length));
        Write_One_Huffman_Table(str,fp);
    }

    //DC1
    memset(count,0, sizeof(count));
    fprintf(fp,"%c",0x01);
    for(int i=1;i<17;i++){
        for(int j=0;j<huff_val_useful->DC1.count;j++){
            if(strlen(huff_val_useful->DC1.code[j])==i){
                count[i]++;
            }
        }
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(count[i]));
        Write_One_Huffman_Table(str,fp);
    }

    for(int i=0;i<huff_val_useful->DC1.count;i++){
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(huff_val_useful->DC1.val[i]));
        Write_One_Huffman_Table(str,fp);
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
    }





    if(Dir_Record->dir_huff[WriteNewData_workon].type==1){
        fprintf(fp,"%c",0xff);
        fprintf(fp,"%c",0xc4);
        int table_length=19+huff_val_useful->AC1.count;
        strcpy(str,"");
        strcpy(str,tobin(table_length));
        Write_One_Huffman_Table(str,fp);
    }
    //AC1
    memset(count,0, sizeof(count));
    fprintf(fp,"%c",0x11);
    for(int i=1;i<17;i++){
        for(int j=0;j<huff_val_useful->AC1.count;j++){
            if(strlen(huff_val_useful->AC1.code[j])==i){
                count[i]++;
            }
        }
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(count[i]));
        Write_One_Huffman_Table(str,fp);
    }

    for(int i=0;i<huff_val_useful->AC1.count;i++){
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
        strcpy(str,"");
        strcpy(str,tobin(huff_val_useful->AC1.val[i]));
        Write_One_Huffman_Table(str,fp);
        WriteNewData_Stream.operator<<=(8);
        WriteNewData_Stream_Bits=0;
    }
    free(str);
};

//__________________________________________________________


//解各种不同的标签
static int Witeheaddata_parse_JFIF(struct jdec_private *priv, const unsigned char *stream,FILE*fp)
{
    int chuck_len;
    int marker;
    int flag = 0;
    int dht_marker_found = 0;
    const unsigned char *next_chunck;
    int count=0;
    /* Parse marker */
    //在Start of scan标签之前
    while (!flag)
    {

        if (*stream != 0xff){
            exit(0);
        }
        fprintf(fp,"%c",*stream);
        stream++;

        /* Skip any padding ff byte (this is normal) */
        //跳过0xff字节
        while (*stream == 0xff){
            fprintf(fp,"%c",*stream);
            stream++;
        }
        //marker是跳过0xff字节后1个字节的内容
        marker = *stream;
        fprintf(fp,"%c",*stream);
        stream++;
        //chunk_len是marker后面2个字节的内容（大端模式需要转换）
        //包含自身，但不包含0xff+marker2字节
        chuck_len = be16_to_cpu(stream);//255
        //指向下一个chunk的指针
        next_chunck = stream + chuck_len;
        count++;

        //各种不同的标签
        switch (marker)
        {
            //Define Huffman table
            case DHT:
                flag=1;
                break;
            default:
                break;
        }
        if(flag==1){
            break;
        }
        //解下一个segment
        for(int i=0;i<chuck_len;i++){
            fprintf(fp,"%c",*stream);
            stream++;
        }
//        stream = next_chunck;
    }
    return 0;

}

int Witeheaddata_tinyjpeg_parse_header(struct jdec_private *priv, const unsigned char *buf, unsigned int size,FILE*fp)
{
    int ret;

    /* Identify the file */
    //0x FF D8
    //是否是JPEG格式文件？
    if ((buf[0] != 0xFF) || (buf[1] != SOI))
        printf("Not a JPG file ?\n");
    fprintf(fp,"%c",buf[0]);
    fprintf(fp,"%c",buf[1]);


    //JPEG格式文件固定的文件头
    //begin指针前移2字节
    priv->stream_begin = buf+2;
    priv->stream_length = size-2;
    priv->stream_end = priv->stream_begin + priv->stream_length;
    //开始解析JFIF
    ret = Witeheaddata_parse_JFIF(priv, priv->stream_begin,fp);
    return ret;
}
/**
 * Load one jpeg image, and decompress it, and save the result.
 */
void write_head_data(const char *infilename,FILE*fpdat)
{
    FILE *fp;
    unsigned int length_of_file;
    unsigned char *buf;
    struct jdec_private *jdec;

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

    Witeheaddata_tinyjpeg_parse_header(jdec, buf, length_of_file,fpdat);

    free(buf);

}

int main(){
    //get the file fullpathname
    getFilelist();

    //get all the huffman table
    for(workon=0;workon<JPEGFileNum;workon++){

        printf("\n%s\n",Dir_Record->dir_huff[workon].FullPathName);
        convert_one_image(Dir_Record->dir_huff[workon].FullPathName);
    }
//    build_huff_val_useful_test();

    build_huff_val_useful();
    build_DC0_tree();
    build_DC1_tree();
    build_AC0_tree();
    build_AC1_tree();


    for(WriteNewData_workon=0;WriteNewData_workon<JPEGFileNum;WriteNewData_workon++){
        printf("\nWriteNewData %s\n",Dir_Record->dir_huff[WriteNewData_workon].FullPathName);
        WriteNewData_convert_one_image(Dir_Record->dir_huff[WriteNewData_workon].FullPathName);
    }

    return 0;
}



