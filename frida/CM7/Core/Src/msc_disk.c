
#include "tusb.h"

#include "ff.h"

#include "ff_gen_drv.h"
#include "sd_diskio.h"

#if CFG_TUD_MSC

// whether host does safe-eject
static bool ejected = false;

#define DISK_BLOCK_SIZE 512

FRESULT res;


void fatfs_init(void) {

    // Mount the filesystem
    /*FRESULT fres = f_mount(&FatFs, "", 1);
    if (fres != FR_OK) {
        // Handle FATFS mount error
        // Again, you might want to print an error message or indicate an error via LED
    	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);

        return;
    }*/


    /*if(f_mount(&SDFatFS, (TCHAR const*)SDPath, 0) != FR_OK)
	{
    	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
	}

    // Optionally, you can open or create a file to test if everything is working
    res = f_open(&SDFile, "test.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    if (res != FR_OK) {
    	HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
		HAL_Delay(500);
        return;
    }

    // Close the file, as it was just a test
    f_close(&SDFile);

    ejected = false;*/

	if(SD_Driver.disk_initialize(0) != 0){
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 1);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, 0);
		HAL_Delay(500);
	}

}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  (void) lun;

  const char vid[] = "TinyUSB";
  const char pid[] = "Frida";
  const char rev[] = "1.0";

  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
  (void) lun;

  // RAM disk is ready until ejected
  if (ejected) {
    // Additional Sense 3A-00 is NOT_FOUND
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }


  return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size) {

	DRESULT res;
	res = SD_Driver.disk_ioctl(lun, GET_SECTOR_COUNT, block_count);
	if(res != RES_OK) return;
    res = SD_Driver.disk_ioctl(lun, GET_SECTOR_SIZE, block_size);
    if(res != RES_OK) return;

    return;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  (void) lun;
  (void) power_condition;

  if ( load_eject )
  {
    if (start)
    {
      // load disk storage
    }else
    {
      // unload disk storage
      ejected = true;
    }
  }

  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    (void) lun;

    DWORD sector = lba; // Logical Block Addressing
    //UINT byteRead;

    //DRESULT res = disk_read(0, buffer, sector, bufsize / DISK_BLOCK_SIZE);
    DRESULT res = SD_Driver.disk_read(lun, buffer, sector, bufsize);
    if (res != RES_OK) {
        return -1;
    }

    return bufsize;
}

bool tud_msc_is_writable_cb (uint8_t lun)
{
  (void) lun;

#ifdef CFG_EXAMPLE_MSC_READONLY
  return false;
#else
  return true;
#endif
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    (void) lun;

    DWORD sector = lba; // Logical Block Addressing
    //UINT byteWritten;

    //DRESULT res = disk_write(0, buffer, sector, bufsize / DISK_BLOCK_SIZE);
    DRESULT res = SD_Driver.disk_write(lun, buffer, sector, bufsize);
    if (res != RES_OK) {
        return -1;
    }

    return bufsize;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t tud_msc_scsi_cb (uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here

  void const* response = NULL;
  int32_t resplen = 0;

  // most scsi handled is input
  bool in_xfer = true;

  switch (scsi_cmd[0])
  {
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
    break;
  }

  // return resplen must not larger than bufsize
  if ( resplen > bufsize ) resplen = bufsize;

  if ( response && (resplen > 0) )
  {
    if(in_xfer)
    {
      memcpy(buffer, response, (size_t) resplen);
    }else
    {
      // SCSI output
    }
  }

  return (int32_t) resplen;
}

#endif