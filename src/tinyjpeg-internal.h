//
// Created by 梁嘉城 on 2020/1/18.
//

#ifndef SRC_TINYJPEG_INTERNAL_H
#define SRC_TINYJPEG_INTERNAL_H

#endif //SRC_TINYJPEG_INTERNAL_H
#define HUFFMAN_BITS_SIZE  256
#define HUFFMAN_HASH_NBITS 9
#define HUFFMAN_HASH_SIZE  (1UL<<HUFFMAN_HASH_NBITS)

#define HUFFMAN_TABLES	   4
#define COMPONENTS	   3

#define be16_to_cpu(x) (((x)[0]<<8)|(x)[1])


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

struct component
{
    unsigned int Hfactor;
    unsigned int Vfactor;
    float *Q_table;		/* Pointer to the quantisation table to use */
    struct huffman_table *AC_table;
    struct huffman_table *DC_table;
    short int previous_DC;	/* Previous DC coefficient */
    short int DCT[64];		/* DCT coef */
#if SANITY_CHECK
    unsigned int cid;
#endif
};

#define cY	0
#define cCb	1
#define cCr	2

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

    struct component component_infos[COMPONENTS];
    float Q_tables[COMPONENTS][64];		/* quantization tables */
    struct huffman_table HTDC[HUFFMAN_TABLES];	/* DC huffman tables   */
    struct huffman_table HTAC[HUFFMAN_TABLES];	/* AC huffman tables   */
    int default_huffman_table_initialized;
    int restart_interval;
    int restarts_to_go;				/* MCUs left in this restart interval */
    int last_rst_marker_seen;			/* Rst marker is incremented each time */

    /* Temp space used after the IDCT to store each components */
    uint8_t Y[64*4], Cr[64], Cb[64];

    jmp_buf jump_state;
    /* Internal Pointer use for colorspace conversion, do not modify it !!! */
    uint8_t *plane[COMPONENTS];

};

struct huffman_val{
    int DC0[256]={0};
    int DC1[256]={0};
    int AC0[256]={0};
    int AC1[256]={0};
}huff_val;


typedef struct
{
    unsigned int weight;
    unsigned int parent,lchild,rchild;
}HTNode,*HuffmanTree; // 动态分配数组存储赫夫曼树
typedef char **HuffmanCode; // 动态分配数组存储赫夫曼编码表

typedef struct {
    int val[256]={0};
    long int num[256]={0};
    HuffmanCode code[256];
    int count=0;
}one_hufftype;

typedef struct{
    one_hufftype DC0;
    one_hufftype DC1;
    one_hufftype AC0;
    one_hufftype AC1;
}HUFF_VAL_USEFUL;