/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "MMC.h"	    /* Header file of MMC SD card control module */


DSTATUS stat;
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	//return the status code
	return stat;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	int result;

	result = mmcInit();
	switch (result)
	{
	case MMC_SUCCESS:
		stat = RES_OK;
		break;
	case MMC_BLOCK_SET_ERROR:
		stat = RES_ERROR;
		break;
	case MMC_RESPONSE_ERROR:
		stat = RES_ERROR;
		break;
	default:
		stat = RES_NOTRDY;
		break;
	}
/*
		MMC_DATA_TOKEN_ERROR
		MMC_INIT_ERROR
		MMC_CRC_ERROR
		MMC_WRITE_ERROR
		MMC_OTHER_ERROR
		MMC_TIMEOUT_ERROR
		*/
	return stat;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	int result = RES_PARERR;
	int iIdx   = 0;

	for(iIdx = 0; ((iIdx < count) && (stat == RES_OK)); iIdx++)
	{
		result = mmcReadBlock(sector*SECTOR_SIZE, SECTOR_SIZE, buff);
		if(result == MMC_SUCCESS)
		{
			stat = RES_OK;
			buff += SECTOR_SIZE;
		}
		else
		{
			stat = RES_ERROR;
		}
	}


	return stat;
}

DRESULT disk_read_ex (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Bytes to read */
)
{
	int result = RES_PARERR;
	//int iIdx   = 0;

	//for(iIdx = 0; ((iIdx < count) && (stat == RES_OK)); iIdx++)
	{
		result = mmcReadBlock(sector*SECTOR_SIZE, count, buff);
		if(result == MMC_SUCCESS)
		{
			stat = RES_OK;
			//buff += SECTOR_SIZE;
		}
		else
		{
			stat = RES_ERROR;
		}
	}


	return stat;
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	int result = RES_PARERR;
	int iIdx   = 0;

	for(iIdx = 0; ((iIdx < count) && (stat == RES_OK)); iIdx++)
	{
		result = mmcWriteBlock(sector*SECTOR_SIZE, SECTOR_SIZE, buff);
		if(result == MMC_SUCCESS)
		{
			stat = RES_OK;
			buff += SECTOR_SIZE;
		}
		else
		{
			stat = RES_ERROR;
		}
	}


	return stat;

}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (cmd)
	{
	case CTRL_SYNC:
		stat = RES_OK; //as write is Write-around cache
		break;
	case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (WORD) */
		*(DWORD*)buff = SECTOR_COUNT;	//ZZZZ use the mmcReadCardSize
		stat = RES_OK;
		break;
	case GET_BLOCK_SIZE:
		*(DWORD*)buff = SECTOR_SIZE;
		stat = RES_OK;
		break;
	default:
		stat = RES_PARERR;
		break;
	}
	return stat;
}
#endif
