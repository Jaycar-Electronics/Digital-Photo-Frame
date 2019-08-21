
#include "JPEGDecoder.h"
#include "picojpeg.h"

JPEGDecoder JpegDec;

JPEGDecoder::JPEGDecoder(){
	mcu_x = 0 ;
	mcu_y = 0 ;
	is_available = 0;
	thisPtr = this;
}


JPEGDecoder::~JPEGDecoder(){
	if (pImage) delete[] pImage;
	pImage = NULL;
}


uint8_t JPEGDecoder::pjpeg_callback(uint8_t* pBuf, uint8_t buf_size, uint8_t *pBytes_actually_read, void *pCallback_data) {
	JPEGDecoder *thisPtr = JpegDec.thisPtr ;
	thisPtr->pjpeg_need_bytes_callback(pBuf, buf_size, pBytes_actually_read, pCallback_data);
	return 0;
}


uint8_t JPEGDecoder::pjpeg_need_bytes_callback(uint8_t* pBuf, uint8_t buf_size, uint8_t *pBytes_actually_read, void *pCallback_data) {
	uint n;

	//pCallback_data;

	n = jpg_min(g_nInFileSize - g_nInFileOfs, buf_size);

	if (jpg_source == JPEG_ARRAY) { // We are handling an array
		for (int i = 0; i < n; i++) {
			pBuf[i] = pgm_read_byte(jpg_data++);
			//Serial.println(pBuf[i],HEX);
		}
	}
	if (jpg_source == JPEG_FS_FILE) g_pInFileFs.read(pBuf,n); // else we are handling a file
	*pBytes_actually_read = (uint8_t)(n);
	g_nInFileOfs += n;
	return 0;
}

int JPEGDecoder::decode_mcu(void) {

	status = pjpeg_decode_mcu();

	if (status) {
		is_available = 0 ;

		if (status != PJPG_NO_MORE_BLOCKS) {
			Serial.print("pjpeg_decode_mcu() failed with status ");
			Serial.println(status);
			return -1;
		}
	}
	return 1;
}


int JPEGDecoder::read(void) {
	int y, x;
	uint16_t *pDst_row;

	if(is_available == 0 || mcu_y >= image_info.m_MCUSPerCol) {
		abort();
		return 0;
	}
	
	// Copy MCU's pixel blocks into the destination bitmap.
	pDst_row = pImage;
	for (y = 0; y < image_info.m_MCUHeight; y += 8) {

		const int by_limit = jpg_min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

		for (x = 0; x < image_info.m_MCUWidth; x += 8) {
			uint16_t *pDst_block = pDst_row + x;

			// Compute source byte offset of the block in the decoder's MCU buffer.
			uint src_ofs = (x * 8U) + (y * 16U);
			const uint8_t *pSrcR = image_info.m_pMCUBufR + src_ofs;
			const uint8_t *pSrcG = image_info.m_pMCUBufG + src_ofs;
			const uint8_t *pSrcB = image_info.m_pMCUBufB + src_ofs;

			const int bx_limit = jpg_min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

			if (image_info.m_scanType == PJPG_GRAYSCALE) {
				int bx, by;
				for (by = 0; by < by_limit; by++) {
					uint16_t *pDst = pDst_block;

					for (bx = 0; bx < bx_limit; bx++) {
#ifdef SWAP_BYTES
						*pDst++ = (*pSrcR & 0xF8) | (*pSrcR & 0xE0) >> 5 | (*pSrcR & 0xF8) << 5 | (*pSrcR & 0x1C) << 11;
#else
						*pDst++ = (*pSrcR & 0xF8) << 8 | (*pSrcR & 0xFC) <<3 | *pSrcR >> 3;
#endif
						pSrcR++;
					}

					pSrcR += (8 - bx_limit);

					pDst_block += row_pitch;
				}
			}
			else {
				int bx, by;
				for (by = 0; by < by_limit; by++) {
					uint16_t *pDst = pDst_block;

					for (bx = 0; bx < bx_limit; bx++) {
#ifdef SWAP_BYTES
						*pDst++ = (*pSrcR & 0xF8) | (*pSrcG & 0xE0) >> 5 | (*pSrcB & 0xF8) << 5 | (*pSrcG & 0x1C) << 11;
#else
						*pDst++ = (*pSrcR & 0xF8) << 8 | (*pSrcG & 0xFC) <<3 | *pSrcB >> 3;
#endif
						pSrcR++; pSrcG++; pSrcB++;
					}

					pSrcR += (8 - bx_limit);
					pSrcG += (8 - bx_limit);
					pSrcB += (8 - bx_limit);

					pDst_block += row_pitch;
				}
			}
		}
		pDst_row += (row_pitch * 8);
	}

	MCUx = mcu_x;
	MCUy = mcu_y;

	mcu_x++;
	if (mcu_x == image_info.m_MCUSPerRow) {
		mcu_x = 0;
		mcu_y++;
	}

	if(decode_mcu()==-1) is_available = 0 ;

	return 1;
}

int JPEGDecoder::readSwappedBytes(void) {
	int y, x;
	uint16_t *pDst_row;

	if(is_available == 0 || mcu_y >= image_info.m_MCUSPerCol) {
		abort();
		return 0;
	}
	
	// Copy MCU's pixel blocks into the destination bitmap.
	pDst_row = pImage;
	for (y = 0; y < image_info.m_MCUHeight; y += 8) {

		const int by_limit = jpg_min(8, image_info.m_height - (mcu_y * image_info.m_MCUHeight + y));

		for (x = 0; x < image_info.m_MCUWidth; x += 8) {
			uint16_t *pDst_block = pDst_row + x;

			// Compute source byte offset of the block in the decoder's MCU buffer.
			uint src_ofs = (x * 8U) + (y * 16U);
			const uint8_t *pSrcR = image_info.m_pMCUBufR + src_ofs;
			const uint8_t *pSrcG = image_info.m_pMCUBufG + src_ofs;
			const uint8_t *pSrcB = image_info.m_pMCUBufB + src_ofs;

			const int bx_limit = jpg_min(8, image_info.m_width - (mcu_x * image_info.m_MCUWidth + x));

			if (image_info.m_scanType == PJPG_GRAYSCALE) {
				int bx, by;
				for (by = 0; by < by_limit; by++) {
					uint16_t *pDst = pDst_block;

					for (bx = 0; bx < bx_limit; bx++) {

						*pDst++ = (*pSrcR & 0xF8) | (*pSrcR & 0xE0) >> 5 | (*pSrcR & 0xF8) << 5 | (*pSrcR & 0x1C) << 11;

						pSrcR++;
					}
				}
			}
			else {
				int bx, by;
				for (by = 0; by < by_limit; by++) {
					uint16_t *pDst = pDst_block;

					for (bx = 0; bx < bx_limit; bx++) {

						*pDst++ = (*pSrcR & 0xF8) | (*pSrcG & 0xE0) >> 5 | (*pSrcB & 0xF8) << 5 | (*pSrcG & 0x1C) << 11;

						pSrcR++; pSrcG++; pSrcB++;
					}

					pSrcR += (8 - bx_limit);
					pSrcG += (8 - bx_limit);
					pSrcB += (8 - bx_limit);

					pDst_block += row_pitch;
				}
			}
		}
		pDst_row += (row_pitch * 8);
	}

	MCUx = mcu_x;
	MCUy = mcu_y;

	mcu_x++;
	if (mcu_x == image_info.m_MCUSPerRow) {
		mcu_x = 0;
		mcu_y++;
	}

	if(decode_mcu()==-1) is_available = 0 ;

	return 1;
}


// Generic file call for SD or SPIFFS, uses leading / to distinguish SPIFFS files

// Call specific to SPIFFS
int JPEGDecoder::decodeFsFile(const char *pFilename) {

	fs::File pInFile = SPIFFS.open( pFilename, "r");

	return decodeFsFile(pInFile);
}

int JPEGDecoder::decodeFsFile(const String& pFilename) {

	fs::File pInFile = SPIFFS.open( pFilename, "r");

	return decodeFsFile(pInFile);
}

int JPEGDecoder::decodeFsFile(fs::File jpgFile) { // This is for the SPIFFS library

	g_pInFileFs = jpgFile;

	jpg_source = JPEG_FS_FILE; // Flag to indicate a SPIFFS file

	if (!g_pInFileFs) {
		Serial.println("ERROR: SPIFFS file not found!");
		return -1;
	}

	g_nInFileOfs = 0;

	g_nInFileSize = g_pInFileFs.size();

	return decodeCommon();
}


int JPEGDecoder::decodeArray(const uint8_t array[], uint32_t  array_size) {

	jpg_source = JPEG_ARRAY; // We are not processing a file, use arrays

	g_nInFileOfs = 0;

	jpg_data = (uint8_t *)array;

	g_nInFileSize = array_size;

	return decodeCommon();
}


int JPEGDecoder::decodeCommon(void) {

	width = 0;
	height = 0;
	comps = 0;
	MCUSPerRow = 0;
	MCUSPerCol = 0;
	scanType = (pjpeg_scan_type_t)0;
	MCUWidth = 0;
	MCUHeight = 0;

	status = pjpeg_decode_init(&image_info, pjpeg_callback, NULL, 0);

	if (status) {
		Serial.print("pjpeg_decode_init() failed with status ");
		Serial.println(status);

		if (status == PJPG_UNSUPPORTED_MODE) {
			Serial.println("Progressive JPEG files are not supported.");
		}

		return 0;
	}

	decoded_width =  image_info.m_width;
	decoded_height =  image_info.m_height;
	
	row_pitch = image_info.m_MCUWidth;
	pImage = new uint16_t[image_info.m_MCUWidth * image_info.m_MCUHeight];

	memset(pImage , 0 , image_info.m_MCUWidth * image_info.m_MCUHeight * sizeof(*pImage));

	row_blocks_per_mcu = image_info.m_MCUWidth >> 3;
	col_blocks_per_mcu = image_info.m_MCUHeight >> 3;

	is_available = 1 ;

	width = decoded_width;
	height = decoded_height;
	comps = 1;
	MCUSPerRow = image_info.m_MCUSPerRow;
	MCUSPerCol = image_info.m_MCUSPerCol;
	scanType = image_info.m_scanType;
	MCUWidth = image_info.m_MCUWidth;
	MCUHeight = image_info.m_MCUHeight;

	return decode_mcu();
}

void JPEGDecoder::abort(void) {

	mcu_x = 0 ;
	mcu_y = 0 ;
	is_available = 0;
	if(pImage) delete[] pImage;
	pImage = NULL;
	
#ifdef LOAD_SPIFFS
	if (jpg_source == JPEG_FS_FILE) if (g_pInFileFs) g_pInFileFs.close();
#endif

#if defined (LOAD_SD_LIBRARY) || defined (LOAD_SDFAT_LIBRARY)
	if (jpg_source == JPEG_SD_FILE) if (g_pInFileSd) g_pInFileSd.close();
#endif
}
