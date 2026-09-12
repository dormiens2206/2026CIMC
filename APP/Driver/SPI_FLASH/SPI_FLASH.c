#include "SPI_FLASH.h"

#define WRITE            0x02       //写数据指令
#define WRSR             0x01       //写状态寄存器指令
#define WREN             0x06       //写使能指令

#define READ             0x03       //读数据指令
#define RDSR             0x05       //读状态寄存器指令
#define RDID             0x9F       //读器件ID指令
#define SE               0x20       //扇区擦除指令，擦除大小为4KB
#define BE               0xC7       //块擦除指令，擦除大小为64KB

#define WIP_FLAG         0x01       //写入进行标志位
#define DUMMY_BYTE       0xA5       //SPI Flash时钟空闲时输出的默认值，实际应用中可以根据需要调整


//初始化flash
void spi_flash_init(void)
{
    spi_parameter_struct spi_init_struct;

    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_SPI0);
    rcu_periph_clock_enable(RCU_GPIOA);


    /* SPI0_CLK(PB3), SPI0_MISO(PB4), SPI0_MOSI(PB5) */
    gpio_af_set(GPIOB, GPIO_AF_5, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_25MHZ, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5);

    /* SPI0_CS(PA15) GPIO pin configuration */
    gpio_mode_set(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_15);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_15);


    /* chip select invalid*/
    SPI_FLASH_CS_HIGH();

    /* SPI0 parameter config */
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_8;
    spi_init_struct.endian = SPI_ENDIAN_MSB;;
    spi_init(SPI0, &spi_init_struct);

    /* enable SPI0 */
    spi_enable(SPI0);
}

//扇区擦除
void spi_flash_sector_erase(uint32_t sector_addr)
{
    /* send write enable instruction */
    spi_flash_write_enable();

    /* sector erase */
    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();
    /* send sector erase instruction */
    spi_flash_send_byte(SE);
    /* send sector_addr high nibble address byte */
    spi_flash_send_byte((sector_addr & 0xFF0000) >> 16);
    /* send sector_addr medium nibble address byte */
    spi_flash_send_byte((sector_addr & 0xFF00) >> 8);
    /* send sector_addr low nibble address byte */
    spi_flash_send_byte(sector_addr & 0xFF);
    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();

    /* wait the end of flash writing */
    spi_flash_wait_for_write_end();
}

void spi_flash_buffer_erase(uint32_t sector_addr, uint32_t num_byte_to_erase)
{
    uint8_t buffer_data[SPI_FLASH_SECTOR_SIZE] = { 0 };
    uint8_t buffer_data1[SPI_FLASH_SECTOR_SIZE] = { 0 };
    uint8_t num_of_sector = 0, num_of_single = 0, addr = 0, count = 0;
    // uint8_t temp = 0;

    addr = sector_addr % SPI_FLASH_SECTOR_SIZE;
    count = SPI_FLASH_PAGE_SIZE - addr;	
    num_of_sector = num_byte_to_erase / SPI_FLASH_SECTOR_SIZE;
    num_of_single = num_byte_to_erase % SPI_FLASH_SECTOR_SIZE;


    if (0 == addr)
    {

        while (num_of_sector--)	
        {
            spi_flash_sector_erase(sector_addr);
            sector_addr += SPI_FLASH_PAGE_SIZE;
        }
        if (0 != num_of_single)	
        {
            spi_flash_buffer_read(buffer_data, sector_addr + num_of_single, SPI_FLASH_SECTOR_SIZE - num_of_single);	
            spi_flash_sector_erase(sector_addr);
            spi_flash_buffer_write(buffer_data, sector_addr + num_of_single, SPI_FLASH_SECTOR_SIZE - num_of_single);
        }
    }
    else
    {
        if (num_byte_to_erase < count)
        {
            spi_flash_buffer_read(buffer_data, num_of_sector * SPI_FLASH_SECTOR_SIZE, addr);
            spi_flash_buffer_read(buffer_data1, num_of_sector * SPI_FLASH_SECTOR_SIZE + addr + num_byte_to_erase, SPI_FLASH_SECTOR_SIZE - (addr)-num_byte_to_erase);
            spi_flash_sector_erase(sector_addr);		
            spi_flash_buffer_write(buffer_data, num_of_sector * SPI_FLASH_SECTOR_SIZE, addr);
            spi_flash_buffer_write(buffer_data1, num_of_sector * SPI_FLASH_SECTOR_SIZE + addr + num_byte_to_erase, SPI_FLASH_SECTOR_SIZE - (addr)-num_byte_to_erase);
        }
        else
        {
            spi_flash_buffer_read(buffer_data, num_of_sector * SPI_FLASH_SECTOR_SIZE, addr);
            spi_flash_buffer_write(buffer_data, num_of_sector * SPI_FLASH_SECTOR_SIZE, addr);

        
            num_byte_to_erase -= addr;
            num_of_sector = num_byte_to_erase / SPI_FLASH_SECTOR_SIZE;
            num_of_single = num_byte_to_erase % SPI_FLASH_SECTOR_SIZE;
            sector_addr += count;

            while (num_of_sector--)	
            {
                spi_flash_sector_erase(sector_addr);
                sector_addr += SPI_FLASH_PAGE_SIZE;
            }
            if (0 != num_of_single)
            {
                spi_flash_buffer_read(buffer_data, sector_addr + num_of_single, SPI_FLASH_SECTOR_SIZE - num_of_single);	
                spi_flash_sector_erase(sector_addr);
                spi_flash_buffer_write(buffer_data, sector_addr + num_of_single, SPI_FLASH_SECTOR_SIZE - num_of_single);
            }
        }
    }
}



//整片擦除
void spi_flash_bulk_erase(void)
{
    /* send write enable instruction */
    spi_flash_write_enable();

    /* bulk erase */
    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();
    /* send bulk erase instruction  */
    spi_flash_send_byte(BE);
    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();

    /* wait the end of flash writing */
    spi_flash_wait_for_write_end();
}

//写一页数据，页大小为256字节
void spi_flash_page_write(uint8_t* pbuffer, uint32_t write_addr, uint16_t num_byte_to_write)
{
    /* enable the write access to the flash */
    spi_flash_write_enable();

    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();

    /* send "write to memory" instruction */
    spi_flash_send_byte(WRITE);
    /* send write_addr high nibble address byte to write to */
    spi_flash_send_byte((write_addr & 0xFF0000) >> 16);
    /* send write_addr medium nibble address byte to write to */
    spi_flash_send_byte((write_addr & 0xFF00) >> 8);
    /* send write_addr low nibble address byte to write to */
    spi_flash_send_byte(write_addr & 0xFF);

    /* while there is data to be written on the flash */
    while (num_byte_to_write--) {
        /* send the current byte */
        spi_flash_send_byte(*pbuffer);
        /* point on the next byte to be written */
        pbuffer++;
    }

    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();

    /* wait the end of flash writing */
    spi_flash_wait_for_write_end();
}

//写一段数据，长度不固定
void spi_flash_buffer_write(uint8_t* pbuffer, uint32_t write_addr, uint32_t num_byte_to_write)
{

    uint8_t num_of_page = 0, num_of_single = 0, addr = 0, count = 0;
    // uint8_t temp = 0;

    addr = write_addr % SPI_FLASH_PAGE_SIZE;
    count = SPI_FLASH_PAGE_SIZE - addr;
    num_of_page = num_byte_to_write / SPI_FLASH_PAGE_SIZE;
    num_of_single = num_byte_to_write % SPI_FLASH_PAGE_SIZE;


    if (0 == addr)
    {

        while (num_of_page--)
        {
            spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
            write_addr += SPI_FLASH_PAGE_SIZE;
            pbuffer += SPI_FLASH_PAGE_SIZE;
        }
        if (0 != num_of_single)
            spi_flash_page_write(pbuffer, write_addr, num_of_single);
    }
    else
    {
        if (num_byte_to_write < count)
        {
            spi_flash_page_write(pbuffer, write_addr, num_byte_to_write);
        }
        else
        {

            spi_flash_page_write(pbuffer, write_addr, count);


            num_of_page = (num_byte_to_write - count) / SPI_FLASH_PAGE_SIZE;
            num_of_single = (num_byte_to_write - count) % SPI_FLASH_PAGE_SIZE;
            write_addr += count;
            pbuffer += count;
            while (num_of_page--)
            {
                spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
                write_addr += SPI_FLASH_PAGE_SIZE;
                pbuffer += SPI_FLASH_PAGE_SIZE;
            }

            if (0 != num_of_single)
                spi_flash_page_write(pbuffer, write_addr, num_of_single);
        }
    }
}

//读一段数据，长度不固定
void spi_flash_buffer_read(uint8_t* pbuffer, uint32_t read_addr, uint16_t num_byte_to_read)
{
    /* select the flash: chip slect low */
    SPI_FLASH_CS_LOW();

    /* send "read from memory " instruction */
    spi_flash_send_byte(READ);

    /* send read_addr high nibble address byte to read from */
    spi_flash_send_byte((read_addr & 0xFF0000) >> 16);
    /* send read_addr medium nibble address byte to read from */
    spi_flash_send_byte((read_addr & 0xFF00) >> 8);
    /* send read_addr low nibble address byte to read from */
    spi_flash_send_byte(read_addr & 0xFF);

    /* while there is data to be read */
    while (num_byte_to_read--) {
        /* read a byte from the flash */
        *pbuffer = spi_flash_send_byte(DUMMY_BYTE);
        /* point to the next location where the byte read will be saved */
        pbuffer++;
    }

    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();
}

//读设备ID，检查是否正确连接了SPI Flash
uint32_t spi_flash_read_id(void)
{
    uint32_t temp = 0, temp0 = 0, temp1 = 0, temp2 = 0;

    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();

    /* send "RDID " instruction */
    spi_flash_send_byte(0x9F);

    /* read a byte from the flash */
    temp0 = spi_flash_send_byte(DUMMY_BYTE);

    /* read a byte from the flash */
    temp1 = spi_flash_send_byte(DUMMY_BYTE);

    /* read a byte from the flash */
    temp2 = spi_flash_send_byte(DUMMY_BYTE);

    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();

    temp = (temp0 << 16) | (temp1 << 8) | temp2;

    return temp;
}


void spi_flash_start_read_sequence(uint32_t read_addr)
{
    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();

    /* send "read from memory " instruction */
    spi_flash_send_byte(READ);

    /* send the 24-bit address of the address to read from */
    /* send read_addr high nibble address byte */
    spi_flash_send_byte((read_addr & 0xFF0000) >> 16);
    /* send read_addr medium nibble address byte */
    spi_flash_send_byte((read_addr & 0xFF00) >> 8);
    /* send read_addr low nibble address byte */
    spi_flash_send_byte(read_addr & 0xFF);
}

//读一个字节，适合连续读取
uint8_t spi_flash_read_byte(void)
{
    return(spi_flash_send_byte(DUMMY_BYTE));
}

//发送一个字节并返回接收到的字节，适合连续读写
uint8_t spi_flash_send_byte(uint8_t byte)
{
    /* loop while data register in not emplty */
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE));

    /* send byte through the SPI0 peripheral */
    spi_i2s_data_transmit(SPI0, byte);

    /* wait to receive a byte */
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE));

    /* return the byte read from the SPI bus */
    return(spi_i2s_data_receive(SPI0));
}

//发送一个半字并返回接收到的半字，适合连续读写
uint16_t spi_flash_send_halfword(uint16_t half_word)
{
    /* loop while data register in not emplty */
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE));

    /* send half word through the SPI0 peripheral */
    spi_i2s_data_transmit(SPI0, half_word);

    /* wait to receive a half word */
    while (RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE));

    /* return the half word read from the SPI bus */
    return spi_i2s_data_receive(SPI0);
}

//使能写操作
void spi_flash_write_enable(void)
{
    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();

    /* send "write enable" instruction */
    spi_flash_send_byte(WREN);

    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();
}

//等待写入完成
void spi_flash_wait_for_write_end(void)
{
    uint8_t flash_status = 0;

    /* select the flash: chip select low */
    SPI_FLASH_CS_LOW();

    /* send "read status register" instruction */
    spi_flash_send_byte(RDSR);

    /* loop as long as the memory is busy with a write cycle */
    do {
        /* send a dummy byte to generate the clock needed by the flash
        and put the value of the status register in flash_status variable */
        flash_status = spi_flash_send_byte(DUMMY_BYTE);
    } while ((flash_status & WIP_FLAG) == SET);

    /* deselect the flash: chip select high */
    SPI_FLASH_CS_HIGH();
}
