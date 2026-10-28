/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006 Christian Walter <wolti@sil.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * File: $Id: mbrtu.c,v 1.18 2007/09/12 10:15:56 wolti Exp $
 */

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"
/* STM32 HAL replaced by Zephyr */
/* ----------------------- Platform includes --------------------------------*/
#include "port.h"
/* leddef.h not needed in Zephyr port */
/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbrtu.h"
#include "mbframe.h"

#include "mbcrc.h"
#include "mbport.h"

/* CMSIS-OS replaced by Zephyr; RS485 sem handled by port layer */ 

/* ----------------------- Defines ------------------------------------------*/
#define MB_SER_PDU_SIZE_MIN     4       /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_MAX     256     /*!< Maximum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_CRC     2       /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF     0       /*!< Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF      1       /*!< Offset of Modbus-PDU in Ser-PDU. */

/* ----------------------- Type definitions ---------------------------------*/
typedef enum
{
    STATE_RX_INIT,              /*!< Receiver is in initial state. */
    STATE_RX_IDLE,              /*!< Receiver is in idle state. */
    STATE_RX_RCV,               /*!< Frame is beeing received. */
    STATE_RX_ERROR              /*!< If the frame is invalid. */
} eMBRcvState;

typedef enum
{
    STATE_TX_IDLE,              /*!< Transmitter is in idle state. */
    STATE_TX_XMIT               /*!< Transmitter is in transfer state. */
} eMBSndState;

/* ----------------------- Static variables ---------------------------------*/
volatile eMBSndState eSndState;
volatile eMBRcvState eRcvState;

volatile UCHAR  ucRTUInBuf[MB_SER_PDU_SIZE_MAX];
volatile UCHAR  ucRTUOutBuf[MB_SER_PDU_SIZE_MAX];

volatile UCHAR *pucSndBufferCur;
volatile USHORT usSndBufferCount;

static volatile USHORT usRcvBufferPos;
volatile eMBSndState eSndState3;
volatile eMBRcvState eRcvState3;

volatile UCHAR  ucRTUInBuf3[MB_SER_PDU_SIZE_MAX];
volatile UCHAR  ucRTUOutBuf3[MB_SER_PDU_SIZE_MAX];

volatile UCHAR *pucSndBufferCur3;
volatile USHORT usSndBufferCount3;

static volatile USHORT usRcvBufferPos3;

volatile eMBSndState eSndState6;
volatile eMBRcvState eRcvState6;

volatile UCHAR  ucRTUInBuf6[MB_SER_PDU_SIZE_MAX];
volatile UCHAR  ucRTUOutBuf6[MB_SER_PDU_SIZE_MAX];

volatile UCHAR *pucSndBufferCur6;
volatile USHORT usSndBufferCount6;

static volatile USHORT usRcvBufferPos6;

/* ----------------------- Start implementation -----------------------------*/
eMBErrorCode
eMBRTUInit( UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    ULONG           usTimerT35_50us;

    ( void )ucSlaveAddress;
    ENTER_CRITICAL_SECTION(  );

    /* Modbus RTU uses 8 Databits. */
    if( xMBPortSerialInit( ucPort, ulBaudRate, 8, eParity ) != TRUE )
    {
        eStatus = MB_EPORTERR;
    }
    else
    {
        /* If baudrate > 19200 then we should use the fixed timer values
         * t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
         */
        if( ulBaudRate > 19200 )
        {
            usTimerT35_50us = 35;       /* 1800us. */
        }
        else
        {
            /* The timer reload value for a character is given by:
             *
             * ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
             *             = 11 * Ticks_per_1s / Baudrate
             *             = 220000 / Baudrate
             * The reload for t3.5 is 1.5 times this value and similary
             * for t3.5.
             */
            usTimerT35_50us = ( 7UL * 2200000UL ) / ( 2UL * ulBaudRate );
        }
        if( xMBPortTimersInit( ( USHORT ) usTimerT35_50us ) != TRUE )
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

eMBErrorCode
eMBRTUInit3( UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity )
{
    eMBErrorCode eStatus = MB_ENOERR;
    ULONG usTimerT35_50us;

    ( void )ucSlaveAddress;
    ENTER_CRITICAL_SECTION(  );

    if( xMBPortSerialInit3( ucPort, ulBaudRate, 8, eParity ) != TRUE )
    {
        eStatus = MB_EPORTERR;
    }
    else
    {
        if( ulBaudRate > 19200 )
        {
            usTimerT35_50us = 35;
        }
        else
        {
            usTimerT35_50us = ( 7UL * 2200000UL ) / ( 2UL * ulBaudRate );
        }
        if( xMBPortTimersInit3( ( USHORT ) usTimerT35_50us ) != TRUE )
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

eMBErrorCode
eMBRTUInit6( UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity )
{
    eMBErrorCode eStatus = MB_ENOERR;
    ULONG usTimerT35_50us;

    ( void )ucSlaveAddress;
    ENTER_CRITICAL_SECTION(  );

    if( xMBPortSerialInit6( ucPort, ulBaudRate, 8, eParity ) != TRUE )
    {
        eStatus = MB_EPORTERR;
    }
    else
    {
        if( ulBaudRate > 19200 )
        {
            usTimerT35_50us = 35;
        }
        else
        {
            usTimerT35_50us = ( 7UL * 2200000UL ) / ( 2UL * ulBaudRate );
        }
        if( xMBPortTimersInit6( ( USHORT ) usTimerT35_50us ) != TRUE )
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION(  );

    return eStatus;
}

void
eMBRTUStart( void )
{
    ENTER_CRITICAL_SECTION(  );
    /* Initially the receiver is in the state STATE_RX_INIT. we start
     * the timer and if no character is received within t3.5 we change
     * to STATE_RX_IDLE. This makes sure that we delay startup of the
     * modbus protocol stack until the bus is free.
     */
    eRcvState = STATE_RX_INIT;
    vMBPortSerialEnable( TRUE, FALSE );
    vMBPortTimersEnable(  );

    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStop( void )
{
    ENTER_CRITICAL_SECTION(  );
    vMBPortSerialEnable( FALSE, FALSE );
    vMBPortTimersDisable(  );
    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStart3( void )
{
    ENTER_CRITICAL_SECTION(  );
    eRcvState3 = STATE_RX_INIT;
    vMBPortSerialEnable3( TRUE, FALSE );
    vMBPortTimersEnable3(  );
    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStop3( void )
{
    ENTER_CRITICAL_SECTION(  );
    vMBPortSerialEnable3( FALSE, FALSE );
    vMBPortTimersDisable3(  );
    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStart6( void )
{
    ENTER_CRITICAL_SECTION(  );
    eRcvState6 = STATE_RX_INIT;
    vMBPortSerialEnable6( TRUE, FALSE );
    vMBPortTimersEnable6(  );
    EXIT_CRITICAL_SECTION(  );
}

void
eMBRTUStop6( void )
{
    ENTER_CRITICAL_SECTION(  );
    vMBPortSerialEnable6( FALSE, FALSE );
    vMBPortTimersDisable6(  );
    EXIT_CRITICAL_SECTION(  );
}

eMBErrorCode
eMBRTUReceive( UCHAR * pucRcvAddress, UCHAR ** pucFrame, USHORT * pusLength )
{
//    BOOL            xFrameReceived = FALSE;
    eMBErrorCode    eStatus = MB_ENOERR;

    ENTER_CRITICAL_SECTION(  );
    assert_param( usRcvBufferPos < MB_SER_PDU_SIZE_MAX );

    /* Length and CRC check */
    if( ( usRcvBufferPos >= MB_SER_PDU_SIZE_MIN )
        && ( usMBCRC16( ( UCHAR * ) ucRTUInBuf, usRcvBufferPos ) == 0 ) )
    {
        /* Save the address field. All frames are passed to the upper layed
         * and the decision if a frame is used is done there.
         */
			  
        *pucRcvAddress = ucRTUInBuf[MB_SER_PDU_ADDR_OFF];

        /* Total length of Modbus-PDU is Modbus-Serial-Line-PDU minus
         * size of address field and CRC checksum.
         */
        *pusLength = ( USHORT )( usRcvBufferPos - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC );

        /* Return the start of the Modbus PDU to the caller. */
			for(int i = 0;i < *pusLength;i++)     //把负载转移到临时buff
			{
				ucRTUOutBuf[i+MB_SER_PDU_PDU_OFF] = ucRTUInBuf[i+MB_SER_PDU_PDU_OFF];
			}
        *pucFrame = ( UCHAR * ) & ucRTUOutBuf[MB_SER_PDU_PDU_OFF];
//        xFrameReceived = TRUE;
    }
    else
    {
        eStatus = MB_EIO;
    }

    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUSend( UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength )
{
    eMBErrorCode    eStatus = MB_ENOERR;
    USHORT          usCRC16;

    ENTER_CRITICAL_SECTION(  );

    /* Check if the receiver is still in idle state. If not we where to
     * slow with processing the received frame and the master sent another
     * frame on the network. We have to abort sending the frame.
     */
    if( eRcvState == STATE_RX_IDLE )
    {
			/* First byte before the Modbus-PDU is the slave address. */
        pucSndBufferCur = ( UCHAR * ) pucFrame - 1;
        usSndBufferCount = 1;

        /* Now copy the Modbus-PDU into the Modbus-Serial-Line-PDU. */
        pucSndBufferCur[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
        usSndBufferCount += usLength;

        /* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
        usCRC16 = usMBCRC16( ( UCHAR * ) pucSndBufferCur, usSndBufferCount );
        pucSndBufferCur[usSndBufferCount++] = ( UCHAR )( usCRC16 & 0xFF );
        pucSndBufferCur[usSndBufferCount++] = ( UCHAR )( usCRC16 >> 8 );
				
//        /* First byte before the Modbus-PDU is the slave address. */
//				pucSndBufferCur = ucRTUOutBuf;      
//        usSndBufferCount = 1;

//        /* Now copy the Modbus-PDU into the Modbus-Serial-Line-PDU. */
//        pucSndBufferCur[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
//				for(int i = 0;i < usLength;i++)                 //把负载转移到发送buff
//				{
//					pucSndBufferCur[MB_SER_PDU_ADDR_OFF + 1 + i] = pucFrame[i];
//				}
//        usSndBufferCount += usLength;

//        /* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
//        usCRC16 = usMBCRC16( ( UCHAR * ) pucSndBufferCur, usSndBufferCount );
//        pucSndBufferCur[usSndBufferCount++] = ( UCHAR )( usCRC16 & 0xFF );
//        pucSndBufferCur[usSndBufferCount++] = ( UCHAR )( usCRC16 >> 8 );

        /* Activate the transmitter. */
        eSndState = STATE_TX_XMIT;
        vMBPortSerialEnable( FALSE, TRUE );
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUReceive3( UCHAR * pucRcvAddress, UCHAR ** pucFrame, USHORT * pusLength )
{
    eMBErrorCode eStatus = MB_ENOERR;

    ENTER_CRITICAL_SECTION(  );
    assert_param( usRcvBufferPos3 < MB_SER_PDU_SIZE_MAX );

    if( ( usRcvBufferPos3 >= MB_SER_PDU_SIZE_MIN )
        && ( usMBCRC16( ( UCHAR * ) ucRTUInBuf3, usRcvBufferPos3 ) == 0 ) )
    {
        *pucRcvAddress = ucRTUInBuf3[MB_SER_PDU_ADDR_OFF];
        *pusLength = ( USHORT )( usRcvBufferPos3 - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC );
        for(int i = 0; i < *pusLength; i++)
        {
            ucRTUOutBuf3[i + MB_SER_PDU_PDU_OFF] = ucRTUInBuf3[i + MB_SER_PDU_PDU_OFF];
        }
        *pucFrame = ( UCHAR * ) & ucRTUOutBuf3[MB_SER_PDU_PDU_OFF];
    }
    else
    {
        eStatus = MB_EIO;
    }

    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUSend3( UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength )
{
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT usCRC16;

    ENTER_CRITICAL_SECTION(  );

    if( eRcvState3 == STATE_RX_IDLE )
    {
        pucSndBufferCur3 = ( UCHAR * ) pucFrame - 1;
        usSndBufferCount3 = 1;
        pucSndBufferCur3[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
        usSndBufferCount3 += usLength;
        usCRC16 = usMBCRC16( ( UCHAR * ) pucSndBufferCur3, usSndBufferCount3 );
        pucSndBufferCur3[usSndBufferCount3++] = ( UCHAR )( usCRC16 & 0xFF );
        pucSndBufferCur3[usSndBufferCount3++] = ( UCHAR )( usCRC16 >> 8 );
        eSndState3 = STATE_TX_XMIT;
        vMBPortSerialEnable3( FALSE, TRUE );
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUReceive6( UCHAR * pucRcvAddress, UCHAR ** pucFrame, USHORT * pusLength )
{
    eMBErrorCode eStatus = MB_ENOERR;

    ENTER_CRITICAL_SECTION(  );
    assert_param( usRcvBufferPos6 < MB_SER_PDU_SIZE_MAX );

    if( ( usRcvBufferPos6 >= MB_SER_PDU_SIZE_MIN )
        && ( usMBCRC16( ( UCHAR * ) ucRTUInBuf6, usRcvBufferPos6 ) == 0 ) )
    {
        *pucRcvAddress = ucRTUInBuf6[MB_SER_PDU_ADDR_OFF];
        *pusLength = ( USHORT )( usRcvBufferPos6 - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC );
        for(int i = 0; i < *pusLength; i++)
        {
            ucRTUOutBuf6[i + MB_SER_PDU_PDU_OFF] = ucRTUInBuf6[i + MB_SER_PDU_PDU_OFF];
        }
        *pucFrame = ( UCHAR * ) & ucRTUOutBuf6[MB_SER_PDU_PDU_OFF];
    }
    else
    {
        eStatus = MB_EIO;
    }

    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

eMBErrorCode
eMBRTUSend6( UCHAR ucSlaveAddress, const UCHAR * pucFrame, USHORT usLength )
{
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT usCRC16;

    ENTER_CRITICAL_SECTION(  );

    if( eRcvState6 == STATE_RX_IDLE )
    {
        pucSndBufferCur6 = ( UCHAR * ) pucFrame - 1;
        usSndBufferCount6 = 1;
        pucSndBufferCur6[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
        usSndBufferCount6 += usLength;
        usCRC16 = usMBCRC16( ( UCHAR * ) pucSndBufferCur6, usSndBufferCount6 );
        pucSndBufferCur6[usSndBufferCount6++] = ( UCHAR )( usCRC16 & 0xFF );
        pucSndBufferCur6[usSndBufferCount6++] = ( UCHAR )( usCRC16 >> 8 );
        eSndState6 = STATE_TX_XMIT;
        vMBPortSerialEnable6( FALSE, TRUE );
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION(  );
    return eStatus;
}

BOOL
xMBRTUReceiveFSM( void )
{
    BOOL            xTaskNeedSwitch = FALSE;
    UCHAR           ucByte;

    assert_param( eSndState == STATE_TX_IDLE );

    /* Always read the character. */
    ( void )xMBPortSerialGetByte( ( CHAR * ) & ucByte );

    switch ( eRcvState )
    {
        /* If we have received a character in the init state we have to
         * wait until the frame is finished.
         */
    case STATE_RX_INIT:
        vMBPortTimersEnable( );
        break;

        /* In the error state we wait until all characters in the
         * damaged frame are transmitted.
         */
    case STATE_RX_ERROR:
        vMBPortTimersEnable( );
        break;

        /* In the idle state we wait for a new character. If a character
         * is received the t1.5 and t3.5 timers are started and the
         * receiver is in the state STATE_RX_RECEIVCE.
         */
    case STATE_RX_IDLE:
        usRcvBufferPos = 0;
        ucRTUInBuf[usRcvBufferPos++] = ucByte;
        eRcvState = STATE_RX_RCV;

        /* Enable t3.5 timers. */
        vMBPortTimersEnable( );
        break;

        /* We are currently receiving a frame. Reset the timer after
         * every character received. If more than the maximum possible
         * number of bytes in a modbus frame is received the frame is
         * ignored.
         */
    case STATE_RX_RCV:
        if( usRcvBufferPos < MB_SER_PDU_SIZE_MAX )
        {
            ucRTUInBuf[usRcvBufferPos++] = ucByte;
        }
        else
        {
            eRcvState = STATE_RX_ERROR;
        }
        vMBPortTimersEnable();
        break;
    }
    return xTaskNeedSwitch;
}

BOOL
xMBRTUTransmitFSM( void )
{
    BOOL            xNeedPoll = FALSE;

//    assert_param( eRcvState == STATE_RX_IDLE );

    switch ( eSndState )
    {
        /* We should not get a transmitter event if the transmitter is in
         * idle state.  */
    case STATE_TX_IDLE:
        /* enable receiver/disable transmitter. */
        vMBPortSerialEnable( TRUE, FALSE );
        break;

    case STATE_TX_XMIT:
        /* check if we are finished. */
        if( usSndBufferCount != 0 )
        {
            xMBPortSerialPutByte( ( CHAR )*pucSndBufferCur );
            pucSndBufferCur++;  /* next byte in sendbuffer. */
            usSndBufferCount--;
        }
        else
        {
            xNeedPoll = xMBPortEventPost( EV_FRAME_SENT );
            /* Disable transmitter. This prevents another transmit buffer
             * empty interrupt. */
						xMBPortSerialWaitTC();
            vMBPortSerialEnable( TRUE, FALSE );
            eSndState = STATE_TX_IDLE;
//					LED2_OFF
        }
        break;
    }

    return xNeedPoll;
}

BOOL
xMBRTUTimerT35Expired( void )
{
    BOOL            xNeedPoll = FALSE;

    switch ( eRcvState )
    {
        /* Timer t35 expired. Startup phase is finished. */
    case STATE_RX_INIT:
        xNeedPoll = xMBPortEventPost( EV_READY );
        break;

        /* A frame was received and t35 expired. Notify the listener that
         * a new frame was received. */
    case STATE_RX_RCV:
//			LED1_OFF
        xNeedPoll = xMBPortEventPost( EV_FRAME_RECEIVED );
        break;

        /* An error occured while receiving the frame. */
    case STATE_RX_ERROR:
        break;

        /* Function called in an illegal state. */
    default:
        assert_param( ( eRcvState == STATE_RX_INIT ) ||
                ( eRcvState == STATE_RX_RCV ) || ( eRcvState == STATE_RX_ERROR ) );
    }

    vMBPortTimersDisable(  );
    eRcvState = STATE_RX_IDLE;

    return xNeedPoll;
}

BOOL
xMBRTUReceiveFSM3( void )
{
    BOOL xTaskNeedSwitch = FALSE;
    UCHAR ucByte;

    assert_param( eSndState3 == STATE_TX_IDLE );
    ( void )xMBPortSerialGetByte3( ( CHAR * ) & ucByte );

    switch ( eRcvState3 )
    {
    case STATE_RX_INIT:
        vMBPortTimersEnable3( );
        break;
    case STATE_RX_ERROR:
        vMBPortTimersEnable3( );
        break;
    case STATE_RX_IDLE:
        usRcvBufferPos3 = 0;
        ucRTUInBuf3[usRcvBufferPos3++] = ucByte;
        eRcvState3 = STATE_RX_RCV;
        vMBPortTimersEnable3( );
        break;
    case STATE_RX_RCV:
        if( usRcvBufferPos3 < MB_SER_PDU_SIZE_MAX )
        {
            ucRTUInBuf3[usRcvBufferPos3++] = ucByte;
        }
        else
        {
            eRcvState3 = STATE_RX_ERROR;
        }
        vMBPortTimersEnable3();
        break;
    }
    return xTaskNeedSwitch;
}

BOOL
xMBRTUTransmitFSM3( void )
{
    BOOL xNeedPoll = FALSE;

    switch ( eSndState3 )
    {
    case STATE_TX_IDLE:
        vMBPortSerialEnable3( TRUE, FALSE );
        break;
    case STATE_TX_XMIT:
        if( usSndBufferCount3 != 0 )
        {
            xMBPortSerialPutByte3( ( CHAR )*pucSndBufferCur3 );
            pucSndBufferCur3++;
            usSndBufferCount3--;
        }
        else
        {
            xNeedPoll = xMBPortEventPost3( EV_FRAME_SENT );
            xMBPortSerialWaitTC3();
            vMBPortSerialEnable3( TRUE, FALSE );
            eSndState3 = STATE_TX_IDLE;
        }
        break;
    }

    return xNeedPoll;
}

BOOL
xMBRTUTimerT35Expired3( void )
{
    BOOL xNeedPoll = FALSE;

    switch ( eRcvState3 )
    {
    case STATE_RX_INIT:
        xNeedPoll = xMBPortEventPost3( EV_READY );
        break;
    case STATE_RX_RCV:
        xNeedPoll = xMBPortEventPost3( EV_FRAME_RECEIVED );
        break;
    case STATE_RX_ERROR:
        break;
    default:
        assert_param( ( eRcvState3 == STATE_RX_INIT ) ||
                ( eRcvState3 == STATE_RX_RCV ) || ( eRcvState3 == STATE_RX_ERROR ) );
    }

    vMBPortTimersDisable3(  );
    eRcvState3 = STATE_RX_IDLE;

    return xNeedPoll;
}

BOOL
xMBRTUReceiveFSM6( void )
{
    BOOL xTaskNeedSwitch = FALSE;
    UCHAR ucByte;

    assert_param( eSndState6 == STATE_TX_IDLE );
    ( void )xMBPortSerialGetByte6( ( CHAR * ) & ucByte );

    switch ( eRcvState6 )
    {
    case STATE_RX_INIT:
        vMBPortTimersEnable6( );
        break;
    case STATE_RX_ERROR:
        vMBPortTimersEnable6( );
        break;
    case STATE_RX_IDLE:
        usRcvBufferPos6 = 0;
        ucRTUInBuf6[usRcvBufferPos6++] = ucByte;
        eRcvState6 = STATE_RX_RCV;
        vMBPortTimersEnable6( );
        break;
    case STATE_RX_RCV:
        if( usRcvBufferPos6 < MB_SER_PDU_SIZE_MAX )
        {
            ucRTUInBuf6[usRcvBufferPos6++] = ucByte;
        }
        else
        {
            eRcvState6 = STATE_RX_ERROR;
        }
        vMBPortTimersEnable6();
        break;
    }
    return xTaskNeedSwitch;
}

BOOL
xMBRTUTransmitFSM6( void )
{
    BOOL xNeedPoll = FALSE;

    switch ( eSndState6 )
    {
    case STATE_TX_IDLE:
        vMBPortSerialEnable6( TRUE, FALSE );
        break;
    case STATE_TX_XMIT:
        if( usSndBufferCount6 != 0 )
        {
            xMBPortSerialPutByte6( ( CHAR )*pucSndBufferCur6 );
            pucSndBufferCur6++;
            usSndBufferCount6--;
        }
        else
        {
            xNeedPoll = xMBPortEventPost6( EV_FRAME_SENT );
            xMBPortSerialWaitTC6();
            vMBPortSerialEnable6( TRUE, FALSE );
            eSndState6 = STATE_TX_IDLE;
        }
        break;
    }

    return xNeedPoll;
}

BOOL
xMBRTUTimerT35Expired6( void )
{
    BOOL xNeedPoll = FALSE;

    switch ( eRcvState6 )
    {
    case STATE_RX_INIT:
        xNeedPoll = xMBPortEventPost6( EV_READY );
        break;
    case STATE_RX_RCV:
        xNeedPoll = xMBPortEventPost6( EV_FRAME_RECEIVED );
        break;
    case STATE_RX_ERROR:
        break;
    default:
        assert_param( ( eRcvState6 == STATE_RX_INIT ) ||
                ( eRcvState6 == STATE_RX_RCV ) || ( eRcvState6 == STATE_RX_ERROR ) );
    }

    vMBPortTimersDisable6(  );
    eRcvState6 = STATE_RX_IDLE;

    return xNeedPoll;
}
