#include "RCU.h"


#define RCU_MODIFY_4(__delay)   do{                                     \
                                    volatile uint32_t i, reg;           \
                                    if(0 != __delay){                   \
                                        /* 插入一段软件延时（空循环） */   \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      /* 清空原来的 AHB 分频设置 */ \
                                        reg |= RCU_AHB_CKSYS_DIV2;      /* 设置为 2 分频 */ \
                                        /* 此时 AHB总线频率 = 系统时钟 / 2 */            \
                                        RCU_CFG0 = reg;                 \
                                        /* 插入一段软件延时（空循环） */   \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      /* 清空原来的 AHB 分频设置 */\
                                        reg |= RCU_AHB_CKSYS_DIV4;      /* 设置为 4 分频 */\
                                        /* 此时 AHB总线频率 = 系统时钟 / 4 */            \
                                        RCU_CFG0 = reg;                 \
                                        /* 插入一段软件延时（空循环） */   \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      /* 清空原来的 AHB 分频设置 */\
                                        reg |= RCU_AHB_CKSYS_DIV8;      /* 设置为 8 分频 */\
                                        /* 此时 AHB总线频率 = 系统时钟 / 8 */            \
                                        RCU_CFG0 = reg;                 \
                                        /* 插入一段软件延时（空循环） */   \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                        reg = RCU_CFG0;                 \
                                        reg &= ~(RCU_CFG0_AHBPSC);      /* 清空原来的 AHB 分频设置 */\
                                        reg |= RCU_AHB_CKSYS_DIV16;     /* 设置为 16 分频 */\
                                        /* 此时 AHB总线频率 = 系统时钟 / 16 */           \
                                        RCU_CFG0 = reg;                 \
                                        /* 插入一段软件延时（空循环） */   \
                                        for(i=0; i<__delay; i++){       \
                                        }                               \
                                    }                                   \
                                }while(0)


static void _soft_delay_(uint32_t time)
{
    __IO uint32_t i;
    for (i = 0; i < time * 10; i++)
    {
    }
}

void rcu_config(void)
{
    uint32_t timeout = 0U;
	uint32_t stab_flag = 0U;

	// 降频缓冲，防止直接复位时钟树导致总线死机
	RCU_MODIFY_4(0x50);
	
    // 将系统时钟源切换到外部晶振 (HXTAL)，并复位整个 RCU 到出厂状态
	rcu_system_clock_source_config(RCU_CKSYSSRC_HXTAL);
	
	_soft_delay_(200);  // 软件延时，让硬件有时间完成复位
	rcu_deinit();

	// 开启外部高速晶振 (HXTAL)
	RCU_CTL |= RCU_CTL_HXTALEN;

	// 等外部晶振起振并完全稳定，或者等待时间超出设定的超时阈值
	do
	{
		timeout++;
		stab_flag = (RCU_CTL & RCU_CTL_HXTALSTB);
	} while ((0U == stab_flag) && (HXTAL_STARTUP_TIMEOUT != timeout));

	// 故障保护
	if (0U == (RCU_CTL & RCU_CTL_HXTALSTB))
	{
		while (0U == (RCU_CTL & RCU_CTL_HXTALSTB))
		{
		}
	}

    // 使能电源管理单元 (PMU) 时钟，并配置低压检测，为后续提频做电源准备
	RCU_APB1EN |= RCU_APB1EN_PMUEN;
	PMU_CTL |= PMU_CTL_LDOVS;

	// 外部晶振 (HXTAL) 已经稳定，配置三条核心总线的分频系数 (让它们跑在自己能承受的最大速度)

	// AHB 总线 = 系统主频 (240MHz)
	RCU_CFG0 |= RCU_AHB_CKSYS_DIV1;
	// APB2 高速外设总线 = AHB / 2 (120MHz)
	RCU_CFG0 |= RCU_APB2_CKAHB_DIV2;
	// APB1 低速外设总线 = AHB / 4 (60MHz)
	RCU_CFG0 |= RCU_APB1_CKAHB_DIV4;

	/* 核心数学计算：配置主锁相环 (PLL) 参数
     * 公式: VCO = (外部晶振 / PSC) * PLL_N;  系统频率 = VCO / PLL_P 
     * 参数配置: 外部晶振=25M, PSC=25, PLL_N=480, PLL_P=2, PLL_Q=10
     * 演算: VCO = (25 / 25) * 480 = 480MHz;  系统频率 = 480 / 2 = 240MHz 
     */
	RCU_PLL = (25U | (480U << 6U) | (((2U >> 1U) - 1U) << 16U) |
		(RCU_PLLSRC_HXTAL) | (10U << 24U));

	// 开启这个配置好的 PLL
	RCU_CTL |= RCU_CTL_PLLEN;

	// 等待，直到 PLL 完全锁定并稳定
	while (0U == (RCU_CTL & RCU_CTL_PLLSTB))
	{
	}

	// 开启 PMU 的高驱动模式
	PMU_CTL |= PMU_CTL_HDEN;
	while (0U == (PMU_CS & PMU_CS_HDRF))    // 等待高驱动开启成功
	{
	}

	// 选择并进入高驱动模式
	PMU_CTL |= PMU_CTL_HDS;
	while (0U == (PMU_CS & PMU_CS_HDSRF))   // 等待高驱动模式稳定
	{
	}

	// 将系统的主时钟源从外部晶振正式切换到 240MHz 的 PLL
	RCU_CFG0 &= ~RCU_CFG0_SCS;      // 先清空原来的选择
	RCU_CFG0 |= RCU_CKSYSSRC_PLLP;  // 写入新的选择

	// 确认切换成功
	while (0U == (RCU_CFG0 & RCU_SCSS_PLLP))
	{
	}
}
