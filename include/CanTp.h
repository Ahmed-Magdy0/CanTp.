#ifndef CanTp_H_
#define CanTp_H_


#include "CanTp_Types.h"
#include "CanTpRuntime.h"



void initTx15765RuntimeData(CanTp_ChannelPrivateType *txRuntimeParams);
void initRx15765RuntimeData(CanTp_ChannelPrivateType *rxRuntimeParams);



/********************************************** Prototypes Of Functions used ********************************************************/


/* This Function initate the global parameters of the CanTp Module and move the state to CanTp_ON if there is No Error */
void CanTp_Init( const CanTp_ConfigType* CfgPtr );


/* This Function moves the state to CanTp_OFF */
void CanTp_Shutdown( void );

Std_ReturnType CanTp_Transmit( PduIdType TxPduId, const PduInfoType* PduInfoPtr );

Std_ReturnType CanTp_CancelTransmit( PduIdType TxPduId );  // this one wasn't in the refrence file of the company

Std_ReturnType CanTp_CancelReceive( PduIdType RxPduId ); // also this one

Std_ReturnType CanTp_ChangeParameter( PduIdType id, TPParameterType parameter, uint16 value ); // and this

Std_ReturnType CanTp_ReadParameter( PduIdType id, TPParameterType parameter, uint16* value );



void CanTp_MainFunction( void );


void CanTp_TxConfirmation(PduIdType CanTpTxPduId ,Std_ReturnType result );


#endif
