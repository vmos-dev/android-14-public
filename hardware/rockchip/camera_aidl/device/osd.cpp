#include "osd.h"

#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

#include <math.h>
#include <time.h>
#include <cwchar>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>

#include <png.h>
#include "pngstruct.h"
#include "pnginfo.h"


#include <inttypes.h>



#include "android-base/macros.h"
#include <utils/Timers.h>
#include <utils/Trace.h>
#include <linux/videodev2.h>
#include <sync/sync.h>

#define HAVE_JPEG // required for libyuv.h to export MJPEG decode APIs
#include <libyuv.h>

#include <jpeglib.h>
#include "RgaCropScale.h"
#include <RockchipRga.h>
#include <im2d_api/im2d.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>

#include <unordered_map>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>

#define MS_REFRESH_INTERVAL 1000
#define MS_PER_SEC 1000ULL
#define NS_PER_SEC 1000000000ULL

#define NS_PER_MS (NS_PER_SEC /MS_PER_SEC)

static inline long get_time_diff_ms(struct timespec *from,
struct timespec *to) {
return (to->tv_sec - from->tv_sec) * (long)MS_PER_SEC +
(to->tv_nsec - from->tv_nsec) / (long)NS_PER_MS;
}

namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_4 {
namespace implementation {
#define FONT_PIXEL 28
#define ALIGN(value, align) ((value + align -1) & ~(align-1))



int osd_time_pos_x,osd_time_pos_y;
int osd_logo_pos_x,osd_logo_pos_y;

int  OSD_IMAGE_W;
int  OSD_IMAGE_H;

std::mutex sLock;
bool sUseLogo = false;
std::unordered_map<int, FT_Library> sMapLibrary;
std::unordered_map<int, FT_Face> sMapFace;
std::unordered_map<int, FT_GlyphSlot> sMapSlot;
std::unordered_map<int, FT_Matrix> sMapMatrix;
std::unordered_map<int, FT_Vector> sMapPen;
std::unordered_map<int, uint32_t *> sMapPixelsLogo;
std::unordered_map<int, buffer_handle_t> sMapHandleLogo;
std::unordered_map<int, uint32_t *> sMapPixelsText;
std::unordered_map<int, buffer_handle_t> sMapHandleText;
std::unordered_map<int, uint32_t> sMapTextWidth;
std::unordered_map<int, uint32_t> sMapTextHeight;
std::unordered_map<int, struct timespec> sMapLastTime;
std::unordered_map<int, int> sMapTextLen;


void deInitFt(int index) {
  FT_Done_FreeType(sMapLibrary[index]);
}

void resetFt(int index) {
  double        angle;
  angle         = ( 0.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
  /* set up matrix */
  sMapMatrix[index].xx = (FT_Fixed)( cos( angle ) * 0x10000L );
  sMapMatrix[index].xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
  sMapMatrix[index].yx = (FT_Fixed)( sin( angle ) * 0x10000L );
  sMapMatrix[index].yy = (FT_Fixed)( cos( angle ) * 0x10000L );
  /* the pen position in 26.6 cartesian space coordinates; */
  /* start at (0,40) relative to the upper left corner  */
  sMapPen[index].x = 0;
  sMapPen[index].y = ( 5  ) * 64;
}

int initFt(int index) {

  FT_Error      error;

  double        angle;

  angle         = ( 0.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */

  error = FT_Init_FreeType(&sMapLibrary[index]);              /* initialize library */

  /* set up matrix */
  sMapMatrix[index].xx = (FT_Fixed)( cos( angle ) * 0x10000L );
  sMapMatrix[index].xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
  sMapMatrix[index].yx = (FT_Fixed)( sin( angle ) * 0x10000L );
  sMapMatrix[index].yy = (FT_Fixed)( cos( angle ) * 0x10000L );

  /* the pen position in 26.6 cartesian space coordinates; */
  /* start at (0,40) relative to the upper left corner  */
  sMapPen[index].x = 0;
  sMapPen[index].y = 0;
  return 0;
}


void draw_bitmap( FT_Bitmap*  bitmap, unsigned char *buf,  FT_Int   x, FT_Int  y, int w, int h)
{

  //ALOGD("%s(%d)FT_New_Face,x:%d,y:%d,w:%d,h:%d",__FUNCTION__,__LINE__,x,y,w,h);
  if(y < 0){
     y = 0;
  }
  if(x < 0){
     x = 0;
  }
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;
  //ALOGD("x_max:%d,y_max:%d",x_max,y_max);
  int pixByte = 4;
  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
        unsigned char alth_vale = bitmap->buffer[q * bitmap->width + p];
        double alth =(alth_vale*1.0)/255;
        buf[(j*w+i)*pixByte]   = 0xFF*alth; //R
        buf[(j*w+i)*pixByte+1] = 0xFF*alth; //G
        buf[(j*w+i)*pixByte+2] = 0xFF*alth; //B
        buf[(j*w+i)*pixByte+3] = alth_vale;
    }
  }
}
void wchar2RGBA(wchar_t  *text, unsigned char *rgba, int w, int h,int index) {
    //FT_ENCODING_GB2312, FT_ENCODING_UNICODE
    int len = wcslen(text);
    for (int i=0; i<len; i++) {
      if(text[i]>127){
        FT_New_Face(sMapLibrary[index],FONT_CN_FILE,0,&sMapFace[index]);
      }
      else{
        FT_New_Face(sMapLibrary[index],FONT_EN_FILE,0,&sMapFace[index]);
      }
      FT_Set_Pixel_Sizes(sMapFace[index], FONT_PIXEL, 0);

       sMapSlot[index] = sMapFace[index]->glyph;
       FT_Select_Charmap(sMapFace[index], FT_ENCODING_UNICODE);
       FT_Set_Transform(sMapFace[index], &sMapMatrix[index], &sMapPen[index]);
       int result = FT_Load_Glyph(sMapFace[index], FT_Get_Char_Index(sMapFace[index], text[i]), FT_LOAD_DEFAULT);

       //FT_RENDER_MODE_MONO FT_RENDER_MODE_NORMAL 第二个参数为渲染模式
        result = FT_Render_Glyph(sMapFace[index]->glyph,  FT_RENDER_MODE_NORMAL);
        FT_Bitmap bmp = sMapFace[index]->glyph->bitmap;

        //ALOGD("%s %d %c x:%d,y:%d,w:%d,h:%d",__FUNCTION__,__LINE__,text[i],mFace->glyph->bitmap_left,mFace->glyph->bitmap_top,w,h);
        draw_bitmap(&bmp, rgba,
                              sMapFace[index]->glyph->bitmap_left,
                              h - sMapFace[index]->glyph->bitmap_top, w, h);
        sMapPen[index].x += sMapSlot[index]->advance.x;
        sMapPen[index].y += sMapSlot[index]->advance.y;
        FT_Done_Face(sMapFace[index]);
    }
}
void getSize(wchar_t  *text, int *w,int *h,int index) {
    int len = wcslen(text);
    for (int i=0; i<len; i++) {
        if(text[i]>127){
            FT_New_Face(sMapLibrary[index],FONT_CN_FILE,0,&sMapFace[index]);
        }
        else{
            FT_New_Face(sMapLibrary[index],FONT_EN_FILE,0,&sMapFace[index]);
        }
       FT_Set_Pixel_Sizes(sMapFace[index], FONT_PIXEL, 0);
       sMapSlot[index] = sMapFace[index]->glyph;

       FT_Select_Charmap(sMapFace[index], FT_ENCODING_UNICODE);

       FT_Set_Transform(sMapFace[index], &sMapMatrix[index], &sMapPen[index]);

       int result = FT_Load_Glyph(sMapFace[index], FT_Get_Char_Index(sMapFace[index], text[i]), FT_LOAD_DEFAULT);

       //FT_RENDER_MODE_MONO FT_RENDER_MODE_NORMAL 第二个参数为渲染模式
        result = FT_Render_Glyph(sMapFace[index]->glyph,  FT_RENDER_MODE_NORMAL);
        FT_Bitmap bmp = sMapFace[index]->glyph->bitmap;

        //ALOGD("%s %d %c x:%d,y:%d,w:%d,h:%d",__FUNCTION__,__LINE__,text[i],mFace->glyph->bitmap_left,mFace->glyph->bitmap_top,*w,*h);
        if(sMapFace[index]->glyph->bitmap_left > *w){
          *w = sMapFace[index]->glyph->bitmap_left;
        }
        if(sMapFace[index]->glyph->bitmap_top > *h){
          *h = sMapFace[index]->glyph->bitmap_top;
        }
        sMapPen[index].x += sMapSlot[index]->advance.x;
        sMapPen[index].y += sMapSlot[index]->advance.y;
        FT_Done_Face(sMapFace[index]);
    }
    *w += FONT_PIXEL;
    //ALOGD("%s %dx%d",__FUNCTION__,*w,*h);
}


void getCSTTimeFormat(char* pStdTimeFormat)
{
	time_t nTimeStamp;
	time(&nTimeStamp);
	char pTmpString[256] = {0};
	tm *pTmStruct = localtime(&nTimeStamp);
	sprintf(pTmpString, "%04d-%02d-%02d %02d:%02d:%02d", pTmStruct->tm_year + 1900, pTmStruct->tm_mon + 1, pTmStruct->tm_mday, \
		pTmStruct->tm_hour, pTmStruct->tm_min, pTmStruct->tm_sec);
	strcpy(pStdTimeFormat, pTmpString);
}

void getCSTTimeFormatUnicode(wchar_t* pStdTimeFormat)
{
        time_t  nTimeStamp;
        time(&nTimeStamp);
        wchar_t pTmpString[256] = {0};
        tm *pTmStruct = localtime(&nTimeStamp);
        swprintf(pTmpString, 256, OSD_TEXT, pTmStruct->tm_year + 1900, pTmStruct->tm_mon + 1, pTmStruct->tm_mday, \
                pTmStruct->tm_hour, pTmStruct->tm_min, pTmStruct->tm_sec);
        wmemcpy(pStdTimeFormat, pTmpString, 128);
        return;
}

void getGMTTimeFormat(char* pStdTimeFormat)
{
	time_t ltime;
	time(&ltime);
	//ltime += 8*3600; //北京时区
	tm* gmt = gmtime(&ltime);
	char s[128] = { 0 };
	strftime(s, 80, "%Y-%m-%d %H:%M:%S", gmt);
	strcpy(pStdTimeFormat, s);
}

void timeFormat2Timestamp(const char* strTimeFormat, time_t& timeStamp)
{
  // strTimeFormat should be such as "2001-11-12 18:31:01"
  struct tm *timeinfo;
  memset( timeinfo, 0, sizeof(struct tm));
  strptime(strTimeFormat, "%Y-%m-%d %H:%M:%S",  timeinfo);
  timeStamp = mktime(timeinfo);
}

bool DecodePNG(char* png_path,unsigned int* buf) {
  FILE *fp;
  png_byte sig[8];
  fp = fopen(png_path, "rb");

  if (fp != NULL) {
    fread(sig, 1, 8, fp);
    if(png_sig_cmp(sig, 0, 8)){
      fclose(fp);
      return false;
    }
  } else {
      ALOGE("DecodePNG %s failed(%d, %s) file not exist",png_path,fp, strerror(errno));
      return false;
  }

  png_structp png_ptr;
  png_infop info_ptr;

  png_ptr = png_create_read_struct(png_get_libpng_ver(NULL), NULL, NULL, NULL);
  info_ptr = png_create_info_struct(png_ptr);
  setjmp(png_jmpbuf(png_ptr));
  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  ALOGD("PNG INFO(%s):\n pixel_depth = %d,bit_depth = %d, width = %d,height = %d", png_path,
  info_ptr->pixel_depth,info_ptr->bit_depth,
  info_ptr->width,info_ptr->height);

  png_bytep row_pointers[info_ptr->height];
  int row;
  for (row = 0; row < info_ptr->height; row++){
    row_pointers[row] = NULL;
    row_pointers[row] = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr,info_ptr));
  }
  png_read_image(png_ptr, row_pointers);
  int i, j;
  OSD_IMAGE_W = ALIGN(info_ptr->width,16);
  OSD_IMAGE_H = info_ptr->height;
  info_ptr->width = OSD_IMAGE_W;
  unsigned int *pDst = buf;//(unsigned int*)malloc(size);//因为sizeof(unsigned long)=8

  printf("sizeof(unsigned int) = %d\n", (int)sizeof(unsigned int));
  if(info_ptr->pixel_depth == 32){
      unsigned char* pSrc;
      unsigned int pixelR,pixelG,pixelB,pixelA;
      for(j=0; j<info_ptr->height; j++){
          pSrc = row_pointers[j];
          for(i=0; i<info_ptr->width; i++){
              pixelR = *pSrc++;
              pixelG = *pSrc++;
              pixelB = *pSrc++;
              pixelA = *pSrc++;
              pDst[i] = (pixelR<< 24) | (pixelR << 16) | (pixelB << 8) | pixelA;
              if (pixelA==0) {
                pDst[i] = 0;
              }
          }
          pDst += info_ptr->width;
      }
  }

  for (row = 0; row < info_ptr->height; row++){
    png_free(png_ptr,row_pointers[row]);
  }
  fclose(fp);
  png_free_data(png_ptr,info_ptr,PNG_FREE_ALL,-1);
  png_destroy_read_struct(&png_ptr,NULL,NULL);
  return true;
}

void processOSD(int width,int height,unsigned long dst_fd,int index){
  struct timespec last_tm;
  struct timespec curr_tm;
  clock_gettime(CLOCK_MONOTONIC_COARSE, &last_tm);
  clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);

  RockchipRga& rkRga(RockchipRga::get());
  std::lock_guard<std::mutex> lk(sLock);
  android::GraphicBufferAllocator& alloc(android::GraphicBufferAllocator::get());
  uint32_t *pixelsLogo = sMapPixelsLogo[index];
  buffer_handle_t memHandle = sMapHandleLogo[index];

  if(pixelsLogo == nullptr){
    const auto usage =
        GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_OFTEN;
    unsigned pixelsPerLine;
    android::status_t result =
            alloc.allocate(500, 500, HAL_PIXEL_FORMAT_RGBA_8888, 1, usage, &memHandle, &pixelsPerLine, 0,
                            "ExternalCameraDevice");
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    mapper.lock(memHandle,
                GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER,
                android::Rect(500, 500),
                (void **) &pixelsLogo);

    // If we failed to lock the pixel buffer, we're about to crash, but log it first
    if (!pixelsLogo) {
        ALOGE("Camera failed to gain access to image buffer for writing");
    }
    sUseLogo = DecodePNG(PNG_LOGO,pixelsLogo);
    sMapPixelsLogo[index] = pixelsLogo;
    sMapHandleLogo[index] = memHandle;
  }

  wchar_t text[128] = { 0 };
  getCSTTimeFormatUnicode(text);

  int textLen =  wcslen(text);

  int w,h;
  w = sMapTextWidth[index];
  h = sMapTextHeight[index];
  if (sMapTextLen[index]!=textLen)
  {
    initFt(index);
    getSize(text,&w,&h,index);
    deInitFt(index);
    sMapTextWidth[index] = w;
    sMapTextHeight[index] = h;
    sMapTextLen[index] = textLen;
  }

  uint32_t *pixelsFont = sMapPixelsText[index] ;
  int text_width =  ALIGN(w,16);
  int text_height =  FONT_PIXEL;
  //ALOGD("%s w:%d text_width:%d",__FUNCTION__,w,text_width);

  buffer_handle_t textHandle = sMapHandleText[index];
  if(pixelsFont == nullptr){
    const auto usage =
        GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_SW_READ_RARELY | GRALLOC_USAGE_SW_WRITE_OFTEN;
    unsigned pixelsPerLine;
    android::status_t result =
            alloc.allocate(text_width, text_height, HAL_PIXEL_FORMAT_RGBA_8888, 1, usage, &textHandle, &pixelsPerLine, 0,
                            "ExternalCameraDevice");
    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    mapper.lock(textHandle,
                GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_NEVER,
                android::Rect(text_width, text_height),
                (void **) &pixelsFont);

    // If we failed to lock the pixel buffer, we're about to crash, but log it first
    if (!pixelsFont) {
        ALOGE("Camera failed to gain access to image buffer for writing");
    }
    sMapPixelsText[index] = pixelsFont;
    sMapHandleText[index] = textHandle;
  }

  if (get_time_diff_ms(&sMapLastTime[index],&curr_tm) >= MS_REFRESH_INTERVAL)
  {
    initFt(index);
    resetFt(index);
    memset((unsigned char*)pixelsFont,0x00,text_width*text_height*4);
    wchar2RGBA(text, (unsigned char*)pixelsFont, text_width, text_height,index);
    deInitFt(index);
    clock_gettime(CLOCK_MONOTONIC_COARSE, &sMapLastTime[index]);
  }


	camera2::RgaCropScale::Params rgain, rgain0, rgaout;
	unsigned char* timeOsdVddr = NULL;
	unsigned char* labelOsdVddr = NULL;
	int timeOsdFd = -1;
	int labelOsdFd = -1;
	int timeOsdWidth = 0;
	int timeOsdHeight = 0;
	int labelOsdWidth = 0;
	int labelOsdHeight = 0;

  timeOsdVddr = (unsigned char*)pixelsFont;

  labelOsdVddr = (unsigned char*)pixelsLogo;
  timeOsdWidth = text_width;
  timeOsdHeight = text_height;
  labelOsdWidth = OSD_IMAGE_W;
  labelOsdHeight = OSD_IMAGE_H;

  osd_time_pos_x = width - timeOsdWidth;
  osd_time_pos_y = height - timeOsdHeight;
  osd_time_pos_y -= 16;

  //ALOGD("osd_time_pos_x:%d,osd_time_pos_y:%d",osd_time_pos_x,osd_time_pos_y);
  rkRga.RkRgaGetBufferFd(textHandle, &timeOsdFd);
  rgain.fd = timeOsdFd;
	rgain.fmt = HAL_PIXEL_FORMAT_RGBA_8888;
	rgain.vir_addr = nullptr;
	rgain.mirror = false;
	rgain.width = timeOsdWidth;
	rgain.height = timeOsdHeight;
	rgain.offset_x = 0;
	rgain.offset_y = 0;
	rgain.width_stride = timeOsdWidth;
	rgain.height_stride = timeOsdHeight;
	rgain.translate_x = osd_time_pos_x;
	rgain.translate_y = osd_time_pos_y;
	rgain.blend = 1;

	rgaout.fd = dst_fd;
	rgaout.fmt = HAL_PIXEL_FORMAT_YCrCb_NV12;
	rgaout.mirror = false;
	rgaout.width = width;
	rgaout.height =  height;
	rgaout.offset_x = 0;
	rgaout.offset_y = 0;
	rgaout.width_stride = width;
	rgaout.height_stride = height;
  camera2::RgaCropScale::Im2dBlit(&rgain, &rgaout);

  //ALOGD("labelOsdWidth:%d,labelOsdHeight:%d",labelOsdWidth,labelOsdHeight);
  if (sUseLogo)
  {
    osd_logo_pos_x = width - labelOsdWidth - 20;
    osd_logo_pos_y = height - labelOsdHeight - 30;
    osd_logo_pos_y -= 16;

    //ALOGD("osd_logo_pos_x:%d,osd_logo_pos_y:%d",osd_logo_pos_x,osd_logo_pos_y);
    rkRga.RkRgaGetBufferFd(memHandle, &labelOsdFd);
    rgain0.fd = labelOsdFd;
    rgain0.fmt = HAL_PIXEL_FORMAT_RGBA_8888;
    rgain0.vir_addr = nullptr;
    rgain0.mirror = false;
    rgain0.width = labelOsdWidth;
    rgain0.height = labelOsdHeight;
    rgain0.offset_x = 0;
    rgain0.offset_y = 0;
    rgain0.width_stride = labelOsdWidth;
    rgain0.height_stride = labelOsdHeight;
    rgain0.translate_x = osd_logo_pos_x;
    rgain0.translate_y = osd_logo_pos_y;
    rgain0.blend = 1;

    camera2::RgaCropScale::Im2dBlit(&rgain0, &rgaout);
  }
  clock_gettime(CLOCK_MONOTONIC_COARSE, &curr_tm);
  //ALOGD("%s(%d) use %ldms", __FUNCTION__,__LINE__,1(&last_tm,&curr_tm));
}


}  // namespace implementation
}  // namespace V3_4
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
