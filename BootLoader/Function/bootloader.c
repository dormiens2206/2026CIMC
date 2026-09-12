#include "HeaderFiles.h"
#include "bootloader.h"

#define FMC_PAGE_SIZE 0x1000U

static void Boot_Backup_Domain_Init(void);
static uint32_t Boot_Get_Flash_Sector(uint32_t addr);
static uint32_t Boot_Get_Next_Sector_Address(uint32_t addr);
static void Boot_DeInit_Before_Jump(void);


//初始化 Bootloader 需要的硬件
void Boot_HW_Init(void)
{
	//初始化系统滴答定时器
	systick_config();

	/* 使用工程已有的 LED 宏定义：PB12/PB13/PB14/PB15 */
	LED_Init();

	/* 备份寄存器用于保存升级请求标志 */
	Boot_Backup_Domain_Init();
}


//初始化备份域，用来读写升级标志。
static void Boot_Backup_Domain_Init(void)
{
	rcu_periph_clock_enable(RCU_PMU);       // 使能 PMU 外设时钟
	pmu_backup_write_enable();

	/* 只打开 RTC 外设时钟，不重新配置 RTC，避免影响 App 里的时间。 */
	rcu_periph_clock_enable(RCU_RTC);
}



//升级标志相关函数
uint32_t Boot_Get_Update_Flag(void)
{
	Boot_Backup_Domain_Init();
	return BOOT_UPDATE_FLAG_REG;
}


//设置或者清除升级标志，传入非0，写入0xA55A5AA5
void Boot_Set_Update_Flag(uint32_t flag)
{
	Boot_Backup_Domain_Init();

	if(flag != 0U) {
			BOOT_UPDATE_FLAG_REG = BOOT_UPDATE_FLAG_VALUE;
	} else {
			BOOT_UPDATE_FLAG_REG = 0U;
	}
}

//设置升级波特率
void Boot_Set_Update_Baudrate(uint32_t baudrate)
{
    Boot_Backup_Domain_Init();

    switch(baudrate)
    {
        case 4800U:
        case 9600U:
        case 19200U:
        case 115200U:
            BOOT_UPDATE_BAUD_REG = baudrate;
            break;

        default:
            BOOT_UPDATE_BAUD_REG = 19200U;
            break;
    }
}

uint32_t Boot_Get_Update_Baudrate(void)
{
    uint32_t baudrate;

    Boot_Backup_Domain_Init();

    baudrate = BOOT_UPDATE_BAUD_REG;

    switch(baudrate)
    {
        case 4800U:
        case 9600U:
        case 19200U:
        case 115200U:
            return baudrate;

        default:
            return 19200U;
    }
}

/* App 里以后可以调用这个函数，然后软件复位进入 Bootloader 升级模式 */
void Boot_Request_Update(void)
{
	Boot_Set_Update_Flag(BOOT_UPDATE_FLAG_VALUE);   // 设置升级标志
	NVIC_SystemReset();     // 软件复位，进入 Bootloader
}



//读取 App 向量表
uint32_t Boot_Get_App_Stack(uint32_t app_addr)
{
	return *(volatile uint32_t *)app_addr;      //读取 App 的主栈指针
}


//读取 App 入口地址
uint32_t Boot_Get_App_Reset_Handler(uint32_t app_addr)
{
	return *(volatile uint32_t *)(app_addr + 4U);       //读取 App 的 Reset_Handler 地址
}


//判断 App 是否合法
Boot_Status Boot_Check_App_Valid(uint32_t app_addr)
{
    uint32_t app_stack;
    uint32_t app_reset;
    uint32_t app_reset_aligned;

    app_stack = *(volatile uint32_t *)app_addr;     //读取 App 的主栈指针
    app_reset = *(volatile uint32_t *)(app_addr + 4U);      //读取 App 的 Reset_Handler 地址

    app_reset_aligned = app_reset & 0xFFFFFFFEU;        //去掉最低位，得到对齐的地址

    if((app_stack < SRAM_START_ADDR) || (app_stack > SRAM_END_ADDR))        //判断 App 的主栈指针是否在合法的 SRAM 范围内
    {
        return BOOT_ERROR;
    }

    /*
     * Cortex-M 函数入口最低位应为 1，表示 Thumb 状态。
     */
    if((app_reset & 0x1U) == 0U)        //判断 App 的 Reset_Handler 是否为 Thumb 指令集
    {
        return BOOT_ERROR;
    }

    if((app_reset_aligned < APPLICATION_FLASH_BASE) ||      //判断 App 的 Reset_Handler 是否在合法的 Flash 范围内
       (app_reset_aligned > APPLICATION_FLASH_END))
    {
        return BOOT_ERROR;
    }

    return BOOT_OK;
}

typedef void (*pFunction)(void);    //函数指针类型，指向无参无返回值的函数

//跳转到 App
void Boot_Jump_To_Application(uint32_t app_addr)
{
    uint32_t app_stack;     //读取 App 的主栈指针
    uint32_t app_reset;     //读取 App 的 Reset_Handler 地址
    pFunction app_entry;    //函数指针，指向 App 的入口函数
    uint8_t i;

    app_stack = *(volatile uint32_t *)app_addr;
    app_reset = *(volatile uint32_t *)(app_addr + 4U);

    app_entry = (pFunction)app_reset;

    // 关闭所有中断
    __disable_irq();

    /*
     * 关 SysTick，避免跳到 APP 后还进 Bootloader 的 SysTick 中断。
     */
    SysTick->CTRL = 0U; 
    SysTick->LOAD = 0U; 
    SysTick->VAL  = 0U;

    /*
     * 关闭并清除所有 NVIC 中断。
     * GD32F470 中断比较多，8 组基本够用。
     */
    for(i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;      //关闭中断  
        NVIC->ICPR[i] = 0xFFFFFFFFU;        //清除挂起标志
    }

    /*
     * 设置 APP 的中断向量表地址。
     */
    SCB->VTOR = app_addr;

    /*
     * 设置 APP 的主栈指针。
     */
    __set_MSP(app_stack);

    __enable_irq();

    /*
     * 跳到 APP 的 Reset_Handler。
     */
    app_entry();
}


//跳转前清理
static void Boot_DeInit_Before_Jump(void)
{
	uint32_t i;

	
	//关总中断
	__disable_irq();

	/* 关闭 SysTick，避免 Bootloader 的节拍中断带到 App 里。 */
	SysTick->CTRL = 0U;
	SysTick->LOAD = 0U;
	SysTick->VAL  = 0U;

	/* 关闭并清掉所有 NVIC 中断。GD32F4 Cortex-M4 这里 8 组足够覆盖。 */
	for(i = 0U; i < 8U; i++) {
			NVIC->ICER[i] = 0xFFFFFFFFU;
			NVIC->ICPR[i] = 0xFFFFFFFFU;
	}

    //设置 App 的中断向量表地址
	__DSB();
	__ISB();
}

/* 地址到 GD32F4 Flash Sector 的映射。512KB 型号够用。 */
//Flash 不是随便一个字节一个字节擦的，而是按 Sector 擦除。GD32F4 的 Flash 分区如下：
static uint32_t Boot_Get_Flash_Sector(uint32_t addr)
{
	if(addr < 0x08004000U) return CTL_SECTOR_NUMBER_0;  /* 16KB */
	if(addr < 0x08008000U) return CTL_SECTOR_NUMBER_1;  /* 16KB */
	if(addr < 0x0800C000U) return CTL_SECTOR_NUMBER_2;  /* 16KB */
	if(addr < 0x08010000U) return CTL_SECTOR_NUMBER_3;  /* 16KB */
	if(addr < 0x08020000U) return CTL_SECTOR_NUMBER_4;  /* 64KB */
	if(addr < 0x08040000U) return CTL_SECTOR_NUMBER_5;  /* 128KB */
	if(addr < 0x08060000U) return CTL_SECTOR_NUMBER_6;  /* 128KB */
	if(addr < 0x08080000U) return CTL_SECTOR_NUMBER_7;  /* 128KB */

	return 0xFFFFFFFFU;
}

//获取下一个扇区的起始地址
static uint32_t Boot_Get_Next_Sector_Address(uint32_t addr)
{
	if(addr < 0x08004000U) return 0x08004000U;
	if(addr < 0x08008000U) return 0x08008000U;
	if(addr < 0x0800C000U) return 0x0800C000U;
	if(addr < 0x08010000U) return 0x08010000U;
	if(addr < 0x08020000U) return 0x08020000U;
	if(addr < 0x08040000U) return 0x08040000U;
	if(addr < 0x08060000U) return 0x08060000U;
	if(addr < 0x08080000U) return 0x08080000U;

	return MCU_FLASH_END;
}


//擦除 App 区域，串口升级会用
Boot_Status Boot_Erase_App_Area(void)
{
	uint32_t addr;
	uint32_t sector;
	fmc_state_enum state;   //Flash 控制器状态枚举类型

	fmc_unlock();   //解锁 Flash 控制器，允许擦写操作

    //清除所有可能的 Flash 错误标志
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
								 FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);

    //从 App 区起始地址开始，按扇区擦除，直到 App 区结束地址
	addr = APPLICATION_FLASH_BASE;

    //擦除 App 区域的每个扇区
	while (addr <= APPLICATION_FLASH_END)
	{
            //获取当前地址所在的扇区号
			state = fmc_page_erase(addr);
			if (state != FMC_READY)     //如果擦除失败，返回 BOOT_ERROR
			{
					fmc_lock();     //锁定 Flash 控制器，禁止擦写操作
					return BOOT_ERROR;
			}

            //获取下一个扇区的起始地址
			addr += FMC_PAGE_SIZE;
	}

	fmc_lock();
	return BOOT_OK;
}

//写入 App 区域
Boot_Status Boot_Write_App(uint32_t offset, const uint8_t *data, uint32_t len)
{
	uint32_t addr;
	uint32_t i;
	uint32_t word;  //按 4 字节对齐写入 Flash 时的临时变量
	uint32_t copy_len;
	fmc_state_enum state;

	if((data == 0) || (len == 0U)) {
			return BOOT_ERROR;
	}

	if(offset >= APPLICATION_FLASH_SIZE) {
			return BOOT_ERROR;
	}

	if(len > (APPLICATION_FLASH_SIZE - offset)) {
			return BOOT_ERROR;
	}

    //计算写入的起始地址
	addr = APPLICATION_FLASH_BASE + offset;

	fmc_unlock();   //解锁 Flash 控制器，允许擦写操作

    //清除所有可能的 Flash 错误标志
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
								 FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);

    //按 4 字节对齐写入数据到 Flash，Flash 写入是按字（word）为单位的
	for(i = 0U; i < len; i += 4U) {
			word = 0xFFFFFFFFU;
			copy_len = ((len - i) >= 4U) ? 4U : (len - i);
			memcpy(&word, &data[i], copy_len);

			state = fmc_word_program(addr + i, word);   //写入一个字（4 字节）到 Flash
			if(state != FMC_READY) {
					fmc_lock();
					return BOOT_ERROR;
			}

			// 检查写入是否成功
			if(*(volatile uint32_t *)(addr + i) != word) {
					fmc_lock();
					return BOOT_ERROR;
			}
	}

	fmc_lock();
	return BOOT_OK;
}


//擦除指定 Flash 区域，返回 BOOT_OK 或 BOOT_ERROR
static Boot_Status Boot_Erase_Area(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t addr;
    fmc_state_enum state;

    if(start_addr > end_addr)
    {
        return BOOT_ERROR;
    }

    fmc_unlock();

    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);

    addr = start_addr;

    while(addr <= end_addr)
    {
        state = fmc_page_erase(addr);

        if(state != FMC_READY)
        {
            fmc_lock();
            return BOOT_ERROR;
        }

        addr += FMC_PAGE_SIZE;
    }

    fmc_lock();
    return BOOT_OK;
}

//写入指定 Flash 区域，返回 BOOT_OK 或 BOOT_ERROR
static Boot_Status Boot_Write_InternalFlash(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t word;
    uint32_t copy_len;
    fmc_state_enum state;
    if((data == 0) || (len == 0U))
    {
        return BOOT_ERROR;
    }

    fmc_unlock();

    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
                   FMC_FLAG_PGMERR | FMC_FLAG_PGSERR | FMC_FLAG_RDDERR);

    for(i = 0U; i < len; i += 4U)
    {
        word = 0xFFFFFFFFU;
        copy_len = ((len - i) >= 4U) ? 4U : (len - i);

        memcpy(&word, &data[i], copy_len);

        state = fmc_word_program(addr + i, word);

        if(state != FMC_READY)
        {
            fmc_lock();
            return BOOT_ERROR;
        }

        if(*(volatile uint32_t *)(addr + i) != word)
        {
            fmc_lock();
            return BOOT_ERROR;
        }
    }

    fmc_lock();
    return BOOT_OK;
}

//擦除暂存区，返回 BOOT_OK 或 BOOT_ERROR
Boot_Status Boot_Erase_Temp_Area(void)
{
    return Boot_Erase_Area(FW_TEMP_START_ADDR, FW_TEMP_END_ADDR);
}

//写入暂存区，返回 BOOT_OK 或 BOOT_ERROR
Boot_Status Boot_Write_Temp(uint32_t offset, const uint8_t *data, uint32_t len)
{
    if((data == 0) || (len == 0U))
    {
        return BOOT_ERROR;
    }

    if(offset >= FW_TEMP_SIZE)
    {
        return BOOT_ERROR;
    }

    if(len > (FW_TEMP_SIZE - offset))
    {
        return BOOT_ERROR;
    }

    return Boot_Write_InternalFlash(FW_TEMP_START_ADDR + offset, data, len);
}

//检查暂存区的固件是否合法
Boot_Status Boot_Check_Temp_Firmware(uint32_t fw_size)
{
    uint8_t *p;
    uint32_t magic;
    uint32_t app_stack;
    uint32_t app_reset;
    uint32_t app_reset_aligned;

    /*
     * 官方升级包前 4 字节是魔术字 5AA5C33C。
     * 后面才是真正要搬到 APP 区的向量表和程序。
     */
    if(fw_size <= (BOOT_FW_MAGIC_SIZE + 8U))
    {
        return BOOT_ERROR;
    }

    if(fw_size > FW_TEMP_SIZE)
    {
        return BOOT_ERROR;
    }

    p = (uint8_t *)FW_TEMP_START_ADDR;

    magic = ((uint32_t)p[0] << 24) |
            ((uint32_t)p[1] << 16) |
            ((uint32_t)p[2] << 8)  |
            ((uint32_t)p[3]);

    if(magic != BOOT_FW_MAGIC)
    {
        return BOOT_ERROR;
    }

    /*
     * 因为前 4 字节是 magic，所以 APP 的 MSP 在 +4，
     * Reset_Handler 在 +8。
     */
    app_stack = *(volatile uint32_t *)(FW_TEMP_START_ADDR + BOOT_FW_MAGIC_SIZE);
    app_reset = *(volatile uint32_t *)(FW_TEMP_START_ADDR + BOOT_FW_MAGIC_SIZE + 4U);
    app_reset_aligned = app_reset & 0xFFFFFFFEU;

    if((app_stack < SRAM_START_ADDR) || (app_stack > SRAM_END_ADDR))
    {
        return BOOT_ERROR;
    }

    if((app_reset & 0x1U) == 0U)
    {
        return BOOT_ERROR;
    }

    if((app_reset_aligned < APPLICATION_FLASH_BASE) || (app_reset_aligned > APPLICATION_FLASH_END))
    {
        return BOOT_ERROR;
    }

    return BOOT_OK;
}

//把暂存区的固件搬到 APP 区
Boot_Status Boot_Copy_Temp_To_App(uint32_t fw_size)
{
    static uint8_t copy_buf[512];

    // 暂存区的源地址
    uint32_t src_addr;
    // 剩余要搬的字节数
    uint32_t remain;
    // 要搬的字节数
    uint32_t copy_len;
    // 目标地址偏移
    uint32_t app_offset;

    // 检查暂存区的固件是否合法
    if(Boot_Check_Temp_Firmware(fw_size) != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    // 备份当前的 App 区固件，以便回滚
    if(Boot_Backup_App() != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    /*
     * 注意：不能把 magic 写进 APP 区。
     * APP 区第一个 word 必须是 MSP，第二个 word 必须是 Reset_Handler。
     */
    src_addr = FW_TEMP_START_ADDR + BOOT_FW_MAGIC_SIZE;
    remain = fw_size - BOOT_FW_MAGIC_SIZE;
    app_offset = 0U;

    if(remain > APPLICATION_FLASH_SIZE)
    {
        return BOOT_ERROR;
    }

    // 擦除 App 区域
    if(Boot_Erase_App_Area() != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    while(remain > 0U)
    {
        copy_len = (remain > sizeof(copy_buf)) ? sizeof(copy_buf) : remain;

        memcpy(copy_buf, (uint8_t *)src_addr, copy_len);

        // 写入 App 区域
        if(Boot_Write_App(app_offset, copy_buf, copy_len) != BOOT_OK)
        {
            return BOOT_ERROR;
        }

        src_addr += copy_len;
        app_offset += copy_len;
        remain -= copy_len;
    }

    // 检查 App 区是否合法
    if(Boot_Check_App_Valid(APPLICATION_FLASH_BASE) != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    return BOOT_OK;
}


//备份升级前的一个版本
Boot_Status Boot_Backup_App(void)
{
    static uint8_t copy_buf[512];

    uint32_t src_addr;
    uint32_t remain;
    uint32_t copy_len;
    uint32_t backup_addr;

    src_addr = APP_START_ADDR;
    remain = APP_SIZE;
    backup_addr = APP_BACKUP_START_ADDR;

    // 擦除备份区
    if(Boot_Erase_Area(APP_BACKUP_START_ADDR, APP_BACKUP_END_ADDR) != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    while(remain > 0U)
    {
        copy_len = (remain > sizeof(copy_buf)) ? sizeof(copy_buf) : remain;

        memcpy(copy_buf, (uint8_t *)src_addr, copy_len);

        // 写入备份区
        if(Boot_Write_Area(backup_addr, copy_buf, copy_len) != BOOT_OK)
        {
            return BOOT_ERROR;
        }

        src_addr += copy_len;
        backup_addr += copy_len;
        remain -= copy_len;

    }

    return BOOT_OK;
}

//回退到备份版本
Boot_Status Boot_Rollback(void)
{
    static uint8_t copy_buf[512];

    uint32_t src_addr;
    uint32_t remain;
    uint32_t copy_len;
    uint32_t app_offset;

    src_addr = APP_BACKUP_START_ADDR;
    remain = APP_SIZE;
    app_offset = 0U;

    // 检查备份区是否有效
    if(Boot_Check_App_Valid(APP_BACKUP_START_ADDR) != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    if(Boot_Erase_App_Area() != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    while(remain > 0U)
    {
        copy_len = (remain > sizeof(copy_buf) ? sizeof(copy_buf) : remain);
        memcpy(copy_buf, (uint8_t *)src_addr, copy_len);
        // 写入 App 区域
        if(Boot_Write_App(app_offset, copy_buf, copy_len) != BOOT_OK)
        {
            return BOOT_ERROR;
        }
        src_addr += copy_len;
        app_offset += copy_len;
        remain -= copy_len;
    }
    return Boot_Check_App_Valid(APP_START_ADDR);
}

//读备份区固件版本号
Boot_Status Boot_Get_Backup_Version(uint8_t *ver_out)
{
    const uint8_t *p;

    // 读取备份区的版本号，前提是备份区合法
    if(ver_out == 0)
    {
        return BOOT_ERROR;
    }

    // 检查备份区是否有效
    if(Boot_Check_App_Valid(APP_BACKUP_START_ADDR) != BOOT_OK)
    {
        return BOOT_ERROR;
    }

    // 备份区的版本号存放在 APP_BACKUP_START_ADDR + APP_VERSION_OFFSET
    p = (const uint8_t *)(APP_BACKUP_START_ADDR + APP_VERSION_OFFSET);

    ver_out[0] = p[0];
    ver_out[1] = p[1];
    ver_out[2] = p[2];
    ver_out[3] = p[3];

    return BOOT_OK;
}