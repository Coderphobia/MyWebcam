#ifndef JPEG_ENC_H
#define JPEG_ENC_H

typedef unsigned int UINT32;
typedef int    INT32;
typedef unsigned short UINT16;
typedef short   INT16;
typedef unsigned char UINT8;
typedef char   INT8;

#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))
#define  BLOCK_SIZE 64

UINT32 encode_image (UINT8 * input_ptr, UINT8 * output_ptr,UINT32 quality_factor,UINT32 image_width, UINT32 image_height);


#endif // JPEG_ENC_H
