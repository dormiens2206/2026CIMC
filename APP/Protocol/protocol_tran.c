#include "uart_protocol.h"
#include "sample_task.h"
#include "app_task.h"

static uint16_t device_id = DEFAULT_DEVICE_ID; // 默认设备ID，后续可以通过参数配置修改
static uint32_t device_time = 0;			   // 设备时间戳，单位秒
static uint8_t baud_code = BAUD_CODE_19200;	   // 波特率默认19200

/* 收到重启命令后，先回 OK，再由主循环延迟复位 */
static volatile uint8_t protocol_need_reset = 0;

extern const uint8_t g_app_version[4];

void Protocol_Process(uint8_t *rx_buf, uint16_t rx_len)
{
	static uint8_t clean_buf[PROTOCOL_MAX_FRAME_SIZE * 2];
	static uint8_t bin_buff[PROTOCOL_MAX_FRAME_SIZE];
	static ProtocolFrame current_frame;

	uint16_t clean_len = 0;
	uint16_t bin_len = 0;

	int ret;

	memset(&current_frame, 0, sizeof(current_frame));

	if (rx_buf == 0 || rx_len == 0)
	{
		return;
	}

	/* 每次处理命令前，从 Flash 参数镜像同步当前 ID 和波特率 */
	device_id = g_system_params.device_id;
	baud_code = g_system_params.baud_code;

	// Hex转二进制
	ret = Protocol_HexStr2Bin(rx_buf, rx_len, bin_buff, &bin_len);
	if (ret != PROTOCOL_OK)
	{
			Protocol_SendError(device_id);
			return;
	}

	// 解析帧格式
	ret = Protocol_Frame(bin_buff, bin_len, &current_frame);
	if (ret != PROTOCOL_OK)
	{
		// 帧格式错误，发送错误应答
		Protocol_SendError(device_id);
		return;
	}

	// 判断设备id
	if (current_frame.id != PROTOCOL_BROADCAST_ID && current_frame.id != device_id)
	{
		// 设备ID不匹配，忽略此帧
		return;
	}

	Protocol_Dispatch(&current_frame);
}

void Protocol_Dispatch(ProtocolFrame *frame)
{
	if (frame == 0)
		return;

	// 自动上报期间，除了停止自动上报，其他命令都不回复
	if (App_AutoReporting())
	{
		if (frame->cmd != CMD_TIME_STOP)
		{
			return;
		}
	}

	/* 广播寻找设备。
	 * 寻找设备是 心跳帧 0x05 + 命令字 0xFFFF，
	 */
	if ((frame->frame_type == FRAME_TYPE_BEAT) && (frame->cmd == CMD_FIND_DEV))
	{
		Protocol_SendHeartBeat(device_id);
		return;
	}

	/* 后面的 switch 只处理上位机下发命令帧 */
	if (frame->frame_type != FRAME_TYPE_CMD)
	{
		Protocol_SendError(device_id);
		return;
	}

	switch (frame->cmd)
	{
	case CMD_RESET: // 0x0101: 设备重启
	{
		App_ResetRequest(device_id, frame->cmd);
	}
	break;

	case CMD_VERSION_Q: // 0x0104: 查询固件版本
	{
		// 初始固件版本号为 20.01.00.00
		uint8_t version_buf[4] = {0x14, 0x01, 0x00, 0x00};
		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, version_buf, 4);
	}
	break;

	case CMD_TIME_SET: // 0x0105: 设置设备时间
	{
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		Time_t new_time;
		// 从payload提取4字节时间戳，转成小端整数
		memcpy(&device_time, frame->payload, 4);
		device_time = Protocol_Swap32(device_time);

		Timestamp_To_TimeStruct(device_time, &new_time);
		// 设置RTC时间
		if (RTC_SetTime(&new_time) != SUCCESS)
		{
			Protocol_SendError(device_id);
			break;
		}

		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_TIME_Q: // 0x0106: 查询设备时间
	{
		Time_t current_time;
		uint32_t current_timestamp = 0;
		uint32_t time_turn = 0;

		// 从RTC读取时间
		RTC_GetTime(&current_time);
		// 将时间转换成秒
		current_timestamp = TimeStruct_To_Timestamp(&current_time);
		// 转成大端
		time_turn = Protocol_Swap32(current_timestamp);

		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, (uint8_t *)&time_turn, 4);
	}
	break;

	case CMD_ID_SET: // 0x01A1: 设置设备ID
	{
		if (frame->len != 2)
		{
			Protocol_SendError(device_id);
			break;
		}
		// 从上位机发来的payload中提取新的id
		uint16_t new_id = ((uint16_t)frame->payload[0] << 8) | frame->payload[1];

		if (!Param_SetDeviceID(new_id))
		{
			Protocol_SendError(device_id);
			break;
		}
		device_id = g_system_params.device_id;

		// 回复OK，注意回复帧里的 ID 要用新 ID
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_BAUD_SET: // 0x01A2: 设置波特率
	{
		if (frame->len != 1)
		{
			Protocol_SendError(device_id);
			break;
		}

		if (!Param_SetBaudCode(frame->payload[0]))
		{
			Protocol_SendError(device_id);
			break;
		}

		baud_code = g_system_params.baud_code;

		Protocol_SendOK(device_id, frame->cmd);

		/*
		 * 必须等 OK 帧真正发完，再切换波特率。
		 * 只 delay 100ms 大多数时候能用，但不如等待发送完成稳。
		 */
		while(!USART485_IsSendFinish())
		{
		}

		delay_1ms(20);

		USART485_Init();
	}
	break;

	case CMD_ID_Q: // 0x0111: 查询设备ID (上位机会广播发此命令)
	{
		uint8_t id_buf[2];
		// id大端发送：高位在前，低位在后
		id_buf[0] = (device_id >> 8) & 0xFF;
		id_buf[1] = device_id & 0xFF;

		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, id_buf, 2);
	}
	break;

	case CMD_BAUD_Q: // 0x0112: 查询波特率
	{
		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, &baud_code, 1);
	}
	break;

	case CMD_CH0_Q: // 0x0201: 查询CH0
		{
			uint8_t ch0_buf[4];
			float ch0_value;

			if (frame->len != 0)
			{
				Protocol_SendError(device_id);
				break;
			}

			ch0_value = Sample_Getch0();
			Protocol_Float2BigEnd(&ch0_value, ch0_buf);

			Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, ch0_buf, 4);
		}
		break;

	case CMD_CH1_Q: 
		{
			uint8_t ch1_buf[4];
			float ch1_value;

			if (frame->len != 0)
			{
				Protocol_SendError(device_id);
				break;
			}

			ch1_value = Sample_Getch1();
			Protocol_Float2BigEnd(&ch1_value, ch1_buf);

			Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, ch1_buf, 4);
		}
		break;

	case CMD_CH2_Q:
		{
			uint8_t ch2_buf[4];
			float ch2_value;

			if (frame->len != 0)
			{
				Protocol_SendError(device_id);
				break;
			}

			ch2_value = Sample_Getch2();
			Protocol_Float2BigEnd(&ch2_value, ch2_buf);

			Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, ch2_buf, 4);
		}
		break;


	case CMD_CH0_SET: // 0x0241: 设置CH0变比
	{
		float ratio;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		ratio = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH0Ratio(ratio);
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_CH1_SET: // 0x0242: 设置CH1变比
	{
		float ratio;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		ratio = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH1Ratio(ratio);
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_CH2_SET:		// 0x0243: 设置CH2变比
	{
		float ratio;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		ratio = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH2Ratio(ratio);
		Protocol_SendOK(device_id, frame->cmd);
	}	
	break;

	case CMD_INTERVAL_SET: // 0x0261: 设置数据上报时间间隔
	{
		if (frame->len != 1 || !Param_SetReportInterval(frame->payload[0]))
		{
			Protocol_SendError(device_id);
			break;
		}
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

		case CMD_DAC_SET: // 0x0301: 设置DAC输出电压
		// 提取 payload 中的 2 字节 -> 驱动 DAC 输出
		{
			uint16_t dac_raw;

			if (frame->len != 2)
			{
				Protocol_SendError(device_id);
				break;
			}

			dac_raw = ((uint16_t)frame->payload[0] << 8) | frame->payload[1];

			if (dac_raw > 4095)
			{
				Protocol_SendError(device_id);
				break;
			}

			DAC_Set_Raw(DAC0_CHANNEL, dac_raw);

			/* 保存 DAC 输出值，保证重启后 CH1 回读源不丢 */
			Param_SetDacRaw(dac_raw);

			Protocol_SendOK(device_id, frame->cmd);
		}
		break;


	case CMD_TIME_START: // 0x0302: 启动定时上报
	{
		if (frame->len != 0)
		{
			Protocol_SendError(device_id);
			break;
		}

		App_StartAutoReport(device_id, frame->cmd);
	}
	break;

	case CMD_TIME_STOP: // 0x0303: 停止定时上报
	{
		if (frame->len != 0)
		{
			Protocol_SendError(device_id);
			break;
		}

		App_StopAutoReport(device_id, frame->cmd);
	}
	break;

	case CMD_ENTER_SLEEP: // 0x03AA: 进入睡眠模式
	{
		if (frame->len != 0)
		{
			Protocol_SendError(device_id);
			break;
		}

		App_RequestSleep(device_id, frame->cmd);
	}
	break;

	case CMD_THRESHOLD_R: // 0x0400: 批量读取阈值
	{
		uint8_t th_buf[12];
		Protocol_Float2BigEnd(&g_system_params.ch0_threshold, &th_buf[0]);
		Protocol_Float2BigEnd(&g_system_params.ch1_threshold, &th_buf[4]);
		Protocol_Float2BigEnd(&g_system_params.ch2_threshold, &th_buf[8]);

		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, th_buf, 12);
	}
	break;

	case CMD_TH_CH0_R: // 0x0401: 读取CH0单通道阈值
	{
		uint8_t buf[4];
		Protocol_Float2BigEnd(&g_system_params.ch0_threshold, buf);
		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, buf, 4);
	}
	break;

	case CMD_TH_CH1_R: // 0x0402: 读取CH1单通道阈值
	{
		uint8_t buf[4];
		Protocol_Float2BigEnd(&g_system_params.ch1_threshold, buf);
		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, buf, 4);
	}
	break;
	case CMD_TH_CH2_R: // 0x0403: 读取CH2单通道阈值
	{
		uint8_t buf[4];
		Protocol_Float2BigEnd(&g_system_params.ch2_threshold, buf);
		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, buf, 4);
	}
	break;
	case CMD_TH_CH0_W: // 0x0411: 写入CH0阈值
	{
		float threshold;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		threshold = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH0Threshold(threshold);
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_TH_CH1_W: // 0x0412: 写入CH1阈值
	{
		float threshold;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		threshold = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH1Threshold(threshold);
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;
	case CMD_TH_CH2_W: // 0x0413: 写入CH2阈值
	{
		float threshold;
		if (frame->len != 4)
		{
			Protocol_SendError(device_id);
			break;
		}
		threshold = Protocol_BigEndian2Float(frame->payload);
		Param_SetCH2Threshold(threshold);
		Protocol_SendOK(device_id, frame->cmd);
	}
	break;

	case CMD_UPDATE_Q: // 0x0501: 升级请求
	{
		if (frame->len != 0)
		{
			Protocol_SendError(device_id);
			break;
		}

		App_RequestBootloader(device_id, frame->cmd);
	}
	break;

	case CMD_TRANS_DATA:		// 0x0502: 固件升级数据包传输
	{
		Protocol_SendError(device_id);
	}
	break;

	case CMD_UPDATE_EX:		// 0x0503: 执行升级
	{
		Protocol_SendError(device_id);
	}
	break;

	case CMD_BACK_Q:		// 0x0504: 版本回退请求，返回前一版本，Bootloader区下发
	{
		Protocol_SendError(device_id);
	}
	break;

	case CMD_BF_Q:		// 0x0505: 查询备份版本信息
	{
		Protocol_SendError(device_id);
	}
	break;

	case CMD_EXIT_BOOT:			// 0x0599: 退出Bootloader，进入APP
	{
		Protocol_SendError(device_id);
	}
	break;

	case CMD_ALARM_SUB: // 0x0601: 询问是否上报告警
	{
		Alarm_SetMode(device_id, frame->cmd, frame->payload, frame->len);
	}
	break;

	case CMD_ALARM_Q: // 0x0602: 查询告警记录
	{
		Alarm_Query();
	}
	break;

	case CMD_ALARM_CLEAR: // 0x0603: 清除告警记录
	{
		Alarm_Clear(device_id, frame->cmd);
	}
	break;

	case CMD_BREAK_Q: // 0x0606: 断线检测查询
	{
		uint8_t break_flag;

		if (frame->len != 0)
		{
			Protocol_SendError(device_id);
			break;
		}

		/* 返回值 1 字节：未断线 = 0x00，断线 = 0xFF */
		break_flag = (g_sample_data.current_break != 0U) ? 0xFF : 0x00;

		Protocol_SendFrame(device_id, FRAME_TYPE_REPLY, frame->cmd, &break_flag, 1);
	}
	break;

	default:
		Protocol_SendError(device_id);
		break;
	}
}
