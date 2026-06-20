/**
 ****************************************************************************************************
 * @file        sys.c
 * @author      ����ԭ���Ŷ�(ALIENTEK)
 * @version     V1.0
 * @date        2021-12-30
 * @brief       ϵͳ��ʼ������(����ʱ������/�жϹ���/GPIO���õ�)
 * @license     Copyright (c) 2020-2032, �������������ӿƼ����޹�˾
 ****************************************************************************************************
 * @attention
 *
 * ʵ��ƽ̨:����ԭ�� STM32F407������
 * ������Ƶ:www.yuanzige.com
 * ������̳:www.openedv.com
 * ��˾��ַ:www.alientek.com
 * �����ַ:openedv.taobao.com
 *
 * �޸�˵��
 * V1.0 20211230
 * ��һ�η���
 ****************************************************************************************************
 */

#include "sys.h"


/**
 * @brief       �����ж�������ƫ�Ƶ�ַ
 * @param       baseaddr: ��ַ
 * @param       offset: ƫ����(������0, ����0X100�ı���)
 * @retval      ��
 */
void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset)
{
    /* ����NVIC��������ƫ�ƼĴ���,VTOR��9λ����,��[8:0]���� */
    SCB->VTOR = baseaddr | (offset & (uint32_t)0xFFFFFE00);
}

/**
 * @brief       ����NVIC����
 * @param       group: 0~4,��5��, ��ϸ���ͼ�: sys_nvic_init��������˵��
 * @retval      ��
 */
static void sys_nvic_priority_group_config(uint8_t group)
{
    uint32_t temp, temp1;
    temp1 = (~group) & 0x07;/* ȡ����λ */
    temp1 <<= 8;
    temp = SCB->AIRCR;      /* ��ȡ��ǰ������ */
    temp &= 0X0000F8FF;     /* �����ǰ���� */
    temp |= 0X05FA0000;     /* д��Կ�� */
    temp |= temp1;
    SCB->AIRCR = temp;      /* ���÷��� */
}

/**
 * @brief       ����NVIC(��������/��ռ���ȼ�/�����ȼ���)
 * @param       pprio: ��ռ���ȼ�(PreemptionPriority)
 * @param       sprio: �����ȼ�(SubPriority)
 * @param       ch: �жϱ��(Channel)
 * @param       group: �жϷ���
 *   @arg       0, ��0: 0λ��ռ���ȼ�, 4λ�����ȼ�
 *   @arg       1, ��1: 1λ��ռ���ȼ�, 3λ�����ȼ�
 *   @arg       2, ��2: 2λ��ռ���ȼ�, 2λ�����ȼ�
 *   @arg       3, ��3: 3λ��ռ���ȼ�, 1λ�����ȼ�
 *   @arg       4, ��4: 4λ��ռ���ȼ�, 0λ�����ȼ�
 * @note        ע�����ȼ����ܳ����趨����ķ�Χ! ����������벻���Ĵ���
 * @retval      ��
 */
void sys_nvic_init(uint8_t pprio, uint8_t sprio, uint8_t ch, uint8_t group)
{
    uint32_t temp;
    sys_nvic_priority_group_config(group);  /* ���÷��� */
    temp = pprio << (4 - group);
    temp |= sprio & (0x0f >> group);
    temp &= 0xf;                            /* ȡ����λ */
    NVIC->ISER[ch / 32] |= 1 << (ch % 32);  /* ʹ���ж�λ(Ҫ����Ļ�,����ICER��ӦλΪ1����) */
    NVIC->IP[ch] |= temp << 4;              /* ������Ӧ���ȼ����������ȼ� */
}

/**
 * @brief       �ⲿ�ж����ú���, ֻ���GPIOA~GPIOI
 * @note        �ú������Զ�������Ӧ�ж�, �Լ�������
 * @param       p_gpiox: GPIOA~GPIOG, GPIOָ��
 * @param       pinx: 0X0000~0XFFFF, ����λ��, ÿ��λ����һ��IO, ��0λ����Px0, ��1λ����Px1, ��������. ����0X0101, ����ͬʱ����Px0��Px8.
 *   @arg       SYS_GPIO_PIN0~SYS_GPIO_PIN15, 1<<0 ~ 1<<15
 * @param       tmode: 1~3, ����ģʽ
 *   @arg       SYS_GPIO_FTIR, 1, �½��ش���
 *   @arg       SYS_GPIO_RTIR, 2, �����ش���
 *   @arg       SYS_GPIO_BTIR, 3, �����ƽ����
 * @retval      ��
 */
void sys_nvic_ex_config(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t tmode)
{
    uint8_t offset;
    uint32_t gpio_num = 0;      /* gpio���, 0~10, ����GPIOA~GPIOG */
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    gpio_num = ((uint32_t)p_gpiox - (uint32_t)GPIOA) / 0X400 ;/* �õ�gpio��� */
    RCC->APB2ENR |= 1 << 14;    /* ʹ��SYSCFGʱ��  */

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* һ����λ��� */
        curpin = pinx & pos;    /* ��������Ƿ�Ҫ���� */

        if (curpin == pos)      /* ��Ҫ���� */
        {
            offset = (pinpos % 4) * 4;
            SYSCFG->EXTICR[pinpos / 4] &= ~(0x000F << offset);  /* ���ԭ�����ã����� */
            SYSCFG->EXTICR[pinpos / 4] |= gpio_num << offset;   /* EXTI.BITxӳ�䵽gpiox.bitx */

            EXTI->IMR |= 1 << pinpos;   /* ����line BITx�ϵ��ж�(���Ҫ��ֹ�жϣ��򷴲�������) */

            if (tmode & 0x01) EXTI->FTSR |= 1 << pinpos;        /* line bitx���¼��½��ش��� */

            if (tmode & 0x02) EXTI->RTSR |= 1 << pinpos;        /* line bitx���¼��������ش��� */
        }
    }
}

/**
 * @brief       GPIO���ù���ѡ������
 * @param       p_gpiox: GPIOA~GPIOI, GPIOָ��
 * @param       pinx: 0X0000~0XFFFF, ����λ��, ÿ��λ����һ��IO, ��0λ����Px0, ��1λ����Px1, ��������. ����0X0101, ����ͬʱ����Px0��Px8.
 *   @arg       SYS_GPIO_PIN0~SYS_GPIO_PIN15, 1<<0 ~ 1<<15
 * @param       afx:0~15, ����AF0~AF15.
 *              AF0~15�������(��������г����õ�, ��ϸ�����STM32F407xx�����ֲ�, Table 7):
 *   @arg       AF0:MCO/SWD/SWCLK/RTC       AF1:TIM1/TIM2               AF2:TIM3~5                  AF3:TIM8~11
 *   @arg       AF4:I2C1~I2C3               AF5:SPI1/SPI2/I2S2          AF6:SPI3/I2S3               AF7:USART1~3
 *   @arg       AF8:USART4~6                AF9;CAN1/CAN2/TIM12~14      AF10:USB_OTG/USB_HS         AF11:ETH
 *   @arg       AF12:FSMC/SDIO/OTG_FS       AF13:DCIM                   AF14:                       AF15:EVENTOUT
 * @retval      ��
 */
void sys_gpio_af_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t afx)
{
    uint32_t pinpos = 0, pos = 0, curpin = 0;;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* һ����λ��� */
        curpin = pinx & pos;    /* ��������Ƿ�Ҫ���� */

        if (curpin == pos)      /* ��Ҫ���� */
        {
            p_gpiox->AFR[pinpos >> 3] &= ~(0X0F << ((pinpos & 0X07) * 4));
            p_gpiox->AFR[pinpos >> 3] |= (uint32_t)afx << ((pinpos & 0X07) * 4);
        }
    }
}

/**
 * @brief       GPIOͨ������
 * @param       p_gpiox: GPIOA~GPIOI, GPIOָ��
 * @param       pinx: 0X0000~0XFFFF, ����λ��, ÿ��λ����һ��IO, ��0λ����Px0, ��1λ����Px1, ��������. ����0X0101, ����ͬʱ����Px0��Px8.
 *   @arg       SYS_GPIO_PIN0~SYS_GPIO_PIN15, 1<<0 ~ 1<<15
 *
 * @param       mode: 0~3; ģʽѡ��, ��������:
 *   @arg       SYS_GPIO_MODE_IN,  0, ����ģʽ(ϵͳ��λĬ��״̬)
 *   @arg       SYS_GPIO_MODE_OUT, 1, ���ģʽ
 *   @arg       SYS_GPIO_MODE_AF,  2, ���ù���ģʽ
 *   @arg       SYS_GPIO_MODE_AIN, 3, ģ������ģʽ
 *
 * @param       otype: 0 / 1; �������ѡ��, ��������:
 *   @arg       SYS_GPIO_OTYPE_PP, 0, �������
 *   @arg       SYS_GPIO_OTYPE_OD, 1, ��©���
 *
 * @param       ospeed: 0~3; ����ٶ�, ��������:
 *   @arg       SYS_GPIO_SPEED_LOW,  0, ����
 *   @arg       SYS_GPIO_SPEED_MID,  1, ����
 *   @arg       SYS_GPIO_SPEED_FAST, 2, ����
 *   @arg       SYS_GPIO_SPEED_HIGH, 3, ����
 *
 * @param       pupd: 0~3: ����������, ��������:
 *   @arg       SYS_GPIO_PUPD_NONE, 0, ����������
 *   @arg       SYS_GPIO_PUPD_PU,   1, ����
 *   @arg       SYS_GPIO_PUPD_PD,   2, ����
 *   @arg       SYS_GPIO_PUPD_RES,  3, ����
 *
 * @note:       ע��: ������ģʽ(��ͨ����/ģ������)��, OTYPE��OSPEED������Ч!!
 * @retval      ��
 */
void sys_gpio_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint32_t mode, uint32_t otype, uint32_t ospeed, uint32_t pupd)
{
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;      /* һ����λ��� */
        curpin = pinx & pos;    /* ��������Ƿ�Ҫ���� */

        if (curpin == pos)      /* ��Ҫ���� */
        {
            p_gpiox->MODER &= ~(3 << (pinpos * 2)); /* �����ԭ�������� */
            p_gpiox->MODER |= mode << (pinpos * 2); /* �����µ�ģʽ */

            if ((mode == 0X01) || (mode == 0X02))   /* ��������ģʽ/���ù���ģʽ */
            {
                p_gpiox->OSPEEDR &= ~(3 << (pinpos * 2));       /* ���ԭ�������� */
                p_gpiox->OSPEEDR |= (ospeed << (pinpos * 2));   /* �����µ��ٶ�ֵ */
                p_gpiox->OTYPER &= ~(1 << pinpos) ;             /* ���ԭ�������� */
                p_gpiox->OTYPER |= otype << pinpos;             /* �����µ����ģʽ */
            }

            p_gpiox->PUPDR &= ~(3 << (pinpos * 2)); /* �����ԭ�������� */
            p_gpiox->PUPDR |= pupd << (pinpos * 2); /* �����µ������� */
        }
    }
}

/**
 * @brief       ����GPIOĳ�����ŵ����״̬
 * @param       p_gpiox: GPIOA~GPIOI, GPIOָ��
 * @param       pinx: 0X0000~0XFFFF, ����λ��, ÿ��λ����һ��IO, ��0λ����Px0, ��1λ����Px1, ��������. ����0X0101, ����ͬʱ����Px0��Px8.
 *   @arg       SYS_GPIO_PIN0~SYS_GPIO_PIN15, 1<<0 ~ 1<<15
 * @param       status: 0/1, ����״̬(�����λ��Ч), ��������:
 *   @arg       0, ����͵�ƽ
 *   @arg       1, ����ߵ�ƽ
 * @retval      ��
 */
void sys_gpio_pin_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t status)
{
    if (status & 0X01)
    {
        p_gpiox->BSRR |= pinx;  /* ����GPIOx��pinxΪ1 */
    }
    else
    {
        p_gpiox->BSRR |= (uint32_t)pinx << 16;  /* ����GPIOx��pinxΪ0 */
    }
}

/**
 * @brief       ��ȡGPIOĳ�����ŵ�״̬
 * @param       p_gpiox: GPIOA~GPIOG, GPIOָ��
 * @param       pinx: 0X0000~0XFFFF, ����λ��, ÿ��λ����һ��IO, ��0λ����Px0, ��1λ����Px1, ��������. һ��ֻ�ܶ�ȡһ��GPIO��
 *   @arg       SYS_GPIO_PIN0~SYS_GPIO_PIN15, 1<<0 ~ 1<<15
 * @retval      ��������״̬, 0, �͵�ƽ; 1, �ߵ�ƽ
 */
uint8_t sys_gpio_pin_get(GPIO_TypeDef *p_gpiox, uint16_t pinx)
{
    if (p_gpiox->IDR & pinx)
    {
        return 1;   /* pinx��״̬Ϊ1 */
    }
    else
    {
        return 0;   /* pinx��״̬Ϊ0 */
    }
}

/**
 * @brief       ִ��: WFIָ��(ִ�����ָ�����͹���״̬, �ȴ��жϻ���)
 * @param       ��
 * @retval      ��
 */
void sys_wfi_set(void)
{
    __ASM volatile("wfi");
}

/**
 * @brief       �ر������ж�(���ǲ�����fault��NMI�ж�)
 * @param       ��
 * @retval      ��
 */
void sys_intx_disable(void)
{
    __ASM volatile("cpsid i");
}

/**
 * @brief       ���������ж�
 * @param       ��
 * @retval      ��
 */
void sys_intx_enable(void)
{
    __ASM volatile("cpsie i");
}

/**
 * @brief       ����ջ����ַ
 * @note        ���ĺ�X, ����MDK��, ʵ����û�����
 * @param       addr: ջ����ַ
 * @retval      ��
 */
void sys_msr_msp(uint32_t addr)
{
    __set_MSP(addr);    /* ����ջ����ַ */
}

/**
 * @brief       �������ģʽ
 * @param       ��
 * @retval      ��
 */
void sys_standby(void)
{
    RCC->APB1ENR |= 1 << 28;    /* ʹ�ܵ�Դʱ�� */
    PWR->CSR |= 1 << 8;         /* ����WKUP���ڻ��� */
    PWR->CR |= 1 << 2;          /* ���WKUP ��־ */
    PWR->CR |= 1 << 1;          /* PDDS = 1, �����������˯��ģʽ(PDDS) */
    SCB->SCR |= 1 << 2;         /* ʹ��SLEEPDEEPλ (SYS->CTRL) */
    sys_wfi_set();              /* ִ��WFIָ��, �������ģʽ */
}

/**
 * @brief       ϵͳ����λ
 * @param       ��
 * @retval      ��
 */
void sys_soft_reset(void)
{
    SCB->AIRCR = 0X05FA0000 | (uint32_t)0x04;
}

/**
 * @brief       ʱ�����ú���
 * @param       plln: ��PLL��Ƶϵ��(PLL��Ƶ), ȡֵ��Χ: 64~432.
 * @param       pllm: ��PLL����ƵPLLԤ��Ƶϵ��(��PLL֮ǰ�ķ�Ƶ), ȡֵ��Χ: 2~63.
 * @param       pllp: ��PLL��p��Ƶϵ��(PLL֮��ķ�Ƶ), ��Ƶ����Ϊϵͳʱ��, ȡֵ��Χ: 2, 4, 6, 8.(������4��ֵ)
 * @param       pllq: ��PLL��q��Ƶϵ��(PLL֮��ķ�Ƶ), ȡֵ��Χ: 2~15.
 * @note
 *
 *              Fvco: VCOƵ��
 *              Fsys: ϵͳʱ��Ƶ��, Ҳ����PLL��p��Ƶ���ʱ��Ƶ��
 *              Fq:   ��PLL��q��Ƶ���ʱ��Ƶ��
 *              Fs:   ��PLL����ʱ��Ƶ��, ������HSI, HSE��.
 *              Fvco = Fs * (plln / pllm);
 *              Fsys = Fvco / pllp = Fs * (plln / (pllm * pllp));
 *              Fq   = Fvco / pllq = Fs * (plln / (pllm * pllq));
 *
 *              �ⲿ����Ϊ 8M��ʱ��, �Ƽ�ֵ: plln = 336, pllm = 8, pllp = 2, pllq = 7.
 *              �õ�:Fvco = 8 * (336 / 8) = 336Mhz
 *                   Fsys = pll_p_ck = 336 / 2 = 168Mhz
 *                   Fq   = pll_q_ck = 336 / 7 = 48Mhz
 *
 *              F407Ĭ����Ҫ���õ�Ƶ������:
 *              CPUƵ��(HCLK) = pll_p_ck = 168Mhz
 *              AHB1/2/3(rcc_hclk1/2/3) = 168Mhz
 *              APB1(rcc_pclk1) = pll_p_ck / 4 = 42Mhz
 *              APB1(rcc_pclk2) = pll_p_ck / 2 = 84Mhz
 *
 * @retval      �������: 0, �ɹ�; 1, HSE����; 2, PLL1����; 3, PLL2����; 4, �л�ʱ�Ӵ���;
 */
uint8_t sys_clock_set(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq)
{
    uint32_t retry = 0;
    uint8_t retval = 0;
    uint8_t swsval = 0;

    RCC->CR |= 1 << 16; /* HSEON = 1, ����HSE */

    while (((RCC->CR & (1 << 17)) == 0) && (retry < 0X7FFF))
    {
        retry++;        /* �ȴ�HSE RDY */
    }

    if (retry == 0X7FFF)
    {
        retval = 1;     /* HSE�޷����� */
    }
    else
    {
        RCC->APB1ENR |= 1 << 28;                /* ��Դ�ӿ�ʱ��ʹ�� */
        PWR->CR |= 3 << 14;                     /* ������ģʽ,ʱ�ӿɵ�168Mhz */
        
        RCC->PLLCFGR |= 0X3F & pllm;            /* ������PLLԤ��Ƶϵ��,  PLLM[5:0]: 2~63 */
        RCC->PLLCFGR |= plln << 6;              /* ������PLL��Ƶϵ��,    PLLN[8:0]: 192~432 */
        RCC->PLLCFGR |= ((pllp >> 1) - 1) << 16;/* ������PLL��p��Ƶϵ��, PLLP[1:0]: 0~3, ����2~8��Ƶ */
        RCC->PLLCFGR |= pllq << 24;             /* ������PLL��q��Ƶϵ��, PLLQ[3:0]: 2~15 */
        RCC->PLLCFGR |= 1 << 22;                /* ������PLL��ʱ��Դ����HSE */

        RCC->CFGR |= 0 << 4;                    /* HPRE[3:0]  = 0, AHB  ����Ƶ, rcc_hclk1/2/3 = pll_p_ck */
        RCC->CFGR |= 5 << 10;                   /* PPRE1[2:0] = 5, APB1 4��Ƶ   rcc_pclk1 = pll_p_ck / 4 */
        RCC->CFGR |= 4 << 13;                   /* PPRE2[2:0] = 4, APB2 2��Ƶ   rcc_pclk2 = pll_p_ck / 2 */

        RCC->CR |= 1 << 24;                     /* ����PLL */

        retry = 0;
        while ((RCC->CR & (1 << 25)) == 0)      /* �ȴ�PLL׼���� */
        {
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 2;                     /* ��PLL�޷����� */
                break;
            }
        }

        FLASH->ACR |= 1 << 8;                   /* ָ��Ԥȡʹ�� */
        FLASH->ACR |= 1 << 9;                   /* ָ��cacheʹ�� */
        FLASH->ACR |= 1 << 10;                  /* ����cacheʹ�� */
        FLASH->ACR |= 5 << 0;                   /* 5��CPU�ȴ����� */
        
        RCC->CFGR |= 2 << 0;                    /* ѡ����PLL��Ϊϵͳʱ�� */
        
        retry = 0;
        while (swsval != 3)                     /* �ȴ��ɹ���ϵͳʱ��Դ�л�Ϊpll_p_ck */
        {
            swsval = (RCC->CFGR & 0X0C) >> 2;   /* ��ȡSWS[1:0]��״̬, �ж��Ƿ��л��ɹ� */
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 4; /* �޷��л�ʱ�� */
                break;
            }
        }
    }

    return retval;
}

/**
 * @brief       ϵͳʱ�ӳ�ʼ������
 * @param       plln: PLL1��Ƶϵ��(PLL��Ƶ), ȡֵ��Χ: 4~512.
 * @param       pllm: PLL1Ԥ��Ƶϵ��(��PLL֮ǰ�ķ�Ƶ), ȡֵ��Χ: 2~63.
 * @param       pllp: PLL1��p��Ƶϵ��(PLL֮��ķ�Ƶ), ��Ƶ����Ϊϵͳʱ��, ȡֵ��Χ: 2~128.(�ұ�����2�ı���)
 * @param       pllq: PLL1��q��Ƶϵ��(PLL֮��ķ�Ƶ), ȡֵ��Χ: 1~128.
 * @retval      ��
 */
void sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq)
{
    RCC->CR = 0x00000001;           /* ����HISON, �����ڲ�����RC�񵴣�����λȫ���� */
    RCC->CFGR = 0x00000000;         /* CFGR���� */
    RCC->PLLCFGR = 0x00000000;      /* PLLCFGR���� */
    RCC->CIR = 0x00000000;          /* CIR���� */
    
    sys_clock_set(plln, pllm, pllp, pllq);  /* ����ʱ�� */

    /* �����ж�����ƫ�� */
#ifdef  VECT_TAB_RAM
    sys_nvic_set_vector_table(SRAM_BASE, 0x0);
#else
    sys_nvic_set_vector_table(FLASH_BASE, 0x0);
#endif
}







