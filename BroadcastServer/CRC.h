/**
******************************************************************************
* @file 
* @author   Xiele FlyPin
* @version  V1.0.0
* @date     //2010
* @brief    
******************************************************************************
*/ 

/**
********************************************************************************
Author     Data        CR_Number   Description 
Name     MM/DD/YYYY     
------   ----------    ----------  ---------------------

********************************************************************************
*/

#pragma once

#include "includes.h"

typedef struct
{
	u8 buf[1024];
	u16 len;
}LED_frame_s;

u16 Modbus_CRC_check( LED_frame_s *p_frame );
u16 Modbus_CRC_attach( LED_frame_s *p_frame );
int modbus_collect(LED_frame_s * p_frame);
int Dmodbus_collect(LED_frame_s * p_frame);

extern LED_frame_s  mb_frame;

