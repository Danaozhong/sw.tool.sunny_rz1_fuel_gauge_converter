/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/

#include "Can.h"

#include "stm32f3xx.h"
#include "stm32f3xx_hal_can.h"
//#include "Cpu.h"
//#include "Mcu.h"
#include "CanIf_Cbk.h"
#if defined(USE_DET)
#include "Det.h"
#endif
#if defined(USE_DEM)
#include "Dem.h"
#endif
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "Os.h"
//#include "isr.h"
//#include "arc.h"


/* CONFIGURATION NOTES
 * ------------------------------------------------------------------
 * - CanHandleType must be CAN_ARC_HANDLE_TYPE_BASIC
 *   i.e. CanHandleType=CAN_ARC_HANDLE_TYPE_FULL NOT supported
 *   i.e CanIdValue is NOT supported
 * - All CanXXXProcessing must be CAN_ARC_PROCESS_TYPE_INTERRUPT
 *   ie CAN_ARC_PROCESS_TYPE_POLLED not supported
 * - HOH's for Tx are global and Rx are for each controller
 */

/* IMPLEMENTATION NOTES
 * -----------------------------------------------
 * - A HOH us unique for a controller( not a config-set )
 * - Hrh's are numbered for each controller from 0
 * - loopback in HW NOT supported
 * - Only one transmit mailbox is used because otherwise
 *   we cannot use tx_confirmation since there is no way to know
 *   which mailbox caused the tx interrupt. TP will need this feature.
 * - Sleep,wakeup not fully implemented since other modules lack functionality
 */

/* ABBREVATIONS
 *  -----------------------------------------------
 * - Can Hardware unit - One or multiple Can controllers of the same type.
 * - Hrh - HOH with receive definitions
 * - Hth - HOH with transmit definitions
 *
 */

typedef CAN_TypeDef CAN_HW_t;
//-------------------------------------------------------------------

#define GET_CONTROLLER_CONFIG(_controller)	\
        					&Can_Global.config->CanConfigSet->CanController[(_controller)]

#define GET_CALLBACKS() \
							(Can_Global.config->CanConfigSet->CanCallbacks)

#define GET_PRIVATE_DATA(_controller) \
									&CanUnit[_controller]

#define GET_CONTROLLER_CNT() (CAN_CONTROLLER_CNT)

//-------------------------------------------------------------------

#if ( CAN_DEV_ERROR_DETECT == STD_ON )
#define VALIDATE(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_CAN,0,_api,_err); \
          return CAN_NOT_OK; \
        }

#define VALIDATE_NO_RV(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_CAN,0,_api,_err); \
          return; \
        }

#define DET_REPORTERROR(_x,_y,_z,_q) Det_ReportError(_x, _y, _z, _q)
#else
#define VALIDATE(_exp,_api,_err )
#define VALIDATE_NO_RV(_exp,_api,_err )
#define DET_REPORTERROR(_x,_y,_z,_q)
#endif

#if defined(USE_DEM)
#define VALIDATE_DEM_NO_RV(_exp,_err ) \
        if( !(_exp) ) { \
          Dem_ReportErrorStatus(_err, DEM_EVENT_STATUS_FAILED); \
          return; \
        }
#else
#define VALIDATE_DEM_NO_RV(_exp,_err )
#endif

//-------------------------------------------------------------------

typedef enum
{
  CAN_UNINIT = 0,
  CAN_READY
} Can_DriverStateType;


// Mapping between HRH and Controller//HOH
typedef struct Can_Arc_ObjectHOHMapStruct
{
	CanControllerIdType CanControllerRef;    // Reference to controller
  const Can_HardwareObjectType* CanHOHRef;       // Reference to HOH.
} Can_Arc_ObjectHOHMapType;

/* Type for holding global information used by the driver */
typedef struct {
  Can_DriverStateType initRun;

  // Our config
  const Can_ConfigType *config;

  // One bit for each channel that is configured.
  // Used to determine if validity of a channel
  // 1 - configured
  // 0 - NOT configured
  uint32  configured;
  // Maps the a channel id to a configured channel id
  uint8   channelMap[CAN_CONTROLLER_CNT];

  // This is a map that maps the HTH:s with the controller and Hoh. It is built
  // during Can_Init and is used to make things faster during a transmit.
  Can_Arc_ObjectHOHMapType CanHTHMap[NUM_OF_HTHS];
} Can_GlobalType;

// Global config
Can_GlobalType Can_Global =
{
    .initRun = CAN_UNINIT,
};


/* Type for holding information about each controller */
typedef struct {
	Can_ControllerStateType state;
	uint32		lock_cnt;

	// Statistics
	Can_Arc_StatisticsType stats;

	// Data stored for Txconfirmation callbacks to CanIf
	PduIdType swPduHandle; //
} Can_UnitType;


Can_UnitType CanUnit[CAN_CONTROLLER_CNT] =
{
  {
    .state = CAN_CS_UNINIT,
  },
};



//-------------------------------------------------------------------
static CAN_HW_t * GetController(int unit)
{
#ifdef STM32F3
	/* STM32F3 only has one CAN controller */
	return (CAN_HW_t *)CAN_BASE;
#else
	// Controllers that have more than 1 CAN interface
	return ((CAN_HW_t *)(CAN1_BASE + unit*0x400));
#endif
}

//-------------------------------------------------------------------
/**
 * Function that finds the Hoh( HardwareObjectHandle ) from a Hth
 * A HTH may connect to one or several HOH's. Just find the first one.
 *
 * @param hth The transmit handle
 * @returns Ptr to the Hoh
 */
static const Can_HardwareObjectType * Can_FindHoh( Can_Arc_HTHType hth , uint32* controller)
{
  const Can_HardwareObjectType *hohObj;
  const Can_Arc_ObjectHOHMapType *map;
  const Can_ControllerConfigType *canHwConfig;

  map = &Can_Global.CanHTHMap[hth];

  // Verify that this is the correct map
  if (map->CanHOHRef->CanObjectId != hth)
  {
    DET_REPORTERROR(MODULE_ID_CAN, 0, 0x6, CAN_E_PARAM_HANDLE);
  }

  canHwConfig= GET_CONTROLLER_CONFIG(Can_Global.channelMap[map->CanControllerRef]);

  hohObj = map->CanHOHRef;

  // Verify that this is the correct Hoh type
  if ( hohObj->CanObjectType == CAN_OBJECT_TYPE_TRANSMIT)
  {
    *controller = map->CanControllerRef;
    return hohObj;
  }

  DET_REPORTERROR(MODULE_ID_CAN, 0, 0x6, CAN_E_PARAM_HANDLE);

  return NULL;
}

//-------------------------------------------------------------------

static void Can_RxIsr( int unit );
static void Can_TxIsr( int unit );
static void Can_ErrIsr( int unit );

void Can_1_RxIsr( void  ) {	Can_RxIsr(CAN_CTRL_1); }

//void Can_2_RxIsr( void  ) {	Can_RxIsr(CAN_CTRL_2); }

void Can_1_TxIsr( void  ) {	Can_TxIsr(CAN_CTRL_1); }
//void Can_2_TxIsr( void  ) {	Can_TxIsr(CAN_CTRL_2); }

void Can_1_ErrIsr( void  ) {	Can_ErrIsr(CAN_CTRL_1); }
//void Can_2_ErrIsr( void  ) {	Can_ErrIsr(CAN_CTRL_2); }


//-------------------------------------------------------------------
#if 0
// Uses 25.4.5.1 Transmission Abort Mechanism
static void Can_AbortTx( CAN_HW_t *canHw, Can_UnitType *canUnit ) {
	// Disable Transmit irq

	// check if mb's empty

	// Abort all pending mb's

	// Wait for mb's being emptied
}
#endif

/**
 * Hardware wake ISR for CAN
 *
 * @param unit CAN controller number( from 0 )
 */
static void Can_WakeIsr( int unit ) {
	if (GET_CALLBACKS()->ControllerWakeup != NULL)
	{
	  GET_CALLBACKS()->ControllerWakeup(unit);
	}
	// 269,270,271
	Can_SetControllerMode(unit, CAN_CS_STOPPED);

	// TODO EcuM_CheckWakeup();
}

/**
 * Hardware error ISR for CAN
 *
 * @param unit CAN controller number( from 0 )
 */
static void Can_ErrIsr( int unit ) {
  CAN_HW_t *canHw = GetController(unit);
  Can_UnitType *canUnit = GET_PRIVATE_DATA(unit);
  Can_Arc_ErrorType err;
  err.R = 0;

  // Check wake up
#if 0
  if(SET == IS_CAN_IT(CAN_IT_WAKEUP)) // Need to check peripheral if more than 1
  {
	  Can_WakeIsr(unit);
	  //CAN_ClearITPendingBit(canHw, CAN_IT_WAKEUP);
  }

  if(SET == IS_CAN_IT(CAN_IT_BUSOFF)) // Need to check peripheral if more than 1
  {
	canUnit->stats.boffCnt++;
	if (GET_CALLBACKS()->ControllerBusOff != NULL)
	{
	  GET_CALLBACKS()->ControllerBusOff(unit);
	}
	Can_SetControllerMode(unit, CAN_T_STOP); // CANIF272

	Can_AbortTx( canHw, canUnit ); // CANIF273

	// Clear int
	CAN_ClearITPendingBit(canHw, CAN_IT_BOF);
  }

  if (err.R != 0)
  {
    if (GET_CALLBACKS()->Arc_Error != NULL)
    {
      GET_CALLBACKS()->Arc_Error( unit, err );
    }
  }
#endif
}

//-------------------------------------------------------------------

/**
 * ISR for CAN. Normal Rx/operation
 *
 * @param unit CAN controller number( from 0 )
 */
//static void Can_RxIsr(int unit)
void USB_LP_CAN_RX0_IRQHandler(void)
{
	int unit = 0;

  CAN_HW_t *canHw= GetController(unit);
  const Can_ControllerConfigType *canHwConfig= GET_CONTROLLER_CONFIG(Can_Global.channelMap[unit]);
  Can_UnitType *canUnit = GET_PRIVATE_DATA(unit);
  const Can_HardwareObjectType *hohObj;

  //HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *hcan, uint32_t RxFifo, CAN_RxHeaderTypeDef *pHeader, uint8_t aData[]);

  CAN_RxHeaderTypeDef RxMessageHeader;
  uint8_t au8Data[8];

  RxMessageHeader.StdId=0x00;
  RxMessageHeader.ExtId=0x00;
  RxMessageHeader.IDE=0;
  RxMessageHeader.DLC=0;
  RxMessageHeader.FilterMatchIndex=0;

  au8Data[0]=0x00;
  au8Data[1]=0x00;

  HAL_CAN_GetRxMessage(canHw,CAN_RX_FIFO0, &RxMessageHeader, au8Data);

  // Loop over all the Hoh's
  hohObj= canHwConfig->Can_Arc_Hoh;
  --hohObj;
  do {
	++hohObj;

	if (hohObj->CanObjectType == CAN_OBJECT_TYPE_RECEIVE)
	{
	    Can_IdType id=0;

	    // According to autosar MSB shuould be set if extended
		if (RxMessageHeader.IDE != CAN_ID_STD) {
		  id = RxMessageHeader.ExtId;
		  id |= 0x80000000;
		} else {
		  id = RxMessageHeader.StdId;
		}

		if (GET_CALLBACKS()->RxIndication != NULL)
		{
		  GET_CALLBACKS()->RxIndication(hohObj->CanObjectId,
										id,
										RxMessageHeader.DLC,
										(uint8 *)&au8Data[0] ); // Next layer will copy
		}
		// Increment statistics
		canUnit->stats.rxSuccessCnt++;
	}
  } while ( !hohObj->Can_Arc_EOL);

  // TODO probably must be processed as well HAL_CAN_IRQHandler(&CanHandle);
}

/**
 * ISR for CAN. Normal Tx operation
 *
 * @param unit CAN controller number( from 0 )
 */
//static void Can_TxIsr(int unit) {
void USB_HP_CAN_TX_IRQHandler (void) {
	int unit = 0;

  CAN_HW_t *canHw= GetController(unit);
  const Can_ControllerConfigType *canHwConfig= GET_CONTROLLER_CONFIG(Can_Global.channelMap[unit]);
  Can_UnitType *canUnit = GET_PRIVATE_DATA(unit);
  const Can_HardwareObjectType *hohObj;

  // Loop over all the Hoh's
  hohObj= canHwConfig->Can_Arc_Hoh;
  --hohObj;
  do {
	++hohObj;

	if (hohObj->CanObjectType == CAN_OBJECT_TYPE_TRANSMIT)
	{
		if (GET_CALLBACKS()->TxConfirmation != NULL)
		{
		  GET_CALLBACKS()->TxConfirmation(canUnit->swPduHandle);
		}
		canUnit->swPduHandle = 0;  // Is this really necessary ??

		// Clear Tx interrupts
		CAN->TSR &= !(CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2);
#if 0
		// Todo transmit interrupt no idea so far
        CAN_ClearITPendingBit(canHw,CAN_IT_RQCP0);
        CAN_ClearITPendingBit(canHw,CAN_IT_RQCP1);
        CAN_ClearITPendingBit(canHw,CAN_IT_RQCP2);
#endif
	}
  } while ( !hohObj->Can_Arc_EOL);
}

//-------------------------------------------------------------------

#if 0
// Clemens should not be needed
#define INSTALL_HANDLERS(_can_name,_sce,_rx,_tx) \
  do { \
    ISR_INSTALL_ISR2( "Can_Err", _can_name ## _ErrIsr, _sce, 2, 0 ); \
	ISR_INSTALL_ISR2( "Can_Rx",  _can_name ## _RxIsr,  _rx,  2, 0 ); \
	ISR_INSTALL_ISR2( "Can_Tx",  _can_name ## _TxIsr,  _tx,  2, 0 ); \
  } while(0);
#endif

// This initiates ALL can controllers
void Can_Init( const Can_ConfigType *config ) {
  Can_UnitType *canUnit;
  const Can_ControllerConfigType *canHwConfig;
  uint32 ctlrId;

  VALIDATE_NO_RV( (Can_Global.initRun == CAN_UNINIT), 0x0, CAN_E_TRANSITION );
  VALIDATE_NO_RV( (config != NULL ), 0x0, CAN_E_PARAM_POINTER );

  // Save config
  Can_Global.config = config;
  Can_Global.initRun = CAN_READY;

  for (int configId=0; configId < CAN_ARC_CTRL_CONFIG_CNT; configId++) {
    canHwConfig = GET_CONTROLLER_CONFIG(configId);
    ctlrId = canHwConfig->CanControllerId;

    // Assign the configuration channel used later..
    Can_Global.channelMap[canHwConfig->CanControllerId] = configId;
    Can_Global.configured |= (1<<ctlrId);

    canUnit = GET_PRIVATE_DATA(ctlrId);
    canUnit->state = CAN_CS_STOPPED;

    canUnit->lock_cnt = 0;

    // Clear stats
    memset(&canUnit->stats, 0, sizeof(Can_Arc_StatisticsType));

#if 0
    // Note!
    // Could install handlers depending on HW objects to trap more errors
    // in configuration
    switch( canHwConfig->CanControllerId ) {
#ifndef STM32F10X_CL
    case CAN_CTRL_1:
        INSTALL_HANDLERS(Can_1, CAN1_SCE_IRQn, USB_LP_CAN1_RX0_IRQn, USB_HP_CAN1_TX_IRQn);	break;
#else
    case CAN_CTRL_1:
        INSTALL_HANDLERS(Can_1, CAN1_SCE_IRQn, CAN1_RX0_IRQn, CAN1_TX_IRQn);	break;
    case CAN_CTRL_2:
        INSTALL_HANDLERS(Can_2, CAN2_SCE_IRQn, CAN2_RX0_IRQn, CAN2_TX_IRQn);	break;
#endif
        default:
        assert(0);
    }
#endif

    Can_InitController(ctlrId, canHwConfig);

    // Loop through all Hoh:s and map them into the HTHMap
    const Can_HardwareObjectType* hoh;
    hoh = canHwConfig->Can_Arc_Hoh;
    hoh--;
    do
    {
      hoh++;

      if (hoh->CanObjectType == CAN_OBJECT_TYPE_TRANSMIT)
      {
        Can_Global.CanHTHMap[hoh->CanObjectId].CanControllerRef = canHwConfig->CanControllerId;
        Can_Global.CanHTHMap[hoh->CanObjectId].CanHOHRef = hoh;
      }
    } while (!hoh->Can_Arc_EOL);
  }
  return;
}

// Unitialize the module
void Can_DeInit()
{
  Can_UnitType *canUnit;
  const Can_ControllerConfigType *canHwConfig;
  uint32 ctlrId;

  for (int configId=0; configId < CAN_ARC_CTRL_CONFIG_CNT; configId++) {
    canHwConfig = GET_CONTROLLER_CONFIG(configId);
    ctlrId = canHwConfig->CanControllerId;

    canUnit = GET_PRIVATE_DATA(ctlrId);
    canUnit->state = CAN_CS_UNINIT;

    Can_DisableControllerInterrupts(ctlrId);

    canUnit->lock_cnt = 0;

    // Clear stats
    memset(&canUnit->stats, 0, sizeof(Can_Arc_StatisticsType));
  }

  Can_Global.config = NULL;
  Can_Global.initRun = CAN_UNINIT;

  return;
}

//void Can_InitController( uint8 controller, const Can_ControllerConfigType *config)
Std_ReturnType Can_SetBaudrate( uint8 controller, uint16 BaudRateConfigID)
{
  CAN_HW_t *canHw;
  uint8_t tq;
  uint8_t tqSync;
  uint8_t tq1;
  uint8_t tq2;
  uint32_t clock;
  Can_UnitType *canUnit;
  uint8 cId = controller;
  const Can_ControllerConfigType *canHwConfig;
  const Can_HardwareObjectType *hohObj;

  CAN_FilterTypeDef  sFilterConfig;


  VALIDATE_NO_RV( (Can_Global.initRun == CAN_READY), 0x2, CAN_E_UNINIT );
  VALIDATE_NO_RV( (config != NULL ), 0x2,CAN_E_PARAM_POINTER);
  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x2, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state==CANIF_CS_STOPPED), 0x2, CAN_E_TRANSITION );

  canHw = GetController(cId);
  canHwConfig = GET_CONTROLLER_CONFIG(Can_Global.channelMap[cId]);

  // Start this baby up
  HAL_CAN_DeInit(canHw);

  /* CAN filter init. We set up two filters - one for the master (CAN1) and
   * one for the slave (CAN2)
   *
   * CAN_SlaveStartBank(n) denotes which filter is the first of the slave.
   *
   * The filter registers reside in CAN1 and is shared to CAN2, so we only need
   * to set up this once.
   */

  // We let all frames in and do the filtering in software.

  /*##-2- Configure the CAN Filter ###########################################*/


  // Init filter 0 (CAN1/master)
  //CAN_FilterInitStructure.CAN_FilterNumber=0;
  //CAN_FilterInit(&CAN_FilterInitStructure);

  // Init filter 1 (CAN2/slave)
  //CAN_FilterInitStructure.CAN_FilterNumber=1;
  //CAN_FilterInit(&CAN_FilterInitStructure);

  // Set which filter to use for CAN2.
  //CAN_SlaveStartBank(1);

  // acceptance filters
   hohObj = canHwConfig->Can_Arc_Hoh;
   --hohObj;
   do {
	 ++hohObj;
     if (hohObj->CanObjectType == CAN_OBJECT_TYPE_RECEIVE)
     {
    	 // TODO Hw filtering
     }
   }while( !hohObj->Can_Arc_EOL );

  // Clock calucation
  // -------------------------------------------------------------------
  //
  // * 1 TQ = Sclk period( also called SCK )
  // * Ftq = Fcanclk / ( PRESDIV + 1 ) = Sclk
  //   ( Fcanclk can come from crystal or from the peripheral dividers )
  //
  // -->
  // TQ = 1/Ftq = (PRESDIV+1)/Fcanclk --> PRESDIV = (TQ * Fcanclk - 1 )
  // TQ is between 8 and 25
  clock = 10000; //McuE_GetSystemClock()/2;

  tqSync = canHwConfig->CanControllerPropSeg + 1;
  tq1 = canHwConfig->CanControllerSeg1 + 1;
  tq2 = canHwConfig->CanControllerSeg2 + 1;
  tq = tqSync + tq1 + tq2;

  CAN_HandleTypeDef        CanHandle;
  //CAN_StructInit(&CAN_InitStructure);

  CanHandle.Init.TimeTriggeredMode = DISABLE;
  CanHandle.Init.AutoBusOff = DISABLE;
  CanHandle.Init.AutoWakeUp = DISABLE;
  CanHandle.Init.AutoRetransmission = ENABLE;
  CanHandle.Init.ReceiveFifoLocked = DISABLE;
  CanHandle.Init.TransmitFifoPriority = DISABLE;

  CanHandle.Init.SyncJumpWidth = CAN_SJW_1TQ; //hard coded to value 1 since no configuration is available
  CanHandle.Init.TimeSeg1 = CAN_BS1_9TQ;
  CanHandle.Init.TimeSeg2 = CAN_BS2_8TQ;
  CanHandle.Init.Prescaler = clock/(canHwConfig->CanControllerBaudRate*1000*tq); // 2

  /*   /* CAN cell init
  CAN_InitStructure.CAN_TTCM=DISABLE;
  CAN_InitStructure.CAN_ABOM=ENABLE;
  CAN_InitStructure.CAN_AWUM=ENABLE;
  CAN_InitStructure.CAN_NART=DISABLE;
  CAN_InitStructure.CAN_RFLM=DISABLE;
  CAN_InitStructure.CAN_TXFP=DISABLE;
   */

  if(canHwConfig->Can_Arc_Loopback)
  {
	  CanHandle.Init.Mode = CAN_MODE_LOOPBACK;
  }
  else
  {
	  CanHandle.Init.Mode = CAN_MODE_NORMAL;
  }

  if (HAL_CAN_Init(&CanHandle) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
    return E_NOT_OK;
  }

  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  CanHandle.Instance = canHw;

  if (HAL_CAN_ConfigFilter(&CanHandle, &sFilterConfig) != HAL_OK)
  {
	return E_NOT_OK;
  }

  canUnit->state = CAN_CS_STOPPED;
  Can_EnableControllerInterrupts(cId);
  return E_OK;
}


Std_ReturnType Can_SetControllerMode( uint8 controller, Can_ControllerStateType transition )
{
  //imask_t state;
  CAN_HW_t *canHw;
  Can_ReturnType rv = CAN_OK;
  VALIDATE( (controller < GET_CONTROLLER_CNT()), 0x3, CAN_E_PARAM_CONTROLLER );

  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE( (canUnit->state!=CANIF_CS_UNINIT), 0x3, CAN_E_UNINIT );
  canHw = GetController(controller);

  switch(transition )
  {
  case CAN_CS_STARTED:
    canUnit->state = CAN_CS_STARTED;
    //Irq_Save(state);
    if (canUnit->lock_cnt == 0){   // REQ CAN196
      Can_EnableControllerInterrupts(controller);
    }
    //Irq_Restore(state);
    break;
#if 0
  case CAN_CS_WAKEUP:
	VALIDATE(canUnit->state == CANIF_CS_SLEEP, 0x3, CAN_E_TRANSITION);
	HAL_CAN_WakeUp(canHw);
	canUnit->state = CANIF_CS_STOPPED;
	break;
#endif
  case CAN_CS_SLEEP:  //CAN258, CAN290
    // Should be reported to DEM but DET is the next best
    VALIDATE(canUnit->state == CANIF_CS_STOPPED, 0x3, CAN_E_TRANSITION);
    HAL_CAN_RequestSleep(canHw);
    canUnit->state = CAN_CS_SLEEP;
	break;
  case CAN_CS_STOPPED:

    // Stop
    canUnit->state = CAN_CS_STOPPED;
    //Can_AbortTx( canHw, canUnit ); // CANIF282
    break;
  default:
    // Should be reported to DEM but DET is the next best
    VALIDATE(canUnit->state == CANIF_CS_STOPPED, 0x3, CAN_E_TRANSITION);
    break;
  }

  return rv;
}

void Can_DisableControllerInterrupts( uint8 controller )
{
  //imask_t state;
  Can_UnitType *canUnit;
  CAN_HW_t *canHw;

  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x4, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state!=CANIF_CS_UNINIT), 0x4, CAN_E_UNINIT );

  //Irq_Save(state);
  if(canUnit->lock_cnt > 0 )
  {
    // Interrupts already disabled
    canUnit->lock_cnt++;
    //Irq_Restore(state);
    return;
  }
  canUnit->lock_cnt++;
  //Irq_Restore(state);

  /* Don't try to be intelligent, turn everything off */
  canHw = GetController(controller);

#if 0
  /* Turn off the tx interrupt mailboxes */
  CAN_ITConfig(canHw, CAN_IT_TME, DISABLE);


  /* Turn off the bus off/tx warning/rx warning and error and rx  */
  CAN_ITConfig(canHw, CAN_IT_FMP0 | CAN_IT_BOF | CAN_IT_ERR | CAN_IT_WKU, DISABLE);
#endif
}

void Can_EnableControllerInterrupts( uint8 controller ) {
  //imask_t state;
  Can_UnitType *canUnit;
  CAN_HW_t *canHw;
  const Can_ControllerConfigType *canHwConfig;
  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x5, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state!=CANIF_CS_UNINIT), 0x5, CAN_E_UNINIT );

  //Irq_Save(state);
  if( canUnit->lock_cnt > 1 )
  {
    // IRQ should still be disabled so just decrement counter
    canUnit->lock_cnt--;
    //Irq_Restore(state);
    return;
  } else if (canUnit->lock_cnt == 1)
  {
    canUnit->lock_cnt = 0;
  }
  //Irq_Restore(state);

  canHw = GetController(controller);

  canHwConfig = GET_CONTROLLER_CONFIG(Can_Global.channelMap[controller]);

  if( canHwConfig->CanRxProcessing == CAN_ARC_PROCESS_TYPE_INTERRUPT ) {
    /* Turn on the rx interrupt */
#if 0
	CAN_ITConfig(canHw, CAN_IT_FMP0, ENABLE);
#endif
  }
  if( canHwConfig->CanTxProcessing == CAN_ARC_PROCESS_TYPE_INTERRUPT ) {
	/* Turn on the tx interrupt mailboxes */
#if 0
  	CAN_ITConfig(canHw, CAN_IT_TME, ENABLE);
#endif
  }

  // BusOff here represents all errors and warnings
  if( canHwConfig->CanBusOffProcessing == CAN_ARC_PROCESS_TYPE_INTERRUPT ) {
	/* Turn on the bus off/tx warning/rx warning and error and rx  */
#if 0
	CAN_ITConfig(canHw, CAN_IT_BOF | CAN_IT_ERR | CAN_IT_WKU, ENABLE);

#endif
  }

  return;
}

Can_ReturnType Can_Write( Can_Arc_HTHType hth, Can_PduType *pduInfo ) {
  Can_ReturnType rv = CAN_OK;
  CAN_HW_t *canHw;
  const Can_HardwareObjectType *hohObj;
  const Can_ControllerConfigType *canHwConfig;
  uint32 controller;
  //imask_t state;

  VALIDATE( (Can_Global.initRun == CAN_READY), 0x6, CAN_E_UNINIT );
  VALIDATE( (pduInfo != NULL), 0x6, CAN_E_PARAM_POINTER );
  VALIDATE( (pduInfo->length <= 8), 0x6, CAN_E_PARAM_DLC );
  VALIDATE( (hth < NUM_OF_HTHS ), 0x6, CAN_E_PARAM_HANDLE );

  hohObj = Can_FindHoh(hth, &controller);
  if (hohObj == NULL)
    return CAN_NOT_OK;

  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);

  canHw = GetController(controller);
  //Irq_Save(state);

  //CanTxMsg TxMessage;
  CAN_TxHeaderTypeDef   TxMessageHeader;
  uint8_t               TxData[8];
  uint32_t              TxMailbox;

  TxMessageHeader.RTR=CAN_RTR_DATA;
  TxMessageHeader.DLC=pduInfo->length;

  memcpy(TxData, pduInfo->sdu, pduInfo->length);

  if (hohObj->CanIdType == CAN_ID_TYPE_EXTENDED) {
	  TxMessageHeader.IDE=CAN_ID_EXT;
	  TxMessageHeader.ExtId=pduInfo->id;
  } else {
	  TxMessageHeader.IDE=CAN_ID_STD;
	  TxMessageHeader.StdId=pduInfo->id;
  }

  // check for any free box
  if(HAL_CAN_AddTxMessage(canHw,&TxMessageHeader, TxData, &TxMailbox) != HAL_OK) //CAN_NO_MB) {
  {
    canHwConfig = GET_CONTROLLER_CONFIG(Can_Global.channelMap[controller]);

    if( canHwConfig->CanTxProcessing == CAN_ARC_PROCESS_TYPE_INTERRUPT ) {
  	  /* Turn on the tx interrupt mailboxes */
    	//CAN_ITConfig(canHw,CAN_IT_TME, ENABLE);
    }

	// Increment statistics
	canUnit->stats.txSuccessCnt++;

    // Store pdu handle in unit to be used by TxConfirmation
    canUnit->swPduHandle = pduInfo->swPduHandle;
  } else {
    rv = CAN_BUSY;
  }
  //Irq_Restore(state);

  return rv;
}

void Can_MainFunction_Read( void ) {

	/* NOT SUPPORTED */
}

void Can_MainFunction_BusOff( void ) {
  /* Bus-off polling events */

	/* NOT SUPPORTED */
}

void Can_MainFunction_Wakeup( void ) {
  /* Wakeup polling events */

	/* NOT SUPPORTED */
}


/**
 * Get send/receive/error statistics for a controller
 *
 * @param controller The controller
 * @param stats Pointer to data to copy statistics to
 */

void Can_Arc_GetStatistics( uint8 controller, Can_Arc_StatisticsType *stats)
{
  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);
  *stats = canUnit->stats;
}
