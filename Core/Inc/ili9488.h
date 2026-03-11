/* Function declarations from ili9488.c */

void     ili9488_Init(void);
uint32_t ili9488_ReadID(void);
void     ili9488_DisplayOn(void);
void     ili9488_DisplayOff(void);
void     ili9488_SetCursor(uint16_t Xpos, uint16_t Ypos);
void     ili9488_WritePixel(uint16_t Xpos, uint16_t Ypos, uint16_t RGB_Code);
uint16_t ili9488_ReadPixel(uint16_t Xpos, uint16_t Ypos);
void     ili9488_SetDisplayWindow(uint16_t Xpos, uint16_t Ypos, uint16_t Width, uint16_t Height);
void     ili9488_DrawHLine(uint16_t RGBCode, uint16_t Xpos, uint16_t Ypos, uint16_t Length);
void     ili9488_DrawVLine(uint16_t RGBCode, uint16_t Xpos, uint16_t Ypos, uint16_t Length);
uint16_t ili9488_GetLcdPixelWidth(void);
uint16_t ili9488_GetLcdPixelHeight(void);
void     ili9488_DrawBitmap(uint16_t Xpos, uint16_t Ypos, uint8_t *pbmp);
void     ili9488_DrawRGBImage(uint16_t Xpos, uint16_t Ypos, uint16_t Xsize, uint16_t Ysize, uint16_t *pData);
void     ili9488_ReadRGBImage(uint16_t Xpos, uint16_t Ypos, uint16_t Xsize, uint16_t Ysize, uint16_t *pData);
void     ili9488_FillRect(uint16_t Xpos, uint16_t Ypos, uint16_t Xsize, uint16_t Ysize, uint16_t RGBCode);
void     ili9488_Scroll(int16_t Scroll, uint16_t TopFix, uint16_t BottonFix);
void     ili9488_UserCommand(uint16_t Command, uint8_t * pData, uint32_t Size, uint8_t Mode);

/* LCD interface type
   - 0: SPI half duplex (the mosi pin is bidirectional mode)
   - 1: SPI full duplex (write = mosi pin, read = miso pin)
   - 2: paralell 8 bit interface */
#define ILI9488_INTERFACE     1

/* Orientation:
   - 0: 320x480 micro-sd in the top (portrait)
   - 1: 480x320 micro-sd in the left (landscape)
   - 2: 320x480 micro-sd in the bottom (portrait)
   - 3: 480x320 micro-sd in the right (landscape) */
#define ILI9488_ORIENTATION   3

/* To clear the screen before display turning on ?
   - 0: does not clear
   - 1: clear */
#define ILI9488_INITCLEAR     1

/* Color order
   - 0: RGB
   - 1: BGR */
#define ILI9488_COLORMODE     0

/* Draw and read bitdeph (16: RGB565, 24: RGB888)
   note: my SPI ILI9488 LCD can only work in 24/24 bit depth
         my paralell 8 bit ILI9488 LCD can work in 16/16, 16/24, 24/16, 24/24 bit depth */
#define ILI9488_WRITEBITDEPTH 24
#define ILI9488_READBITDEPTH  24

/* ILI9488 Size (physical resolution in default orientation) */
#define  ILI9488_LCD_PIXEL_WIDTH   320
#define  ILI9488_LCD_PIXEL_HEIGHT  480
