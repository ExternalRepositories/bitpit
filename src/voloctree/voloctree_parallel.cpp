/*---------------------------------------------------------------------------*\
 *
 *  bitpit
 *
 *  Copyright (C) 2015-2017 OPTIMAD engineering Srl
 *
 *  -------------------------------------------------------------------------
 *  License
 *  This file is part of bitpit.
 *
 *  bitpit is free software: you can redistribute it and/or modify it
 *  under the terms of the GNU Lesser General Public License v3 (LGPL)
 *  as published by the Free Software Foundation.
 *
 *  bitpit is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with bitpit. If not, see <http://www.gnu.org/licenses/>.
 *
\*---------------------------------------------------------------------------*/
#if BITPIT_ENABLE_MPI==1

#include <mpi.h>
#include <unordered_set>

#include "voloctree.hpp"

using namespace std;

namespace bitpit {

/*!
	Sets the MPI communicator to be used for parallel communications.

	\param communicator is the communicator to be used for parallel
	communications.
*/
void VolOctree::setCommunicator(MPI_Comm communicator)
{
	PatchKernel::setCommunicator(communicator);

	m_tree->setComm(communicator);
}

/*!
	Prepares the patch for performing the partitioning.

	Default implementation is a no-op function.

	\param trackPartitioninging if set to true the function will return the
	changes that will be performed in the alter step
	\result If the partitioning is tracked, returns a vector of adaption::Info
	that can be used to discover what changes will be performed in the alter
	step, otherwise an empty vector will be returned.
*/
std::vector<adaption::Info> VolOctree::_partitioningPrepare(bool trackPartitioning)
{
	std::vector<adaption::Info> partitioningData;
	if (trackPartitioning) {
		int currentRank = getRank();
		PabloUniform::LoadBalanceRanges loadBalanceRanges = m_tree->evalLoadBalanceRanges((std::vector<double> *) nullptr);
		for (const auto &entry : loadBalanceRanges.sendRanges) {
			int receiver = entry.first;
			if (receiver == currentRank) {
				continue;
			}

			adaption::Type adaptionType;
			if (loadBalanceRanges.sendAction == PabloUniform::LoadBalanceRanges::ACTION_DELETE) {
				adaptionType = adaption::TYPE_DELETION;
			} else {
				adaptionType = adaption::TYPE_PARTITION_SEND;
			}

			partitioningData.emplace_back();
			adaption::Info &partitioningInfo = partitioningData.back();
			partitioningInfo.entity = adaption::ENTITY_CELL;
			partitioningInfo.type   = adaptionType;
			partitioningInfo.rank   = receiver;

			const std::array<uint32_t, 2> &sendRange = entry.second;
			uint32_t beginTreeId = sendRange[0];
			uint32_t endTreeId   = sendRange[1];
			partitioningInfo.previous.reserve(endTreeId - beginTreeId);
			for (uint32_t treeId = beginTreeId; treeId < endTreeId; ++treeId) {
				OctantInfo octantInfo(treeId, true);
				partitioningInfo.previous.emplace_back();
				long &cellId = partitioningInfo.previous.back();
				cellId = getOctantId(octantInfo);
			}
		}
	}

	return partitioningData;
}

/*!
	Alter the patch performing the partitioning.

	Default implementation is a no-op function.

	\param trackPartitioning if set to true the function will return the changes
	done to the patch during the partitioning
	\result If the partitioning is tracked, returns a vector of adaption::Info
	with all the changes done to the patch during the adaption, otherwise an
	empty vector will be returned.
*/
std::vector<adaption::Info> VolOctree::_partitioningAlter(bool trackPartitioning)
{
	// Updating the tree
	m_tree->loadBalance();

	// Sync the patch
	std::vector<adaption::Info> partitioningData = sync(true, true, trackPartitioning);

	// The bounding box is frozen, it is not updated automatically
	setBoundingBox();

	return partitioningData;
}

/*!
	Cleanup patch data structured after the partitioning.
*/
void VolOctree::_partitioningCleanup()
{
}

}

#endif
