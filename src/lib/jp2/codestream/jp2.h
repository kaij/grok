/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the following copyright and
 *    permission notice:
 *
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, 2011-2012, Centre National d'Etudes Spatiales (CNES), FR
 * Copyright (c) 2012, CS Systemes d'Information, France
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
namespace grk {

/**
 @file jp2.h
 @brief The JPEG-2000 file format Reader/Writer (JP2)

 */

/** @defgroup JP2 JP2 - JPEG-2000 file format reader/writer */
/*@{*/

#define     JP2_JP   0x6a502020    /**< JPEG 2000 signature box */
#define     JP2_FTYP 0x66747970    /**< File type box */
#define     JP2_JP2H 0x6a703268    /**< JP2 header box (super-box) */
#define     JP2_IHDR 0x69686472    /**< Image header box */
#define     JP2_COLR 0x636f6c72    /**< Colour specification box */
#define     JP2_JP2C 0x6a703263    /**< Contiguous codestream box */
#define     JP2_URL  0x75726c20    /**< Data entry URL box */
#define     JP2_PCLR 0x70636c72    /**< Palette box */
#define     JP2_CMAP 0x636d6170    /**< Component Mapping box */
#define     JP2_CDEF 0x63646566    /**< Channel Definition box */
#define     JP2_DTBL 0x6474626c    /**< Data Reference box */
#define     JP2_BPCC 0x62706363    /**< Bits per component box */
#define     JP2_JP2  0x6a703220    /**< File type fields */

#define JP2_RES			0x72657320   /**< Resolution box (super-box) */
#define JP2_CAPTURE_RES 0x72657363   /**< Capture resolution box */
#define JP2_DISPLAY_RES 0x72657364   /**< Display resolution box */

#define JP2_JP2I 0x6a703269   /**< Intellectual property box */
#define JP2_XML  0x786d6c20   /**< XML box */
#define JP2_UUID 0x75756964   /**< UUID box */
#define JP2_UINF 0x75696e66   /**< UUID info box (super-box) */
#define JP2_ULST 0x756c7374   /**< UUID list box */

/* ----------------------------------------------------------------------- */

#define JP2_MAX_NUM_UUIDS	128

const uint8_t IPTC_UUID[16] = { 0x33, 0xC7, 0xA4, 0xD2, 0xB8, 0x1D, 0x47, 0x23,
		0xA0, 0xBA, 0xF1, 0xA3, 0xE0, 0x97, 0xAD, 0x38 };
const uint8_t XMP_UUID[16] = { 0xBE, 0x7A, 0xCF, 0xCB, 0x97, 0xA9, 0x42, 0xE8,
		0x9C, 0x71, 0x99, 0x94, 0x91, 0xE3, 0xAF, 0xAC };

enum JP2_STATE {
	JP2_STATE_NONE = 0x0,
	JP2_STATE_SIGNATURE = 0x1,
	JP2_STATE_FILE_TYPE = 0x2,
	JP2_STATE_HEADER = 0x4,
	JP2_STATE_CODESTREAM = 0x8,
	JP2_STATE_END_CODESTREAM = 0x10,
	JP2_STATE_UNKNOWN = 0x7fffffff /* ISO C restricts enumerator values to range of 'int' */
};

enum JP2_IMG_STATE {
	JP2_IMG_STATE_NONE = 0x0, JP2_IMG_STATE_UNKNOWN = 0x7fffffff
};

/**
 JP2 component
 */
struct grk_jp2_comps {
	uint32_t depth;
	uint32_t sgnd;
	uint32_t bpcc;
};

struct grk_jp2_buffer {
	grk_jp2_buffer(uint8_t *buf, size_t size, bool owns) :
			buffer(buf), len(size), ownsData(owns) {
	}
	grk_jp2_buffer() :
			grk_jp2_buffer(nullptr, 0, false) {
	}
	bool alloc(size_t length) {
		dealloc();
		buffer = (uint8_t*) grok_malloc(length);
		len = length;
		ownsData = buffer != nullptr;
		return buffer ? true : false;
	}
	void dealloc() {
		if (ownsData && buffer)
			grok_free(buffer);
		buffer = nullptr;
		ownsData = false;
	}
	uint8_t *buffer;
	size_t len;
	bool ownsData;
};

struct grk_jp2_uuid: public grk_jp2_buffer {
	grk_jp2_uuid(const uint8_t myuuid[16], uint8_t *buf, size_t size, bool owns) :
			grk_jp2_buffer(buf, size, owns) {
		for (int i = 0; i < 16; ++i) {
			uuid[i] = myuuid[i];
		}
	}
	uint8_t uuid[16];
};

struct grk_jp2;
typedef bool (*jp2_procedure)(grk_jp2 *jp2, BufferedStream*);

/**
 JPEG-2000 file format reader/writer
 */
struct grk_jp2 {
	/** handle to the J2K codec  */
	grk_j2k *j2k;
	/** list of validation procedures */
	std::vector<jp2_procedure> *m_validation_list;
	/** list of execution procedures */
	std::vector<jp2_procedure> *m_procedure_list;

	/* width of image */
	uint32_t w;
	/* height of image */
	uint32_t h;
	/* number of components in the image */
	uint32_t numcomps;
	uint32_t bpc;
	uint32_t C;
	uint32_t UnkC;
	uint32_t IPR;
	uint32_t meth;
	uint32_t approx;
	uint32_t enumcs;
	uint32_t precedence;
	uint32_t brand;
	uint32_t minversion;
	uint32_t numcl;
	uint32_t *cl;
	grk_jp2_comps *comps;
	/* FIXME: The following two variables are used to save offset
	 as we write out a JP2 file to disk. This mechanism is not flexible
	 as codec writers will need to extend those fields as new part
	 of the standard are implemented.
	 */
	uint64_t j2k_codestream_offset;
	bool needs_xl_jp2c_box_length;
	uint32_t jp2_state;
	uint32_t jp2_img_state;
	grk_jp2_color color;

	bool has_capture_resolution;
	double capture_resolution[2];

	bool has_display_resolution;
	double display_resolution[2];

	grk_jp2_buffer xml;
	grk_jp2_uuid uuids[JP2_MAX_NUM_UUIDS];
	uint32_t numUuids;
};

/**
 JP2 Box
 */
struct grk_jp2_box {
	uint64_t length;
	uint32_t type;
};

struct grk_jp2_header_handler {
	/* marker value */
	uint32_t id;
	/* action linked to the marker */
	bool (*handler)(grk_jp2 *jp2, uint8_t *p_header_data, uint32_t header_size);
};

struct grk_jp2_img_header_writer_handler {
	/* action to perform */
	uint8_t* (*handler)(grk_jp2 *jp2, uint32_t *data_size);
	/* result of the action : data */
	uint8_t *m_data;
	/* size of data */
	uint32_t m_size;
};

/** @name Exported functions */
/*@{*/
/* ----------------------------------------------------------------------- */

/**
 Setup the decoder decoding parameters using user parameters.
 Decoding parameters are returned in jp2->j2k->cp.
 @param jp2 JP2 decompressor handle
 @param parameters decompression parameters
 */
void jp2_setup_decoder(void *jp2_void,  grk_dparameters  *parameters);

/**
 * Decode an image from a JPEG-2000 file stream
 * @param jp2 JP2 decompressor handle
 * @param p_stream  FIXME DOC
 * @param p_image   FIXME DOC

 *
 * @return a decoded image if successful, returns nullptr otherwise
 */
bool jp2_decode(grk_jp2 *jp2, grk_plugin_tile *tile, BufferedStream *p_stream,
		grk_image *p_image);

/**
 * Setup the encoder parameters using the current image and using user parameters.
 * Coding parameters are returned in jp2->j2k->cp.
 *
 * @param jp2 JP2 compressor handle
 * @param parameters compression parameters
 * @param image input filled image

 * @return true if successful, false otherwise
 */
bool jp2_setup_encoder(grk_jp2 *jp2,  grk_cparameters  *parameters,
		grk_image *image);

/**
 Encode an image into a JPEG-2000 file stream
 @param jp2      JP2 compressor handle
 @param stream    Output buffer stream
 @return true if successful, returns false otherwise
 */
bool jp2_encode(grk_jp2 *jp2, grk_plugin_tile *tile, BufferedStream *stream);

/**
 * Starts a compression scheme, i.e. validates the codec parameters, writes the header.
 *
 * @param  jp2    the jpeg2000 file codec.
 * @param  stream    the stream object.
 * @param  p_image   FIXME DOC

 *
 * @return true if the codec is valid.
 */
bool jp2_start_compress(grk_jp2 *jp2, BufferedStream *stream, grk_image *p_image);

/**
 * Ends the compression procedures and possibly add data to be read after the
 * codestream.
 */
bool jp2_end_compress(grk_jp2 *jp2, BufferedStream *cio);

/* ----------------------------------------------------------------------- */

/**
 * Ends the decompression procedures and possibly add data to be read after the
 * codestream.
 */
bool jp2_end_decompress(grk_jp2 *jp2, BufferedStream *cio);

/**
 * Reads a jpeg2000 file header structure.
 *
 * @param p_stream the stream to read data from.
 * @param jp2 the jpeg2000 file header structure.
 * @param p_image   FIXME DOC
 
 *
 * @return true if the box is valid.
 */
bool jp2_read_header(BufferedStream *p_stream, grk_jp2 *jp2,
		 grk_header_info  *header_info, grk_image **p_image);

/**
 * Reads a tile header.
 * @param  p_jp2         the jpeg2000 codec.
 * @param  tile_index  FIXME DOC
 * @param  data_size   FIXME DOC
 * @param  p_tile_x0     FIXME DOC
 * @param  p_tile_y0     FIXME DOC
 * @param  p_tile_x1     FIXME DOC
 * @param  p_tile_y1     FIXME DOC
 * @param  p_nb_comps    FIXME DOC
 * @param  p_go_on       FIXME DOC
 * @param  p_stream      the stream to write data to.
 
 */
bool jp2_read_tile_header(grk_jp2 *p_jp2, uint16_t *tile_index,
		uint64_t *data_size, uint32_t *p_tile_x0, uint32_t *p_tile_y0,
		uint32_t *p_tile_x1, uint32_t *p_tile_y1, uint32_t *p_nb_comps,
		bool *p_go_on, BufferedStream *p_stream);

/**
 * Writes a tile.
 *
 * @param  p_jp2    the jpeg2000 codec.
 * @param tile_index  FIXME DOC
 * @param p_data        FIXME DOC
 * @param data_size   FIXME DOC
 * @param  p_stream      the stream to write data to.
 
 */
bool jp2_write_tile(grk_jp2 *p_jp2, uint16_t tile_index, uint8_t *p_data,
		uint64_t data_size, BufferedStream *p_stream);

/**
 * Decode tile data.
 * @param  p_jp2    the jpeg2000 codec.
 * @param  tile_index FIXME DOC
 * @param  p_data       FIXME DOC
 * @param  data_size  FIXME DOC
 * @param  p_stream      the stream to write data to.
 
 *
 * @return FIXME DOC
 */
bool jp2_decode_tile(grk_jp2 *p_jp2, uint16_t tile_index, uint8_t *p_data,
		uint64_t data_size, BufferedStream *p_stream);

/**
 * Creates a jpeg2000 file decompressor.
 *
 * @return  an empty jpeg2000 file codec.
 */
grk_jp2* jp2_create(bool p_is_decoder);

/**
 Destroy a JP2 decompressor handle
 @param jp2 JP2 decompressor handle to destroy
 */
void jp2_destroy(grk_jp2 *jp2);

/**
 * Sets the given area to be decoded. This function should be called right after grok_read_header and before any tile header reading.
 *
 * @param  p_jp2      the jpeg2000 codec.
 * @param  p_image     FIXME DOC
 * @param  start_x   the left position of the rectangle to decode (in image coordinates).
 * @param  start_y    the up position of the rectangle to decode (in image coordinates).
 * @param  end_x      the right position of the rectangle to decode (in image coordinates).
 * @param  end_y      the bottom position of the rectangle to decode (in image coordinates).
 *
 * @return  true      if the area could be set.
 */
bool jp2_set_decode_area(grk_jp2 *p_jp2, grk_image *p_image, uint32_t start_x,
		uint32_t start_y, uint32_t end_x, uint32_t end_y);

/**
 *
 */
bool jp2_get_tile(grk_jp2 *p_jp2, BufferedStream *p_stream, grk_image *p_image, uint16_t tile_index);

/**
 * Dump some elements from the JP2 decompression structure .
 *
 *@param p_jp2        the jp2 codec.
 *@param flag        flag to describe what elements are dump.
 *@param out_stream      output stream where dump the elements.
 *
 */
void jp2_dump(grk_jp2 *p_jp2, int32_t flag, FILE *out_stream);

/**
 * Get the codestream info from a JPEG2000 codec.
 *
 *@param  p_jp2        jp2 codec.
 *
 *@return  the codestream information extract from the jpg2000 codec
 */
 grk_codestream_info_v2  *  jp2_get_cstr_info(grk_jp2 *p_jp2);

/**
 * Get the codestream index from a JPEG2000 codec.
 *
 *@param  p_jp2        jp2 codec.
 *
 *@return  the codestream index extract from the jpg2000 codec
 */
 grk_codestream_index  *  jp2_get_cstr_index(grk_jp2 *p_jp2);

/*@}*/

/*@}*/

}

