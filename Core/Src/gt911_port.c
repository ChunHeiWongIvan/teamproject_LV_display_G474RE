#include "gt911_port.h"

/* I2C already initialized by MX_I2C1_Init() */
int32_t GT911_IO_Init(void)
{
    return 0;
}

int32_t GT911_IO_DeInit(void)
{
    return 0;
}

int32_t GT911_IO_GetTick(void)
{
    return (int32_t)HAL_GetTick();
}

/* GT911 uses 16-bit register addresses */
int32_t GT911_IO_ReadReg(uint16_t devAddr, uint16_t reg,
                         uint8_t *pData, uint16_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c2,
                         devAddr,
                         reg,
                         I2C_MEMADD_SIZE_16BIT,
                         pData,
                         len,
                         100) == HAL_OK)
    {
        return 0;
    }

    return -1;
}

int32_t GT911_IO_WriteReg(uint16_t devAddr, uint16_t reg,
                          uint8_t *pData, uint16_t len)
{
    if (HAL_I2C_Mem_Write(&hi2c2,
                          devAddr,
                          reg,
                          I2C_MEMADD_SIZE_16BIT,
                          pData,
                          len,
                          100) == HAL_OK)
    {
        return 0;
    }

    return -1;
}
