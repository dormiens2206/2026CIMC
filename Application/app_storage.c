#include "app_storage.h"
#include "app_param.h"
#include "app_sample.h"
#include "flash_param.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>

extern uint32_t g_sys_ms;

static FATFS g_fatfs;
static uint8_t g_tf_ready;
static uint8_t g_storage_running;
static TF_STORAGE_STATUS g_storage_last_error = TF_STORAGE_OK;
static uint32_t g_storage_write_count;

TF_STORAGE_STATUS TF_Storage_Init(void)
{
    DSTATUS disk_status;
    FRESULT result;
    uint8_t retry = 5U;

    sd_card_detect_init();

    g_tf_ready = 0U;

    if(!TF_Storage_IsInserted())
    {
        return TF_STORAGE_ERR_NO_CARD;
    }

    //初始化底层SDIO驱动
    do
    {
        disk_status = disk_initialize(0);
    } while((disk_status != 0U) && (--retry != 0U));

    if(disk_status != 0U) return TF_STORAGE_ERR_DISK_INIT;

    result = f_mount(0, &g_fatfs);

    if(result != FR_OK) return TF_STORAGE_ERR_MOUNT;
    g_tf_ready = 1U;
    return TF_STORAGE_OK;
}

uint8_t TF_Storage_IsReady(void)
{
    return g_tf_ready;
}

uint8_t TF_Storage_IsInserted(void)
{
    return sd_card_is_inserted();
}

TF_STORAGE_STATUS TF_Storage_WriteFile(const char *path, const uint8_t *data, uint32_t len)
{
    FIL file;
    FRESULT result;
    UINT written;

    if(!g_tf_ready) return TF_STORAGE_ERR_NOT_READY;
    if(path == 0 || data == 0) return TF_STORAGE_ERR_OPEN;
    result = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if(result != FR_OK) return TF_STORAGE_ERR_OPEN;
    result = f_write(&file, data, len, &written);
    f_close(&file);
    if(result != FR_OK || written < len) return TF_STORAGE_ERR_WRITE;
    return TF_STORAGE_OK;
}

TF_STORAGE_STATUS TF_Storage_AppendFile(const char *path, const uint8_t *data, uint32_t len)
{
    FIL file;
    FRESULT result;
    UINT written;

    if(!g_tf_ready) return TF_STORAGE_ERR_NOT_READY;
    if(path == 0 || data == 0) return TF_STORAGE_ERR_OPEN;
    result = f_open(&file, path, FA_OPEN_ALWAYS | FA_WRITE);
    if(result != FR_OK) return TF_STORAGE_ERR_OPEN;
    result = f_lseek(&file, f_size(&file));
    if(result != FR_OK)
    {
        f_close(&file);
        return TF_STORAGE_ERR_SEEK;
    }
    result = f_write(&file, data, len, &written);
    f_close(&file);
    if(result != FR_OK || written < len) return TF_STORAGE_ERR_WRITE;
    return TF_STORAGE_OK;
}

TF_STORAGE_STATUS TF_Storage_ReadFile(const char *path, uint8_t *buffer,
                                      uint32_t size, uint32_t *len)
{
    FIL file;
    FRESULT result;
    UINT read_len;

    if(!g_tf_ready) return TF_STORAGE_ERR_NOT_READY;
    if(path == 0 || buffer == 0 || len == 0) return TF_STORAGE_ERR_OPEN;
    result = f_open(&file, path, FA_READ);
    if(result != FR_OK) return TF_STORAGE_ERR_OPEN;
    result = f_read(&file, buffer, size, &read_len);
    f_close(&file);
    if(result != FR_OK) return TF_STORAGE_ERR_READ;
    *len = read_len;
    return TF_STORAGE_OK;
}

TF_STORAGE_STATUS TF_Storage_DeleteFile(const char *path)
{
    if(!g_tf_ready) return TF_STORAGE_ERR_NOT_READY;
    if(path == 0) return TF_STORAGE_ERR_OPEN;
    if(f_unlink(path) != FR_OK) return TF_STORAGE_ERR_DELETE;
    return TF_STORAGE_OK;
}

TF_STORAGE_STATUS TF_Storage_Test(void)
{
    const char *path = "test.txt";
    const char *text = "Hello, TF Card!";
    uint8_t buffer[64];
    uint32_t length;
    TF_STORAGE_STATUS status;

    status = TF_Storage_WriteFile(path, (const uint8_t *)text, strlen(text));
    if(status != TF_STORAGE_OK) return status;
    status = TF_Storage_ReadFile(path, buffer, sizeof(buffer), &length);
    if(status != TF_STORAGE_OK) return status;
    if(length != strlen(text) || memcmp(buffer, text, length) != 0)
    {
        return TF_STORAGE_ERR_READ;
    }
    return TF_Storage_DeleteFile(path);
}

void App_Storage_Start(void)
{
    TF_STORAGE_STATUS status;

    if(!TF_Storage_IsReady())
    {
        status = TF_Storage_Init();
        if(status != TF_STORAGE_OK)
        {
            g_storage_last_error = status;
            g_storage_running = 0U;
            return;
        }
    }
    g_storage_last_error = TF_STORAGE_OK;
    g_storage_running = 1U;
}

void App_Storage_Stop(void)
{
    g_storage_running = 0U;
}

void App_StorageTask(void)
{
    static uint32_t last_tick = 0U;
    static uint32_t last_retry_tick = 0U;

    /*
     * 0xFF：程序刚启动，还没有记录过状态
     * 0：上一次没有卡
     * 1：上一次有卡
     */
    static uint8_t last_inserted = 0xFFU;

    /*
     * 只有发生“拔出后重新插入”时才置1。
     * 正常开机不会走重新初始化逻辑。
     */
    static uint8_t reinsert_pending = 0U;

    uint8_t inserted;
    TF_STORAGE_STATUS status;

    const SystemParam_t *param = App_ParamGet();


    /*
     * 读取当前插卡状态。
     * 这个函数你已经验证过：
     * 插卡=1，拔卡=0，所以不要再改它。
     */
    inserted = TF_Storage_IsInserted();


    /*
     * 第一次进入任务：
     * 只记录当前状态，不做任何重新初始化。
     *
     * 因为正常开机时 App_Storage_Start()
     * 已经负责初始化TF卡了。
     */
    if(last_inserted == 0xFFU)
    {
        last_inserted = inserted;
    }
    else
    {
        /*
         * 只关注一种变化：
         *
         * 上一次没卡 0
         * 当前有卡   1
         *
         * 说明TF卡刚刚重新插入。
         */
        if((last_inserted == 0U) &&
           (inserted != 0U))
        {
            reinsert_pending = 1U;

            /*
             * 让下面可以立即进行第一次初始化尝试。
             */
            last_retry_tick = g_sys_ms - 1000U;
        }

        last_inserted = inserted;
    }


    /*
     * 当前没有卡：
     * 不写文件。
     *
     * 这里不要再碰原来的插拔检测逻辑，
     * 只等待下一次重新插入。
     */
		if(inserted == 0U)
		{
				/*
				 * TF卡已经拔出
				 *
				 * 如果之前已经成功挂载，
				 * 先取消FatFs挂载。
				 */
				if(g_tf_ready != 0U)
				{
						(void)f_mount(0, 0);
				}

				/*
				 * 清除TF卡已挂载状态
				 */
				g_tf_ready = 0U;

				/*
				 * 停止存储
				 */
				g_storage_running = 0U;

				/*
				 * 记录错误状态
				 */
				g_storage_last_error =
						TF_STORAGE_ERR_NO_CARD;

				return;
		}


    /*
     * =====================================================
     * 只有“拔卡以后重新插卡”才执行这里
     * =====================================================
     */
    if(reinsert_pending != 0U)
    {
        /*
         * 每1秒尝试重新初始化一次，
         * 避免初始化失败时疯狂循环。
         */
        if((uint32_t)(g_sys_ms - last_retry_tick) >= 1000U)
        {
            last_retry_tick = g_sys_ms;


            status = TF_Storage_Init();


            if(status == TF_STORAGE_OK)
            {
                /*
                 * SDIO + FatFs重新初始化成功
                 */
                g_storage_running = 1U;

                g_storage_last_error =
                    TF_STORAGE_OK;

                /*
                 * 重插处理结束
                 */
                reinsert_pending = 0U;
            }
            else
            {
                /*
                 * 本次没初始化成功，
                 * 保留pending，
                 * 下一秒继续重试。
                 */
                g_storage_running = 0U;

                g_storage_last_error = status;
            }
        }


        /*
         * 还没重新挂载成功时，
         * 不能继续执行文件存储。
         */
        if(reinsert_pending != 0U)
        {
            return;
        }
    }



    if(param->storage_enable == 0U)
    {
        return;
    }


    if(g_storage_running == 0U)
    {
        return;
    }


    if((uint32_t)(g_sys_ms - last_tick) >=
       param->storage_period_ms)
    {
        last_tick = g_sys_ms;


        status = App_Storage_SaveSample();


        if(status != TF_STORAGE_OK)
        {
            g_storage_last_error = status;
        }
        else
        {
            g_storage_last_error = TF_STORAGE_OK;
        }
    }
}

uint8_t App_Storage_IsRunning(void)
{
    return g_storage_running;
}

TF_STORAGE_STATUS App_Storage_GetLastError(void)
{
    return g_storage_last_error;
}

uint32_t App_Storage_GetWriteCount(void)
{
    return g_storage_write_count;
}

static uint8_t TF_Storage_FileExist(const char *path)
{
    FILINFO fno;
    if(f_stat(path, &fno) == FR_OK)
    {
        return 1U;
    }
    return 0U;
}

TF_STORAGE_STATUS App_Storage_SaveSample(void)
{
    char buf[128];


    if(g_storage_running == 0U)
    {
        return TF_STORAGE_ERR_NOT_READY;
    }


    if(!TF_Storage_IsReady())
    {
        return TF_STORAGE_ERR_NOT_READY;
    }

        // 如果文件不存在，先创建文件并写入表头
    if(!TF_Storage_FileExist("DATA.CSV"))
    {
        char head[] =
        "current_mA,voltage1_V,voltage2_V,current_break\r\n";


        if(TF_Storage_AppendFile("DATA.CSV",
                                 (uint8_t *)head,
                                 strlen(head)) != TF_STORAGE_OK)
        {
            return TF_STORAGE_ERR_WRITE;
        }
    }


    /*
     * 格式化采样数据
     */
    sprintf(buf,
            "%.3f,%.3f,%.3f,%d\r\n",
            g_sample_data.current_mA,
            g_sample_data.voltage1_V,
            g_sample_data.voltage2_V,
            g_sample_data.current_break);


    if(TF_Storage_AppendFile("DATA.CSV",
                             (uint8_t *)buf,
                             strlen(buf)) != TF_STORAGE_OK)
    {
        return TF_STORAGE_ERR_WRITE;
    }


    g_storage_write_count++;


    return TF_STORAGE_OK;
}

// 把用户定值保存为 Config.ini（KEY3 触发）
TF_STORAGE_STATUS App_Storage_SaveConfig(void)
{
    char buf[512];
    int len;

    if(!TF_Storage_IsReady())
    {
        return TF_STORAGE_ERR_NOT_READY;
    }

    // 生成配置文件内容
    len = snprintf(buf, sizeof(buf),
                   "[info]\r\n"
                   "dID=%u\r\n"
                   "[ratio]\r\n"
                   "CH0=%.2f\r\n"
                   "CH1=%.2f\r\n"
                   "CH2=%.2f\r\n"
                   "[limit]\r\n"
                   "CH0=%.2f\r\n"
                   "CH1=%.2f\r\n"
                   "CH2=%.2f\r\n",
                   (unsigned)g_system_params.device_id,
                   g_system_params.ch0_ratio,
                   g_system_params.ch1_ratio,
                   g_system_params.ch2_ratio,
                   g_system_params.ch0_threshold,
                   g_system_params.ch1_threshold,
                   g_system_params.ch2_threshold);

    if(len <= 0 || (uint32_t)len >= sizeof(buf))
    {
        return TF_STORAGE_ERR_WRITE;
    }

    return TF_Storage_WriteFile("Config.ini",
                                (uint8_t *)buf,
                                (uint32_t)len);
}

// 清除存储数据
TF_STORAGE_STATUS App_Storage_ClearData(void)
{
    return TF_Storage_DeleteFile("DATA.CSV");
}

// 读取存储数据
TF_STORAGE_STATUS App_Storage_ReadData(
    uint8_t *buf,
    uint32_t size,
    uint32_t *len)
{
    return TF_Storage_ReadFile("DATA.CSV",
                               buf,
                               size,
                               len);
}