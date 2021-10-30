#ifndef __OGG_READER_H__
#define __OGG_READER_H__


	inline bool CheckOggVorbisFile (const UInt8* ogg_memory, size_t size, int* outChannels)
	{
		*outChannels = 0;
		if (size < 40) return false;
		/*
		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1| Byte
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| capture_pattern: Magic number for page start "OggS"           | 0-3
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| version       | header_type   | granule_position              | 4-7
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                                                               | 8-11
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                               | bitstream_serial_number       | 12-15
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                               | page_sequence_number          | 16-19
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                               | CRC_checksum                  | 20-23
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                               |page_segments  | segment_table | 24-27
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		| ...                                                           | 28-
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		*/

		// capture pattern (OggS)
		if (0x4f != ogg_memory[0]) return false;
		if (0x67 != ogg_memory[1]) return false;
		if (0x67 != ogg_memory[2]) return false;
		if (0x53 != ogg_memory[3]) return false;		
		// version
		if (0 != ogg_memory[4]) return false;
		// ensure its the first page(2) (and not last(1) and not a continued page(4)) 
		const UInt8 page_flag = ogg_memory[5];
		if (!((2 & page_flag) && !(1 & page_flag) && !(4 & page_flag))) return false;
		// vorbis packet id (must be #1)
		if (1 != ogg_memory[28]) return false;
		// vorbis header
		if (memcmp((void*)&ogg_memory[29], "vorbis", 6) != 0) return false;
		// vorbis version [35-38]
		*outChannels = ogg_memory[39];

		return true;
}

#endif // __OGG_READER_H__
