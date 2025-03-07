/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2013 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
    Copyright (C) 2008-2010 Mark Olesen
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

#include "vtkPV4Foam.H"

// OpenFOAM includes
#include "cellSet.H"
#include "faceSet.H"
#include "pointSet.H"
#include "fvMeshSubset.H"
#include "vtkPV4FoamReader.h"
#include "uindirectPrimitivePatch.H"

// VTK includes
#include "vtkDataArraySelection.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkPolyData.h"
#include "vtkUnstructuredGrid.h"

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::vtkPV4Foam::convertMeshVolume
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeVolume_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    // resize for decomposed polyhedra
    regionPolyDecomp_.setSize(range.size());

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshVolume" << endl;
        printMemory();
    }

    // Convert the internalMesh
    // this looks like more than one part, but it isn't
    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word partName = "internalMesh";

        if (!partStatus_[partId])
        {
            continue;
        }

        vtkUnstructuredGrid* vtkmesh = volumeVTKMesh
        (
            mesh,
            regionPolyDecomp_[datasetNo]
        );

        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, partName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshVolume" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshLagrangian
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeLagrangian_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshLagrangian" << endl;
        printMemory();
    }

    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word cloudName = getPartName(partId);

        if (!partStatus_[partId])
        {
            continue;
        }

        vtkPolyData* vtkmesh = lagrangianVTKMesh(mesh, cloudName);

        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, cloudName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshLagrangian" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshPatches
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangePatches_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;
    const polyBoundaryMesh& patches = mesh.boundaryMesh();

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshPatches" << endl;
        printMemory();
    }

    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        if (!partStatus_[partId])
        {
            continue;
        }

        const word patchName = getPartName(partId);

        labelHashSet
            patchIds(patches.patchSet(List<wordRe>(1, wordRe(patchName))));

        if (debug)
        {
            Info<< "Creating VTK mesh for patches [" << patchIds <<"] "
                << patchName << endl;
        }

        vtkPolyData* vtkmesh = NULL;
        if (patchIds.size() == 1)
        {
            vtkmesh = patchVTKMesh(patchName, patches[patchIds.begin().key()]);
        }
        else
        {
            // Patch group. Collect patch faces.
            label sz = 0;
            forAllConstIter(labelHashSet, patchIds, iter)
            {
                sz += patches[iter.key()].size();
            }
            labelList meshFaceLabels(sz);
            sz = 0;
            forAllConstIter(labelHashSet, patchIds, iter)
            {
                const polyPatch& pp = patches[iter.key()];
                forAll(pp, i)
                {
                    meshFaceLabels[sz++] = pp.start()+i;
                }
            }
            UIndirectList<face> fcs(mesh.faces(), meshFaceLabels);
            uindirectPrimitivePatch pp(fcs, mesh.points());

            vtkmesh = patchVTKMesh(patchName, pp);
        }


        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, patchName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshPatches" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshCellZones
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeCellZones_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    // resize for decomposed polyhedra
    zonePolyDecomp_.setSize(range.size());

    if (range.empty())
    {
        return;
    }

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshCellZones" << endl;
        printMemory();
    }

    const cellZoneMesh& zMesh = mesh.cellZones();
    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word zoneName = getPartName(partId);
        const label  zoneId = zMesh.findZoneID(zoneName);

        if (!partStatus_[partId] || zoneId < 0)
        {
            continue;
        }

        if (debug)
        {
            Info<< "Creating VTK mesh for cellZone[" << zoneId << "] "
                << zoneName << endl;
        }

        fvMeshSubset subsetter(mesh);
        subsetter.setLargeCellSubset(zMesh[zoneId]);

        vtkUnstructuredGrid* vtkmesh = volumeVTKMesh
        (
            subsetter.subMesh(),
            zonePolyDecomp_[datasetNo]
        );

        if (vtkmesh)
        {
            // superCells + addPointCellLabels must contain global cell ids
            inplaceRenumber
            (
                subsetter.cellMap(),
                zonePolyDecomp_[datasetNo].superCells()
            );
            inplaceRenumber
            (
                subsetter.cellMap(),
                zonePolyDecomp_[datasetNo].addPointCellLabels()
            );

            // copy pointMap as well, otherwise pointFields fail
            zonePolyDecomp_[datasetNo].pointMap() = subsetter.pointMap();

            AddToBlock(output, vtkmesh, range, datasetNo, zoneName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshCellZones" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshCellSets
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeCellSets_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    // resize for decomposed polyhedra
    csetPolyDecomp_.setSize(range.size());

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshCellSets" << endl;
        printMemory();
    }

    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word partName = getPartName(partId);

        if (!partStatus_[partId])
        {
            continue;
        }

        if (debug)
        {
            Info<< "Creating VTK mesh for cellSet=" << partName << endl;
        }

        const cellSet cSet(mesh, partName);
        fvMeshSubset subsetter(mesh);
        subsetter.setLargeCellSubset(cSet);

        vtkUnstructuredGrid* vtkmesh = volumeVTKMesh
        (
            subsetter.subMesh(),
            csetPolyDecomp_[datasetNo]
        );

        if (vtkmesh)
        {
            // superCells + addPointCellLabels must contain global cell ids
            inplaceRenumber
            (
                subsetter.cellMap(),
                csetPolyDecomp_[datasetNo].superCells()
            );
            inplaceRenumber
            (
                subsetter.cellMap(),
                csetPolyDecomp_[datasetNo].addPointCellLabels()
            );

            // copy pointMap as well, otherwise pointFields fail
            csetPolyDecomp_[datasetNo].pointMap() = subsetter.pointMap();

            AddToBlock(output, vtkmesh, range, datasetNo, partName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshCellSets" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshFaceZones
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeFaceZones_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    if (range.empty())
    {
        return;
    }

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshFaceZones" << endl;
        printMemory();
    }

    const faceZoneMesh& zMesh = mesh.faceZones();
    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word zoneName = getPartName(partId);
        const label  zoneId = zMesh.findZoneID(zoneName);

        if (!partStatus_[partId] || zoneId < 0)
        {
            continue;
        }

        if (debug)
        {
            Info<< "Creating VTKmesh for faceZone[" << zoneId << "] "
                << zoneName << endl;
        }

        vtkPolyData* vtkmesh = patchVTKMesh(zoneName, zMesh[zoneId]());

        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, zoneName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshFaceZones" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshFaceSets
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangeFaceSets_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshFaceSets" << endl;
        printMemory();
    }

    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        const word partName = getPartName(partId);

        if (!partStatus_[partId])
        {
            continue;
        }

        if (debug)
        {
            Info<< "Creating VTK mesh for faceSet=" << partName << endl;
        }

        const faceSet fSet(mesh, partName);

        vtkPolyData* vtkmesh = faceSetVTKMesh(mesh, fSet);
        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, partName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshFaceSets" << endl;
        printMemory();
    }
}


void Foam::vtkPV4Foam::convertMeshPointZones
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangePointZones_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshPointZones" << endl;
        printMemory();
    }

    if (range.size())
    {
        const pointZoneMesh& zMesh = mesh.pointZones();
        for (int partId = range.start(); partId < range.end(); ++partId)
        {
            word zoneName = getPartName(partId);
            label zoneId = zMesh.findZoneID(zoneName);

            if (!partStatus_[partId] || zoneId < 0)
            {
                continue;
            }

            vtkPolyData* vtkmesh = pointZoneVTKMesh(mesh, zMesh[zoneId]);
            if (vtkmesh)
            {
                AddToBlock(output, vtkmesh, range, datasetNo, zoneName);
                vtkmesh->Delete();

                partDataset_[partId] = datasetNo++;
            }
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshPointZones" << endl;
        printMemory();
    }
}



void Foam::vtkPV4Foam::convertMeshPointSets
(
    vtkMultiBlockDataSet* output,
    int& blockNo
)
{
    arrayRange& range = arrayRangePointSets_;
    range.block(blockNo);      // set output block
    label datasetNo = 0;       // restart at dataset 0
    const fvMesh& mesh = *meshPtr_;

    if (debug)
    {
        Info<< "<beg> Foam::vtkPV4Foam::convertMeshPointSets" << endl;
        printMemory();
    }

    for (int partId = range.start(); partId < range.end(); ++partId)
    {
        word partName = getPartName(partId);

        if (!partStatus_[partId])
        {
            continue;
        }

        if (debug)
        {
            Info<< "Creating VTK mesh for pointSet=" << partName << endl;
        }

        const pointSet pSet(mesh, partName);

        vtkPolyData* vtkmesh = pointSetVTKMesh(mesh, pSet);
        if (vtkmesh)
        {
            AddToBlock(output, vtkmesh, range, datasetNo, partName);
            vtkmesh->Delete();

            partDataset_[partId] = datasetNo++;
        }
    }

    // anything added?
    if (datasetNo)
    {
        ++blockNo;
    }

    if (debug)
    {
        Info<< "<end> Foam::vtkPV4Foam::convertMeshPointSets" << endl;
        printMemory();
    }
}


// ************************************************************************* //
