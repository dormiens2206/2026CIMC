#include "uart_protocol.h"
#include "bootloader.h"

extern __IO uint32_t g_sys_ms;

static uint32_t g_boot_fw_size = 0U;
static uint8_t  g_boot_fw_ready = 0U;
static uint8_t  g_boot_upgrade_done = 0U;

#define BOOT_RECV_FW_BUF_SIZE   FW_TEMP_SIZE

uint8_t Bootloader_IsFirmwareReady(void)
{
    return g_boot_fw_ready;
}

uint8_t Bootloader_IsUpgradeDone(void)
{
    return g_boot_upgrade_done;
}

static uint16_t Boot_GetIdFromRawFrame(uint8_t *bin, uint16_t len)
{
    if((bin != 0) && (len >= 4U))
    {
			return ((uint16_t)bin[2] << 8) | bin[3];
    }

    return DEVICE_ID_DEFAULT;
}

static uint16_t Boot_CleanAscii(uint8_t *in, uint16_t in_len, uint8_t *out, uint16_t out_max)
{
	uint16_t i;
	uint16_t out_len = 0U;

	for(i = 0U; i < in_len; i++)
	{
		if((in[i] == ' ') || (in[i] == '\r') || (in[i] == '\n') || (in[i] == '\t'))
		{
				continue;
		}

		if(out_len < out_max)
		{
				out[out_len++] = in[i];
		}
	}

	return out_len;
}

#define BOOT_RECV_FW_BUF_SIZE   FW_TEMP_SIZE

static void Boot_FirmwareFail(void)
{
    g_boot_fw_size = 0U;
    g_boot_fw_ready = 0U;
}

static uint8_t Boot_ReceiveRawFirmware(void)
{
    static uint8_t fw_buf[BOOT_RECV_FW_BUF_SIZE];

    uint16_t rx_len;
    uint32_t start_ms;
    uint32_t last_rx_ms;
    uint32_t total_len;
    uint32_t remain_size;
    uint8_t got_data;

    USART485_ClearRx();

    start_ms = g_sys_ms;
    last_rx_ms = g_sys_ms;
    total_len = 0U;
    got_data = 0U;

    while(1)
    {
        if(total_len < BOOT_RECV_FW_BUF_SIZE)
        {
            remain_size = BOOT_RECV_FW_BUF_SIZE - total_len;

            if(remain_size > 65535U)
            {
                remain_size = 65535U;
            }

            rx_len = USART485_Read(&fw_buf[total_len], (uint16_t)remain_size);
        }
        else
        {
            Boot_FirmwareFail();
            return 0U;
        }

        if(rx_len > 0U)
        {
            got_data = 1U;
            total_len += rx_len;
            last_rx_ms = g_sys_ms;
        }

        /* 收到数据后 500ms 没有新数据，认为 bin 发送结束 */
        if((got_data != 0U) && ((g_sys_ms - last_rx_ms) > 500U))
        {
            break;
        }

        /* 12s 还没收到第一段固件 */
        if((got_data == 0U) && ((g_sys_ms - start_ms) > 12000U))
        {
            Boot_FirmwareFail();
            return 0U;
        }

        /* 防止异常卡死 */
        if((g_sys_ms - start_ms) > 30000U)
        {
            Boot_FirmwareFail();
            return 0U;
        }
    }

    g_boot_fw_size = total_len;

    if(g_boot_fw_size <= (BOOT_FW_MAGIC_SIZE + 8U))
    {
        Boot_FirmwareFail();
        return 0U;
    }

    if((fw_buf[0] != 0x5AU) ||
       (fw_buf[1] != 0xA5U) ||
       (fw_buf[2] != 0xC3U) ||
       (fw_buf[3] != 0x3CU))
    {
        Boot_FirmwareFail();
        return 0U;
    }

    if(Boot_Erase_Temp_Area() != BOOT_OK)
    {
        Boot_FirmwareFail();
        return 0U;
    }

    if(Boot_Write_Temp(0U, fw_buf, g_boot_fw_size) != BOOT_OK)
    {
        Boot_FirmwareFail();
        return 0U;
    }

    if(Boot_Check_Temp_Firmware(g_boot_fw_size) != BOOT_OK)
    {
        Boot_FirmwareFail();
        return 0U;
    }

    g_boot_fw_ready = 1U;
    return 1U;
}
void Protocol_Task(void)
{
	uint8_t rx_ascii[320];
	uint8_t clean_ascii[320];
	uint8_t rx_bin[PROTOCOL_MAX_FRAME_SIZE];

	uint16_t rx_ascii_len;
	uint16_t clean_len;
	uint16_t rx_bin_len;
	uint16_t err_id;

	ProtocolFrame frame;
	int ret;

	rx_ascii_len = USART485_Read(rx_ascii, sizeof(rx_ascii));

	if(rx_ascii_len == 0U)
	{
		return;
	}

	clean_len = Boot_CleanAscii(rx_ascii, rx_ascii_len, clean_ascii, sizeof(clean_ascii));

	if(clean_len == 0U)
	{
		return;
	}

	ret = Protocol_HexStr2Bin(clean_ascii, clean_len, rx_bin, &rx_bin_len);

	if(ret != PROTOCOL_OK)
	{
		Protocol_SendError(DEVICE_ID_DEFAULT);
		return;
	}

	err_id = Boot_GetIdFromRawFrame(rx_bin, rx_bin_len);

	ret = Protocol_Frame(rx_bin, rx_bin_len, &frame);

	if(ret != PROTOCOL_OK)
	{
		Protocol_SendError(err_id);
		return;
	}

	/*
	 * Bootloader 阶段只处理升级相关命令。
	 * 这里不严格判断设备 ID，是为了避免 APP 修改 ID 后，
	 * Bootloader 还不知道新 ID 导致 N 项直接沉默。
	 */
	if(frame.frame_type == FRAME_TYPE_BEAT && frame.cmd == CMD_FIND_DEV)
	{
		Protocol_SendHeartBeat(DEVICE_ID_DEFAULT);
		return;
	}

	if(frame.frame_type != FRAME_TYPE_CMD)
	{
		Protocol_SendError(frame.id);
		return;
	}

	switch(frame.cmd)
	{
		case CMD_TRANS_DATA:
			{
					if(Boot_ReceiveRawFirmware())
					{
							Protocol_SendOK(frame.id, CMD_TRANS_DATA);
					}
					else
					{
							Protocol_SendErrorCmd(frame.id, CMD_TRANS_DATA);

							while(!USART485_IsSendFinish())
							{
							}

							Boot_Erase_Temp_Area();
					}
			} break;

	  case CMD_UPDATE_EX:
			{
				/*
				 * 0x0503：执行升级。
				 * 先确认 0x0502 已经收到正确固件，再回 OK，再搬运。
				 */
				if(g_boot_fw_ready == 0U)
				{
						/*
						 * 没有先通过 0502 收到正确固件，就不允许 0503 执行升级。
						 */
						Protocol_SendErrorCmd(frame.id, CMD_UPDATE_EX);
						return;
				}

				if(Boot_Check_Temp_Firmware(g_boot_fw_size) != BOOT_OK)
				{
						/*
						 * 执行升级前再次校验失败，返回 FF + 0503。
						 */
						Protocol_SendErrorCmd(frame.id, CMD_UPDATE_EX);
						return;
				}

				Protocol_SendOK(frame.id, CMD_UPDATE_EX);

				while(!USART485_IsSendFinish())
				{
				}

				if(Boot_Copy_Temp_To_App(g_boot_fw_size) == BOOT_OK)
				{
					g_boot_upgrade_done = 1U;
				}
				else
				{
					g_boot_upgrade_done = 0U;
				}
			} break;

		default:
		{
			Protocol_SendError(frame.id);
		} break;
	}
}
