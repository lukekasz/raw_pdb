// Copyright 2011-2022, Molecular Matters GmbH <office@molecular-matters.com>
// See LICENSE.txt for licensing details (2-clause BSD License: https://opensource.org/licenses/BSD-2-Clause)

#include "PDB_PCH.h"
#include "PDB_RawFile.h"
#include "PDB_Types.h"
#include "PDB_Util.h"
#include "PDB_DirectMSFStream.h"
#include "Foundation/PDB_PointerUtil.h"
#include "Foundation/PDB_Memory.h"
#include "Foundation/PDB_Assert.h"


// parse the stream directory
// https://llvm.org/docs/PDB/MsfFile.html#the-stream-directory
namespace
{
	// ------------------------------------------------------------------------------------------------
	// ------------------------------------------------------------------------------------------------
	PDB_NO_DISCARD static uint32_t GetDirectoryBlockCount(const PDB::SuperBlock* superBlock) PDB_NO_EXCEPT
	{
		const uint32_t directoryBlockCount = PDB::ConvertSizeToBlockCount(superBlock->directorySize, superBlock->blockSize);

		return directoryBlockCount;
	}

	// ------------------------------------------------------------------------------------------------
	// ------------------------------------------------------------------------------------------------
	PDB_NO_DISCARD static const uint32_t* GetDirectoryBlockIndices(const void* data, const PDB::SuperBlock* superBlock) PDB_NO_EXCEPT
	{
		const size_t directoryIndicesFileOffset = PDB::ConvertBlockIndexToFileOffset(superBlock->directoryIndicesBlockIndex, superBlock->blockSize);
		const uint32_t* directoryBlockIndices = PDB::Pointer::Offset<const uint32_t*>(data, directoryIndicesFileOffset);

		return directoryBlockIndices;
	}
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PDB::RawFile::RawFile(RawFile&& other) PDB_NO_EXCEPT
	: m_data(PDB_MOVE(other.m_data))
	, m_superBlock(PDB_MOVE(other.m_superBlock))
	, m_directoryStream(PDB_MOVE(other.m_directoryStream))
	, m_streamCount(PDB_MOVE(other.m_streamCount))
	, m_streamSizes(PDB_MOVE(other.m_streamSizes))
	, m_streamBlocks(PDB_MOVE(other.m_streamBlocks))
{
	other.m_data = nullptr;
	other.m_superBlock = nullptr;
	other.m_streamCount = 0u;
	other.m_streamSizes = nullptr;
	other.m_streamBlocks = nullptr;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PDB::RawFile& PDB::RawFile::operator=(RawFile&& other) PDB_NO_EXCEPT
{
	if (this != &other)
	{
		PDB_DELETE_ARRAY(m_streamBlocks);

		m_data = PDB_MOVE(other.m_data);
		m_superBlock = PDB_MOVE(other.m_superBlock);
		m_directoryStream = PDB_MOVE(other.m_directoryStream);
		m_streamCount = PDB_MOVE(other.m_streamCount);
		m_streamSizes = PDB_MOVE(other.m_streamSizes);
		m_streamBlocks = PDB_MOVE(other.m_streamBlocks);

		other.m_data = nullptr;
		other.m_superBlock = nullptr;
		other.m_streamCount = 0u;
		other.m_streamSizes = nullptr;
		other.m_streamBlocks = nullptr;
	}

	return *this;
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PDB::RawFile::RawFile(const void* data) PDB_NO_EXCEPT
	: m_data(data)
	, m_superBlock(Pointer::Offset<const SuperBlock*>(data, 0u))
	, m_directoryStream(data, m_superBlock->blockSize, GetDirectoryBlockIndices(data, m_superBlock), GetDirectoryBlockCount(m_superBlock) * m_superBlock->blockSize)
	, m_streamCount(0u)
	, m_streamSizes(nullptr)
	, m_streamBlocks(nullptr)
{
	// https://llvm.org/docs/PDB/MsfFile.html#the-stream-directory
	// parse the directory from its contiguous version. the directory matches the following struct:
	//	struct StreamDirectory
	//	{
	//		uint32_t streamCount;
	//		uint32_t streamSizes[streamCount];
	//		uint32_t streamBlocks[streamCount][];
	//	};
	m_streamCount = *m_directoryStream.GetDataAtOffset<uint32_t>(0u);

	// we can assign pointers into the stream directly, since the RawFile keeps ownership of the directory stream
	m_streamSizes = m_directoryStream.GetDataAtOffset<uint32_t>(sizeof(uint32_t));
	const uint32_t* directoryStreamBlocks = m_directoryStream.GetDataAtOffset<uint32_t>(sizeof(uint32_t) + sizeof(uint32_t) * m_streamCount);

	// prepare indices for directly accessing individual streams
	m_streamBlocks = PDB_NEW_ARRAY(const uint32_t*, m_streamCount);

	const uint32_t* indicesForCurrentBlock = directoryStreamBlocks;
	for (uint32_t i = 0u; i < m_streamCount; ++i)
	{
		const uint32_t sizeInBytes = m_streamSizes[i];
		const uint32_t blockCount = ConvertSizeToBlockCount(sizeInBytes, m_superBlock->blockSize);
		m_streamBlocks[i] = indicesForCurrentBlock;
		
		indicesForCurrentBlock += blockCount;
	}
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
PDB::RawFile::~RawFile(void) PDB_NO_EXCEPT
{
	PDB_DELETE_ARRAY(m_streamBlocks);
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
template <typename T>
PDB_NO_DISCARD T PDB::RawFile::CreateMSFStream(uint32_t streamIndex) const PDB_NO_EXCEPT
{
	return T(m_data, m_superBlock->blockSize, m_streamBlocks[streamIndex], m_streamSizes[streamIndex]);
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
template <typename T>
PDB_NO_DISCARD T PDB::RawFile::CreateMSFStream(uint32_t streamIndex, uint32_t streamSize) const PDB_NO_EXCEPT
{
	PDB_ASSERT(streamSize <= m_streamSizes[streamIndex], "Invalid stream size.");

	return T(m_data, m_superBlock->blockSize, m_streamBlocks[streamIndex], streamSize);
}


// explicit template instantiation
template PDB::CoalescedMSFStream PDB::RawFile::CreateMSFStream<PDB::CoalescedMSFStream>(uint32_t streamIndex) const PDB_NO_EXCEPT;
template PDB::DirectMSFStream PDB::RawFile::CreateMSFStream<PDB::DirectMSFStream>(uint32_t streamIndex) const PDB_NO_EXCEPT;

template PDB::CoalescedMSFStream PDB::RawFile::CreateMSFStream<PDB::CoalescedMSFStream>(uint32_t streamIndex, uint32_t streamSize) const PDB_NO_EXCEPT;
template PDB::DirectMSFStream PDB::RawFile::CreateMSFStream<PDB::DirectMSFStream>(uint32_t streamIndex, uint32_t streamSize) const PDB_NO_EXCEPT;
