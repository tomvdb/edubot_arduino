#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

//DfuSe file definitions !
#define Psignature "DfuSe"
#define Isignature "Target"
#define DFUversion 0x01
#define DFUtargets 0x01
#define DFUalternate 0x00
#define DFUtarget_named 0x01
#define VERSION "0.10a"

typedef struct Hexlines {
    int type;
    int adress;
    int lenght;
    int* data;
} Hexline;
typedef struct SImageElement {
    uint32_t address;
    uint8_t size;
    uint8_t data [32];

} TImageElement;
typedef struct SLargeElement {
    uint32_t address;
    uint32_t size;
    uint8_t *data;

} TLargeElement;





Hexline parse_hex_line(char *theline, int bytes[], int *addr, int *num,
        int *code);
void    create_tprefix(int size, int numOfElements);

void    create_dfuprefix(int size);

void    create_dfusuffix(void);

void    create_image(void);

uint32_t crc32_byte(uint32_t accum, uint8_t delta);

int numOfElements = 0;
int totalDataBytes=0;
int ActuallyDataBytesLoaded = 0;
int load_file(char *filename);
char tprefix[274];
char prefix[11];
char suffix[16];
TLargeElement *largeElements;
TImageElement *imageElements;


//-----------------------------------------
//-----------------------------------------
int main(int argc, const char *argv[]) {
    int i = 0;
    uint32_t crc32 = 0xffffffff;
    uint32_t temp;
    char crc_table[4];
    const char *filename;
    FILE *fDFU;


    printf("DFUse cmd file manager v%s \n",VERSION);
    if (argc < 2) {
        printf("too few arguments\n");
        return 0;
    }
    printf("Loading the file...\n");
    filename = argv[1];
    if (load_file((char*) filename)==0){
        printf("Error! Seems like it was not a hex file. Exiting...");
        return -1;
    }
    printf("Compiling %d small elements into a few big ones...\n",numOfElements);
    create_image();
    create_tprefix(ActuallyDataBytesLoaded, numOfElements);
    create_dfuprefix(11+274+ActuallyDataBytesLoaded+numOfElements*8);
    create_dfusuffix();

    filename = argv[2];
    fDFU = fopen(filename, "wb");
    if (fDFU == NULL) {
        printf("   Can't open  for writing.\n");
        return 0;
    }
    fwrite(prefix, 11, 1, fDFU);
    fwrite(tprefix, 274, 1, fDFU);

    for (i=0;i<numOfElements; i++){
        fwrite(&largeElements[i].address, 4, 1, fDFU);
        fwrite(&largeElements[i].size, 4, 1, fDFU);
        fwrite(largeElements[i].data, largeElements[i].size, 1, fDFU);
        printf("%d bytes are written from 0x%08x.\n",largeElements[i].size, largeElements[i].address );

    }

    /*  old way

    for (i=0;i<numOfElements;i++){
        fwrite(&(imageElement[i].address),4,1,fDFU);//&
        temp = (imageElement[i].size);
        printf("temp = %04x\n",temp);
        fwrite((&temp),4,1,fDFU);
        fwrite(imageElement[i].data,imageElement[i].size,1,fDFU);

        printf("write %08x %d %x20 \n",(imageElement[i].address), (imageElement[i].size),*(imageElement[i].data));
    }

*/


    fwrite(suffix, 12, 1, fDFU);
    fclose(fDFU);

    filename = argv[2];
    fDFU = fopen(filename, "r+b");
    /* compute crc */
    while (!feof(fDFU) && !ferror(fDFU))
    {
        fread(&temp, 1, 1, fDFU);
        if (!feof(fDFU)){
            crc32 = crc32_byte(crc32, temp);
        }
    }
    crc_table[0] = crc32 & 0xFF;
    crc_table[1] = (crc32 >> 8) & 0xFF;
    crc_table[2] = (crc32 >> 16) & 0xFF;
    crc_table[3] = (crc32 >> 24) & 0xFF;
    fwrite(crc_table, 4, 1, fDFU);
    fclose(fDFU);
    printf("\nDone! file crc is 0x%08x. Stored in last 4 bytes. \n", crc32);
    return 0;
}

Hexline parse_hex_line(theline, bytes, addr, num, code)
char *theline;
int *addr, *num, *code;
int bytes[];
{
    int sum, len, cksum;
    Hexline ret;
    char *ptr;

    *num = 0;
    if (theline[0] != ':')
    return ret;
    if (strlen(theline) < 11)
    return ret;

    ptr = theline+1;
    if (!sscanf(ptr, "%02x", &len))
    return ret;

    ptr += 2;
    if ( strlen(theline) < (11 + (len * 2)) )
    return ret;

    if (!sscanf(ptr, "%04x", addr))
    return ret;

    ptr += 4;
    if (!sscanf(ptr, "%02x", code))
    return ret;
    ptr += 2;
    sum = (len & 255) + ((*addr >> 8) & 255) + (*addr & 255) + (*code & 255);
   // printf("  \n DFU image content: %04x %04x ", *addr, len);

    while(*num != len)
    {
        if (!sscanf(ptr, "%02x", &bytes[*num])) return ret;
    //    printf("%02x",bytes[*num]);
        ptr += 2;
        sum += bytes[*num] & 255;
        (*num)++;
        if (*num >= 256) return ret;
    }

    if (!sscanf(ptr, "%02x", &cksum))
    return ret;
    if ( ((sum & 255) + (cksum & 255)) & 255 )
    return ret; /* checksum error */

    ret.type = *code;
    ret.adress = *addr;
    ret.lenght = len;
    ret.data = bytes;
    return ret;
}

int load_file(filename)
char *filename;
{
    char line[1000];
    FILE *fin;
    int addr, n,status, bytes[256];
    Hexline temp;
    int i,j=0;
    uint8_t maxLen = 0;
    int offset = 0;
    uint8_t *pData;
    uint8_t eofin;
    if (strlen(filename) == 0)
    {
        printf("   Can't load a file without the filename.");
        printf("  '?' for help\n");
        return 0;
    }

    fin = fopen(filename, "r");
    if (fin == NULL)
    {
        printf("   Can't open file '%s' for reading.\n", filename);
        return 0;
    }

    while (!feof(fin) && !ferror(fin))
    {
        line[0] = '\0';
        fgets(line, 1000, fin);
        if (line[strlen(line)-1] == '\n')
        line[strlen(line)-1] = '\0';
        if (line[strlen(line)-1] == '\r')
        line[strlen(line)-1] = '\0';
        temp = parse_hex_line(line, bytes, &addr, &n, &status);

        if(temp.type == 0)
        {
            totalDataBytes = totalDataBytes + temp.lenght;
            if (maxLen < temp.lenght) maxLen = temp.lenght;
        }

    }

    printf("%d Bytes in file\n",totalDataBytes);
    imageElements = (TImageElement*)malloc(totalDataBytes*10);

    rewind(fin);
    eofin = 0;
    while (!feof(fin) && !ferror(fin)&&!eofin)
    {
        line[0] = '\0';
        fgets(line, 1000, fin);
        if (line[strlen(line)-1] == '\n')
        line[strlen(line)-1] = '\0';
        if (line[strlen(line)-1] == '\r')
        line[strlen(line)-1] = '\0';
        temp = parse_hex_line(line, bytes, &addr, &n, &status);

        switch(temp.type)
        {
            case 0 :
                //printf("%s\n",line);
                imageElements[numOfElements].address = offset + temp.adress;
                pData = (uint8_t*)temp.data;
                for (i = 0;i<temp.lenght;i++)
                {
                    imageElements[numOfElements].data[i] = *pData;
                    pData+=4;

                }

                imageElements[numOfElements].size = temp.lenght;
                ActuallyDataBytesLoaded +=temp.lenght;
                ;

            for (i = 0; i<imageElements[numOfElements].size;i++)
               // printf("D%02x ",imageElements[numOfElements].data[i]);
           // printf("saved\n");

            numOfElements++;
            break;

            case 1 :  // End of hex file
                //printf("Found type code %d: end of file.\n",temp.type);
                eofin = 1;
            break;

            case 2 :  // Start of segment. Not implemented.
                printf("Error! Found type code %d. It not supported in this version. \n",temp.type);
                eofin = 1;
            break;

            case 4 :  // start of segment
                //printf(line);
                offset = 0;
                for (j=0;j<temp.lenght;j++)
                {
                //    printf("%02x",*(temp.data+j));
                    offset |= *(temp.data+j);
                    offset<<=8;
                }
                offset<<=8;

                printf("\nStart of segment detected. Code starts at 0x%08x\n", offset);
            break;

            case 5 :    // not used in ARM arch
                printf("Found type code %d. Ignored.\n",temp.type);
            break;
            case 3 :    // not used in ARM arch
                printf("Found type code %d. Ignored.\n",temp.type);
            break;

            default :
                printf("Error: bad type code : %d\n",temp.type);
            break;
        }

    }

    fclose(fin);
    printf("   Loaded %d bytes in %d DFU image elements.\n", ActuallyDataBytesLoaded, numOfElements);
    return ActuallyDataBytesLoaded;
}


TLargeElement    CreateLargeElement(uint32_t first, uint32_t count){
    uint32_t i, size=0;
    TLargeElement out;


    //size calculation
    for (i=first;i<(first+count);i++){ size = size + imageElements[i].size;}
    //printf("    size of large image element: %d bytes.\n", size );
    //printf("    from = %d.\n", first );
    //printf("    to = %d.\n", first + count -1);

    out.data = (uint8_t*)malloc(size);
    out.size = size;
    out.address = imageElements[first].address;
    for (i=first;i<(first+count);i++){
       memcpy( out.data, imageElements[i].data,  imageElements[i].size);
       out.data+=imageElements[i].size;
    }
    out.data-=size;
    return out;

}


void create_tprefix(int size, int numOfElements) {

    size = size + 8;
    //szSignature
    tprefix[0] = 'T';
    tprefix[1] = 'a';
    tprefix[2] = 'r';
    tprefix[3] = 'g';
    tprefix[4] = 'e';
    tprefix[5] = 't';

    //bAlternateSetting
    tprefix[6] = DFUalternate;

    //bTargetNamed
    tprefix[7] = DFUtarget_named;

    //szTargetName
    tprefix[11] = 'S';
    tprefix[12] = 'T';
    tprefix[13] = ' ';
    tprefix[14] = 'H';
    tprefix[15] = 'o';
    tprefix[16] = 'r';
    tprefix[17] = 'n';
    tprefix[18] = 'e';
    tprefix[19] = 't';
    tprefix[20] = ' ';
    tprefix[21] = 'B';
    tprefix[22] = 'o';
    tprefix[23] = 'a';
    tprefix[24] = 'r';
    tprefix[25] = 'd';
    tprefix[26] = ' ';
    tprefix[27] = 'D';
    tprefix[28] = 'F';
    tprefix[29] = 'U';
    tprefix[30] = ' ';
    tprefix[31] = 'F';
    tprefix[32] = 'i';
    tprefix[33] = 'l';
    tprefix[34] = 'e';
    tprefix[35] = ' ';

    //dwTargetSize
    tprefix[266] = size & 0xFF;
    tprefix[267] = (size >> 8) & 0xFF;
    tprefix[268] = (size >> 16) & 0xFF;
    tprefix[269] = (size >> 24) & 0xFF;

    //dwNbElements

    tprefix[270] = numOfElements & 0xFF;
    tprefix[271] = (numOfElements >> 8) & 0xFF;
    tprefix[272] = (numOfElements >> 16) & 0xFF;
    tprefix[273] = (numOfElements >> 24) & 0xFF;

}

void create_dfuprefix(int size) {
    int total = size;

    //szSignature
    prefix[0] = 'D';
    prefix[1] = 'f';
    prefix[2] = 'u';
    prefix[3] = 'S';
    prefix[4] = 'e';

    //bVeersion

    prefix[5] = DFUversion;

    //DFUImageSize
    prefix[6] = total & 0xFF;
    prefix[7] = (total >> 8) & 0xFF;
    prefix[8] = (total >> 16) & 0xFF;
    prefix[9] = (total >> 24) & 0xFF;

    //bTargets
    prefix[10] = DFUtargets;

}


void create_dfusuffix(void) {

    //bcdDevice
    suffix[0] = 0x0;
    suffix[1] = 0x0;
    //idProduct
    suffix[2] = 0x0;
    suffix[3] = 0x0;
    //idVendor
    suffix[4] = 0x0;
    suffix[5] = 0x0;
    //bcdDFU
    suffix[6] = 0x1A;
    suffix[7] = 0x01;
    //ucDfuSignature
    suffix[8] = 'U';
    suffix[9] = 'F';
    suffix[10] = 'D';
    //bLength
    suffix[11] = 0x10;

}


void    create_image(void){
    uint32_t i, largeElementsCount = 0,first=0,count=1;
     largeElements = (TLargeElement*)malloc(numOfElements*sizeof(TLargeElement));
    for (i=0;i<numOfElements-1;i++){
        if (((imageElements[i].address + imageElements[i].size)==(imageElements[i+1].address))){
            //printf("procissing %d-th image element\n", i);
            count++;
        }
        else {
            printf("Large Element %d creating...\n", largeElementsCount);
            largeElements[largeElementsCount] = CreateLargeElement(first, count);
            if (largeElements[largeElementsCount].data !=NULL) {
                largeElementsCount++;
                first = i+1;
                count = 1;
               // printf("  ...created.\n");
            }
            else printf("Error during creating Large element no %d\n",largeElementsCount);
        }
        if((i==numOfElements-2)){
            printf("The last large Element %d creating...\n", largeElementsCount);
            largeElements[largeElementsCount] = CreateLargeElement(first, count);
            if (largeElements[largeElementsCount].data !=NULL) {
                largeElementsCount++;
                printf("  ...created.\n");
            }
        }


    }
    numOfElements = largeElementsCount;

}



unsigned long crc32_table[] = { 0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
        0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb,
        0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e,
        0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75,
        0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808,
        0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162,
        0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49,
        0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc,
        0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3,
        0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
        0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d,
        0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
        0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767,
        0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a,
        0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
        0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14,
        0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b,
        0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee,
        0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
        0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
        0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d };

uint32_t crc32_byte(uint32_t accum, uint8_t delta) {
    return crc32_table[(accum ^ delta) & 0xff] ^ (accum >> 8);
}
