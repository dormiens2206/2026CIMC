#ifndef APP_STORAGE_H
#define APP_STORAGE_H

#include <stdint.h>
#define APP_STORAGE_FILE "DATA.CSV"

typedef enum
{
    TF_STORAGE_OK = 0,
    TF_STORAGE_ERR_DISK_INIT,
    TF_STORAGE_ERR_MOUNT,
    TF_STORAGE_ERR_NO_CARD,

    TF_STORAGE_ERR_NOT_READY,
    TF_STORAGE_ERR_OPEN,
    TF_STORAGE_ERR_WRITE,
    TF_STORAGE_ERR_READ,
    TF_STORAGE_ERR_SEEK,
    TF_STORAGE_ERR_DELETE,
    TF_STORAGE_ERR_UNSUPPORTED
} TF_STORAGE_STATUS;

TF_STORAGE_STATUS TF_Storage_Init(void);

uint8_t TF_Storage_IsReady(void);
uint8_t TF_Storage_IsInserted(void);

TF_STORAGE_STATUS TF_Storage_WriteFile(const char *path, const uint8_t *data, uint32_t len);
TF_STORAGE_STATUS TF_Storage_AppendFile(const char *path, const uint8_t *data, uint32_t len);
TF_STORAGE_STATUS TF_Storage_ReadFile(const char *path, uint8_t *buffer, uint32_t size, uint32_t *len);
TF_STORAGE_STATUS TF_Storage_DeleteFile(const char *path);
TF_STORAGE_STATUS TF_Storage_Test(void);


void App_Storage_Start(void);
void App_Storage_Stop(void);
void App_StorageTask(void);
uint8_t App_Storage_IsRunning(void);
TF_STORAGE_STATUS App_Storage_GetLastError(void);
uint32_t App_Storage_GetWriteCount(void);
TF_STORAGE_STATUS App_Storage_SaveSample(void);
TF_STORAGE_STATUS App_Storage_SaveConfig(void);

TF_STORAGE_STATUS App_Storage_ClearData(void);
TF_STORAGE_STATUS App_Storage_ReadData(uint8_t *buf,
                                       uint32_t size,
                                       uint32_t *len);
#endif
