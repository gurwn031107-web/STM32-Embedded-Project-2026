/*
 * cds.c
 *
 *  Created on: 2026. 11. 3.
 *      Author: kccistc
 */


#include "cds.h"

#define ADC_CH_COUNT	2

void cds_init(void)
{
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)value, ADC_CH_COUNT);
}

uint16_t cds_getValue(void)
{

}
