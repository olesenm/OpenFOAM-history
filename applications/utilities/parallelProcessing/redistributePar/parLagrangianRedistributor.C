/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2015 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "ListOps.H"
#include "parLagrangianRedistributor.H"
#include "passiveParticleCloud.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::parLagrangianRedistributor::parLagrangianRedistributor
(
    const fvMesh& srcMesh,
    const fvMesh& tgtMesh,
    const label nSrcCells,
    const mapDistributePolyMesh& distMap
)
:
    srcMesh_(srcMesh),
    tgtMesh_(tgtMesh),
    distMap_(distMap)
{
    const mapDistribute& cellMap = distMap_.cellMap();

    // Get destination processors and cells
    destinationProcID_ = labelList(tgtMesh_.nCells(), Pstream::myProcNo());
    cellMap.reverseDistribute(nSrcCells, destinationProcID_);

    destinationCell_ = identity(tgtMesh_.nCells());
    cellMap.reverseDistribute(nSrcCells, destinationCell_);
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

// Find all clouds (on all processors) and for each cloud all the objects.
// Result will be synchronised on all processors
void Foam::parLagrangianRedistributor::findClouds
(
    const fvMesh& mesh,
    wordList& cloudNames,
    List<wordList>& objectNames
)
{
    fileNameList localCloudDirs
    (
        readDir
        (
            mesh.time().timePath()
          / mesh.dbDir()
          / cloud::prefix,
            fileName::DIRECTORY
        )
    );

    cloudNames.setSize(localCloudDirs.size());
    forAll(localCloudDirs, i)
    {
        cloudNames[i] = localCloudDirs[i];
    }

    // Synchronise cloud names
    Pstream::combineGather(cloudNames, ListUniqueEqOp<word>());
    Pstream::combineScatter(cloudNames);

    objectNames.setSize(cloudNames.size());

    forAll(localCloudDirs, i)
    {
        // Do local scan for valid cloud objects
        IOobjectList sprayObjs
        (
            mesh,
            mesh.time().timeName(),
            cloud::prefix/localCloudDirs[i]
        );

        if (sprayObjs.lookup(word("positions")))
        {
            // One of the objects is positions so must be valid cloud

            label cloudI = findIndex(cloudNames, localCloudDirs[i]);

            objectNames[cloudI].setSize(sprayObjs.size());
            label objectI = 0;
            forAllConstIter(IOobjectList, sprayObjs, iter)
            {
                const word& name = iter.key();
                if (name != "positions")
                {
                    objectNames[cloudI][objectI++] = name;
                }
            }
            objectNames[cloudI].setSize(objectI);
        }
    }

    // Synchronise objectNames
    forAll(objectNames, cloudI)
    {
        Pstream::combineGather(objectNames[cloudI], ListUniqueEqOp<word>());
        Pstream::combineScatter(objectNames[cloudI]);
    }
}


Foam::autoPtr<Foam::mapDistributeBase>
Foam::parLagrangianRedistributor::redistributeLagrangianPositions
(
    passiveParticleCloud& lpi
) const
{
    //Debug(lpi.size());

    labelListList subMap;


    // Allocate transfer buffers
    PstreamBuffers pBufs(Pstream::nonBlocking);

    {
        // List of lists of particles to be transfered for all of the
        // neighbour processors
        List<IDLList<passiveParticle> > particleTransferLists
        (
            Pstream::nProcs()
        );

        // Per particle the destination processor
        labelList destProc(lpi.size());

        label particleI = 0;
        forAllIter(passiveParticleCloud, lpi, iter)
        {
            passiveParticle& ppi = iter();

            label destProcI = destinationProcID_[ppi.cell()];
            label destCellI = destinationCell_[ppi.cell()];

            ppi.cell() = destCellI;
            destProc[particleI++] = destProcI;
            //Pout<< "Sending particle:" << ppi << " to processor " << destProcI
            //    << " to cell " << destCellI << endl;
            particleTransferLists[destProcI].append(lpi.remove(&ppi));
        }


        // Per processor the indices of the particles to send
        subMap = invertOneToMany(Pstream::nProcs(), destProc);


        // Stream into send buffers
        forAll(particleTransferLists, procI)
        {
            //Pout<< "To proc " << procI << " sending "
            //    << particleTransferLists[procI] << endl;
            if (particleTransferLists[procI].size())
            {
                UOPstream particleStream(procI, pBufs);
                particleStream << particleTransferLists[procI];
            }
        }
    }


    // Start sending. Sets number of bytes transferred
    labelListList allNTrans(Pstream::nProcs());
    pBufs.finishedSends(allNTrans);


    {
        // Temporarily rename original cloud so we can construct a new one
        // (to distribute the positions) without getting a duplicate
        // registration warning
        const word cloudName = lpi.name();
        lpi.rename(cloudName + "_old");

        // New cloud on tgtMesh
        passiveParticleCloud lagrangianPositions
        (
            tgtMesh_,
            cloudName,
            IDLList<passiveParticle>()
        );


        // Retrieve from receive buffers
        forAll(allNTrans, procI)
        {
            label nRec = allNTrans[procI][Pstream::myProcNo()];

            //Pout<< "From processor " << procI << " receiving bytes " << nRec
            //    << endl;

            if (nRec)
            {
                UIPstream particleStream(procI, pBufs);

                IDLList<passiveParticle> newParticles
                (
                    particleStream,
                    passiveParticle::iNew(tgtMesh_)
                );

                forAllIter
                (
                    IDLList<passiveParticle>,
                    newParticles,
                    newpIter
                )
                {
                    passiveParticle& newp = newpIter();

                    lagrangianPositions.addParticle(newParticles.remove(&newp));
                }
            }
        }


        //OFstream::debug = 1;
        //Debug(lagrangianPositions.size());
        IOPosition<passiveParticleCloud>(lagrangianPositions).write();
        //OFstream::debug = 0;

        // Restore cloud name
        lpi.rename(cloudName);
    }

    // Work the send indices (subMap) into a mapDistributeBase
    labelListList sizes(Pstream::nProcs());
    labelList& nsTransPs = sizes[Pstream::myProcNo()];
    nsTransPs.setSize(Pstream::nProcs());
    forAll(subMap, sendProcI)
    {
        nsTransPs[sendProcI] = subMap[sendProcI].size();
    }
    // Send sizes across. Note: blocks.
    combineReduce(sizes, Pstream::listEq());

    labelListList constructMap(Pstream::nProcs());
    label constructSize = 0;
    forAll(constructMap, procI)
    {
        label nRecv = sizes[procI][UPstream::myProcNo()];

        labelList& map = constructMap[procI];

        map.setSize(nRecv);
        forAll(map, i)
        {
            map[i] = constructSize++;
        }
    }


    // Construct map
    return autoPtr<mapDistributeBase>
    (
        new mapDistributeBase
        (
            constructSize,
            subMap.xfer(),
            constructMap.xfer()
        )
    );
}


Foam::autoPtr<Foam::mapDistributeBase>
Foam::parLagrangianRedistributor::redistributeLagrangianPositions
(
    const word& cloudName
) const
{
    // Load cloud and send particle
    passiveParticleCloud lpi(srcMesh_, cloudName, false);

    return redistributeLagrangianPositions(lpi);
}


// ************************************************************************* //
