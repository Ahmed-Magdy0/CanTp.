/////*
//// ============================================================================
//// Name        : cantp.c
//// Author      : CAN_TP_Team
//// Version     :
//// Copyright   : Your copyright notice
//// Description : The CanTp Main Task is Segmentation and reassembling
//// ============================================================================
//// */


#include "include/CanTpRuntime.h"
#include "include/Helper_Functions.h"
#include <stdio.h>
#include <stdlib.h>
#include "include/CanTp_Types.h"
#include "include/CanTp_Cfg.h"
#include "include/CanTp.h"
#include "include/CanTp_Cbk.h"
#include "include/SchM_CanTp.h"
#include "include/MemMap.h"
//#include "include/Det.h"


extern CanTp_RunTimeDataType CanTpRunTimeData;
extern CanTp_ConfigType CanTp_Config;

void initTx15765RuntimeData(CanTp_ChannelPrivateType *txRuntimeParams)
{
	
    txRuntimeParams->iso15765.state = IDLE;
    txRuntimeParams->iso15765.NasNarPending = FALSE;
    txRuntimeParams->iso15765.framesHandledCount = 0;
    txRuntimeParams->iso15765.nextFlowControlCount = 0;
    txRuntimeParams->pdurBufferCount = 0;
    txRuntimeParams->transferTotal = 0;
    txRuntimeParams->transferCount = 0;
    txRuntimeParams->mode = CANTP_TX_WAIT;
    txRuntimeParams->pdurBuffer.SduDataPtr = NULL;
}

void initRx15765RuntimeData(CanTp_ChannelPrivateType *rxRuntimeParams)
{

    rxRuntimeParams->iso15765.state = IDLE;
    rxRuntimeParams->iso15765.NasNarPending = FALSE;
    rxRuntimeParams->iso15765.framesHandledCount = 0;
    rxRuntimeParams->iso15765.nextFlowControlCount = 0;
    rxRuntimeParams->pdurBufferCount = 0;
    rxRuntimeParams->transferTotal = 0;
    rxRuntimeParams->transferCount = 0;
    rxRuntimeParams->mode = CANTP_RX_WAIT;
    rxRuntimeParams->pdurBuffer.SduDataPtr = NULL;
    rxRuntimeParams->Buffersize = 0;
}


/******************************************************************************************************************
	Service name		: CanTp_RxIndication
	Syntax				: void CanTp_RxIndication(     PduIdType RxPduId,     const PduInfoType* PduInfoPtr )
	Service ID[hex]		: 0x42
	Sync/Async			: Synchronous
	Reentrancy			: Reentrant for different PduIds. Non reentrant for the same PduId.
	Parameters (in)		: RxPduId    ID of the received PDU.
						  PduInfoPtr Contains the length (SduLength) of the received PDU, 
						  a pointer to a buffer (SduDataPtr)
				          containing the PDU, and the MetaData related to this PDU. 
	Parameters(inout)	: None
	Parameters (out)	: None
    Return value		: None
    Description 		: Indication of a received PDU from a lower layer communication interface module.
******************************************************************************************************************/	 
void CanTp_RxIndication(PduIdType CanTpRxPduId,const PduInfoType *CanTpRxPduPtr)
{

	const CanTp_RxNSduType *rxConfigParams;			 // Parameters reside in ROM.
	const CanTp_TxNSduType *txConfigParams;

	CanTp_AddressingFormatType addressingFormat; 	 // Configured
	CanTp_ChannelPrivateType *runtimeParams = NULL;  // Parameters reside in RAM.
	ISO15765FrameType frameType;




	addressingFormat = CanTp_Config.CanTpChannel.CanTpRxNSdu[CanTpRxPduId].CanTpRxAddressingFormat;

	frameType = getFrameType(&addressingFormat, CanTpRxPduPtr);




				/* in case of tx */

	if( frameType == FLOW_CONTROL_CTS_FRAME )
	{
		if (CanTpRxPduId != 0xFFFF)
			{
				txConfigParams = &CanTp_Config.CanTpChannel.CanTpTxNSdu[CanTpRxPduId];
				runtimeParams = &CanTpRunTimeData.runtimeDataList[txConfigParams->CanTpTxChannel];
			}
		else
			{
				//Invalid FC received
				return;
			}

		rxConfigParams = NULL;
	}

		/* in case of rx */

	else   	 /*{SF,FF,CF}*/
	{
		if( CanTpRxPduId != 0xFFFF )
		{
			rxConfigParams = &CanTp_Config.CanTpChannel.CanTpRxNSdu[CanTpRxPduId];
			runtimeParams = &CanTpRunTimeData.runtimeDataList[rxConfigParams->CanTpRxChannel];	
		}
		else
		{
			//Invalid Frame received
			return;
		}
		txConfigParams = NULL;
	}



	switch (frameType)
	{
	case SINGLE_FRAME:
	{
		if (rxConfigParams != NULL)
		{
			handleSingleFrame(rxConfigParams, runtimeParams, CanTpRxPduPtr);
		}

		break;
	}

	case FIRST_FRAME:
	{
		if (rxConfigParams != NULL)
		{
			handleFirstFrame(rxConfigParams, runtimeParams, CanTpRxPduPtr);
		}
		break;
	}

	case CONSECUTIVE_FRAME:
	{
		if (rxConfigParams != NULL)
		{
			handleConsecutiveFrame(rxConfigParams, runtimeParams, CanTpRxPduPtr);
		}
		break;
	}

	case FLOW_CONTROL_CTS_FRAME:
	{
		if (txConfigParams != NULL)
		{
			handleFlowControlFrame(txConfigParams, runtimeParams, CanTpRxPduPtr);
		}
		break;
	}
	case INVALID_FRAME:
	{
		break;
	}

	default:
		break;
	}
}


/*****************************************************************************************************************
*	Service name		: CanTp_Init
*	Syntax				: void CanTp_Init(const CanTp_ConfigType* CfgPtr )
*	Service ID[hex]		: 0x01
*	Sync/Async			: Synchronous 
*	Reentrancy			: Non Reentrant 
*	Parameters (in)		: (CfgPtr) Pointer to the CanTp post-build configuration data.
*	Parameters (inout)	: None	 
*	Parameters (out)	: None 
*	Return value		: None
*	Description			: This function initializes the CanTp module. 
*****************************************************************************************************************/

///* This Function initate the global parameters of the CanTp Module and move the state to CanTp_ON if there is No Error *///
void CanTp_Init( const CanTp_ConfigType* CfgPtr )
{

		CanTp_ChannelPrivateType *runtimeData;


		 	 	 	 	 	 	 	 	 	 	 	   /* For TX */
		uint8 i;
		for (i=0; i < CANTP_NSDU_CONFIG_LIST_SIZE_TX; i++)
			{
					/* this if handle if CanTpTxChannel < Runtime_list_size
					 *then access the  CanTpTxChannel element in the runtimeDataList
					 * else access the last item in the runtimeDataList
					 */
							if ((uint8)CfgPtr->CanTpChannel.CanTpTxNSdu[i].CanTpTxChannel < CANTP_NSDU_RUNTIME_LIST_SIZE)
							{
								runtimeData = &CanTpRunTimeData.runtimeDataList[CfgPtr->CanTpChannel.CanTpTxNSdu[i].CanTpTxChannel];
							}
							 else
							{
								runtimeData = &CanTpRunTimeData.runtimeDataList[CANTP_NSDU_RUNTIME_LIST_SIZE-1];
							}

							initTx15765RuntimeData( runtimeData );
			}

														/* For RX */
		for (i=0; i < CANTP_NSDU_CONFIG_LIST_SIZE_RX; i++)
		{

			/* this if handle if CanTpRxChannel < Runtime_list_size
			 *then access the  CanTpTxChannel element in the runtimeDataList
			 * else access the last item in the runtimeDataList
			 */
					if (CfgPtr->CanTpChannel.CanTpRxNSdu[i].CanTpRxChannel < CANTP_NSDU_RUNTIME_LIST_SIZE)
					{
						runtimeData = &CanTpRunTimeData.runtimeDataList[CfgPtr->CanTpChannel.CanTpRxNSdu[i].CanTpRxChannel];
					}
					else
					{
						runtimeData = &CanTpRunTimeData.runtimeDataList[CANTP_NSDU_RUNTIME_LIST_SIZE-1];
					}
					initRx15765RuntimeData( runtimeData );
		}


		CanTpRunTimeData.internalState = CANTP_ON;    /* if the initfunc finished correcltly without errors ,then move into CANTP_ON state */

}

/****************************************************************************************************************
*	Service name		: CanTp_Transmit
*	Syntax				: Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
*	Service ID[hex]		: 0x49
*	Sync/Async			: Synchronous
*	Reentrancy			: Reentrant for different PduIds. Non reentrant for the same PduId.
*	Parameters (in)		: TxPduId Identifier of the PDU to be transmitted.
*				    		PduInfoPtr Length of and pointer to the PDU data and pointer to MetaData.
*	Parameters (inout)	: None
*	Parameters (out)	: None
*	Return value		: Std_ReturnType    E_OK: Transmit request has been accepted.
*											E_NOT_OK: Transmit request has not been accepted.
*	Description			: Requests transmission of a PDU.
*****************************************************************************************************************/

/* this function used to copy the Data Length of the data required to be sent from  PDURBuffer to the CanIF Buffer in FF or SF */
Std_ReturnType CanTp_Transmit( PduIdType TxPduId, const PduInfoType* PduInfoPtr )
{
	const CanTp_TxNSduType *txConfig = NULL;
	CanTp_ChannelPrivateType *txRuntime = NULL;
	Std_ReturnType ret = 0;



	/*Here we should make a fuction get the Txid and return the correct index in the cfg.c*/

		txConfig	=  &CanTp_Config.CanTpChannel.CanTpTxNSdu[TxPduId];        		             // id = index for simplicity





		txRuntime = &CanTpRunTimeData.runtimeDataList[txConfig->CanTpTxChannel]; 				 // Runtime data.

		if (txRuntime->iso15765.state == IDLE)
		{

			ISO15765FrameType iso15765Frame;

			txRuntime->canFrameBuffer.byteCount = 0;
			txRuntime->transferCount = 0;
			txRuntime->iso15765.framesHandledCount = 0;
			txRuntime->transferTotal = PduInfoPtr->SduLength;    	 /* copy SduLength to transferTotal */
            
			txRuntime->iso15765.stateTimeoutCount = (txConfig->CanTpNcs);	// Value in seconds of the performance requirement of (N_Cs + N_As). N_Cs is the time in which CanTp is allowed to request the Tx data of a Consecutive Frame N_PDU.
			txRuntime->mode = CANTP_TX_PROCESSING;


			iso15765Frame = calcRequiredProtocolFrameType(txConfig, txRuntime);  // To identify the Frame

			if (txConfig->CanTpTxAddressingFormat == CANTP_EXTENDED)
			{
				txRuntime->canFrameBuffer.data[txRuntime->canFrameBuffer.byteCount++] =
					 txConfig->CanTpNTa.CanTpNTa;					// putting CanTpNTa in the first byte of data in case of extended 		 // Target address.
			}

			switch(iso15765Frame)
			{
			/* This only put the Data length in the correct position in pci */

			/*TODO: We Will support id Can_DL > 8 */
			case SINGLE_FRAME:
				txRuntime->canFrameBuffer.data[txRuntime->canFrameBuffer.byteCount++] =
						ISO15765_TPCI_SF | (uint8)(txRuntime->transferTotal);   /* Can_DL < 8 so it will represent in 8 bits ( putting the SduLength at byte 0 )*/

				txRuntime->pdurBuffer.SduLength = txRuntime->transferTotal ;
				ret = E_OK;
				txRuntime->iso15765.state = TX_WAIT_TRANSMIT;   		/* Activate a TP Task */
				break;




			case FIRST_FRAME:
				txRuntime->canFrameBuffer.data[txRuntime->canFrameBuffer.byteCount++] =
						ISO15765_TPCI_FF | (uint8)((txRuntime->transferTotal & 0xf00) >> 8);  /* putting the  Byte 1  */


				txRuntime->canFrameBuffer.data[txRuntime->canFrameBuffer.byteCount++] =
						(uint8)(txRuntime->transferTotal & 0xff);						      /* putting the  Byte 2  */

				if (txConfig->CanTpTxAddressingFormat == CANTP_STANDARD)
				{
					txRuntime->pdurBuffer.SduLength = MAX_PAYLOAD_FF_STD_ADDR;                  // in case of standard
				}
				else
				{
					txRuntime->pdurBuffer.SduLength = MAX_PAYLOAD_FF_EXT_ADDR;                  // in case of Extended
				}


				// setup block size so that state machine waits for flow control after first frame
				txRuntime->iso15765.nextFlowControlCount = 1;
				txRuntime->iso15765.BS = 1;						/* setting the block size and we can also set STmin  */
				ret = E_OK;
				txRuntime->iso15765.state = TX_WAIT_TRANSMIT;			/* Activate a TP Task */
				break;

			default:
				ret = E_NOT_OK;
			}
		}
		else							/* if state not idle make  ret = E_NOT_OK */
		{
			ret = E_NOT_OK;
		}

	return ret; 		// CAN level error code.
}
/*****************************************************************************************************************
*	Service name		: CanTp_Shutdown
*   Syntax				: void CanTp_Shutdown(void)
*	Service ID[hex]		: 0x02
*   Sync/Async			: Synchronous
*   Reentrancy			: Non Reentrant
*	Parameters (in)		: None
*	Parameters (inout)	: None
*	Parameters (out)	: None
*   Return value		: None
*   Description			: This function is called to shutdown the CanTp module.
******************************************************************************************************************/

/* This Function move the state to CanTp_OFF*/
void CanTp_Shutdown(void)
{

	CanTpRunTimeData.internalState = CANTP_OFF;

}
/*****************************************************************************************************************
*	Service name	: CanTp_MainFunction
*   Syntax			: void CanTp_MainFunction(void)
*	Service ID[hex]	: 0x06 
*   Description		: The main function for scheduling the CAN TP.
******************************************************************************************************************/

void CanTp_MainFunction(void)
{
	BufReq_ReturnType ret;         		   


	CanTp_ChannelPrivateType *txRuntimeListItem = NULL;
	CanTp_ChannelPrivateType *rxRuntimeListItem = NULL;

	const CanTp_TxNSduType *txConfigListItem = NULL;
	const CanTp_RxNSduType *rxConfigListItem = NULL;


	uint8 i = 0;
	for( i=0; i < CANTP_NSDU_CONFIG_LIST_SIZE_TX; i++ )
	{
				/* in case of TX */


			txConfigListItem = (CanTp_TxNSduType*)&CanTp_Config.CanTpChannel.CanTpTxNSdu[i];
			txRuntimeListItem = &CanTpRunTimeData.runtimeDataList[txConfigListItem->CanTpTxChannel];

			switch (txRuntimeListItem->iso15765.state)
			{

			case TX_WAIT_STMIN:
				{
					TIMER_DECREMENT(txRuntimeListItem->iso15765.stateTimeoutCount); 		// Make sure that STmin timer has expired.
					if (txRuntimeListItem->iso15765.stateTimeoutCount != 0)
					{
						break;
					}

					txRuntimeListItem->iso15765.state = TX_WAIT_TRANSMIT;            /* when stateTimeoutCount = 0 */
				}

			case TX_WAIT_TRANSMIT:
				{
					ret = sendNextTxFrame(txConfigListItem, txRuntimeListItem);

					if ( ret == BUFREQ_OK )
					{
						// successfully sent frame.
					}

					else if( ret == BUFREQ_E_BUSY )
					{
						// check N_Cs timeout
						TIMER_DECREMENT(txRuntimeListItem->iso15765.stateTimeoutCount);
						if (txRuntimeListItem->iso15765.stateTimeoutCount == 0)
						{
							txRuntimeListItem->iso15765.state = IDLE;
							txRuntimeListItem->mode = CANTP_TX_WAIT;
							    			 
						}
						else
						{
							/* Misra Rule*/
						}
					}

					else
					{
						txRuntimeListItem->iso15765.state = IDLE;
						txRuntimeListItem->mode = CANTP_TX_WAIT;
						 				 // it is not important in the core Now
					}
					break;
				}

			case TX_WAIT_FLOW_CONTROL:
				{
					TIMER_DECREMENT(txRuntimeListItem->iso15765.stateTimeoutCount);
					if (txRuntimeListItem->iso15765.stateTimeoutCount == 0)
					{
						txRuntimeListItem->iso15765.state = IDLE;
						txRuntimeListItem->mode = CANTP_TX_WAIT;
						 			    
					}
					break;
				}
			case TX_WAIT_TX_CONFIRMATION:
				{
					TIMER_DECREMENT(txRuntimeListItem->iso15765.stateTimeoutCount);

					if (txRuntimeListItem->iso15765.stateTimeoutCount == 0)
					{
						txRuntimeListItem->iso15765.state = IDLE;
						txRuntimeListItem->mode = CANTP_TX_WAIT;
						   
					}
					break;

				default:
					break;
				}

			}
	}




			/* in case of RX */

	for( i=0; i < CANTP_NSDU_CONFIG_LIST_SIZE_RX; i++ )
		{
			rxConfigListItem =(CanTp_RxNSduType*)&CanTp_Config.CanTpChannel.CanTpRxNSdu[i];
			rxRuntimeListItem = &CanTpRunTimeData.runtimeDataList [ rxConfigListItem->CanTpRxChannel ];

			switch (rxRuntimeListItem->iso15765.state)
			{
				case RX_WAIT_CONSECUTIVE_FRAME:
					{
						TIMER_DECREMENT (rxRuntimeListItem->iso15765.stateTimeoutCount);

						if (rxRuntimeListItem->iso15765.stateTimeoutCount == 0)
						{
							
							rxRuntimeListItem->iso15765.state = IDLE;
							rxRuntimeListItem->mode = CANTP_RX_WAIT;
						}
						break;
					}           `                   ``      `1`     `

				case RX_WAIT_SDU_BUFFER:
					{
						TIMER_DECREMENT (rxRuntimeListItem->iso15765.stateTimeoutCount);
						/* We end up here if we have requested a buffer from the
						 * PDUR but the response have been BUSY. We assume
						 * we have data in our local buffer and we are expected
						 * to send a flow-control clear to send (CTS).
						 */
						if (rxRuntimeListItem->iso15765.stateTimeoutCount == 0)
						{
							
							rxRuntimeListItem->iso15765.state = IDLE;
							rxRuntimeListItem->mode = CANTP_RX_WAIT;
						}


						else			//	stateTimeoutCount != 0
						{
							PduLengthType bytesRemaining = 0;

							 /* copies from local buffer to PDUR buffer. */
							ret = copySegmentToPduRRxBuffer(rxConfigListItem,rxRuntimeListItem,rxRuntimeListItem->canFrameBuffer.data);

							bytesRemaining = rxRuntimeListItem->transferTotal -  rxRuntimeListItem->transferCount;
							if (bytesRemaining > 0)
							{
								sendFlowControlFrame( rxConfigListItem, rxRuntimeListItem, ret ); 			/* (Busy or CTS) */
							}

							if (ret == BUFREQ_OK)
							{
								if ( bytesRemaining > 0 )
								{
									rxRuntimeListItem->iso15765.stateTimeoutCount = (rxConfigListItem->CanTpNcr);  //UH
									rxRuntimeListItem->iso15765.state = RX_WAIT_CONSECUTIVE_FRAME;
								}
								else
								{
												  /* CanTp_00214 */
									rxRuntimeListItem->iso15765.state = IDLE;
									rxRuntimeListItem->mode = CANTP_RX_WAIT;
								}
							}
							else if (ret == BUFREQ_E_NOT_OK )
							{
								       /* CanTp_00214 */
								rxRuntimeListItem->iso15765.state = IDLE;
								rxRuntimeListItem->mode = CANTP_RX_WAIT;
							}
							else if ( ret == BUFREQ_E_BUSY )
							{

							}
						}
						break;
					}
				default:
					break;
			}  // end of switch case

		} // end of for loop
}


void CanTp_TxConfirmation(PduIdType CanTpTxPduId ,Std_ReturnType result )
{
	const CanTp_RxNSduType *rxConfigParams = NULL;
	const CanTp_TxNSduType *txConfigParams = NULL;


			/* in case of TX */

			txConfigParams = &CanTp_Config.CanTpChannel.CanTpTxNSdu[CanTpTxPduId];
			CanTp_ChannelPrivateType *txRuntime = &CanTpRunTimeData.runtimeDataList[txConfigParams->CanTpTxChannel];
			if(txRuntime->iso15765.state == TX_WAIT_TX_CONFIRMATION)
			{
				handleNextTxFrameSent(txConfigParams, txRuntime);
			}


}
