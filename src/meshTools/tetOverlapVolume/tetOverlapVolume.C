/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2012-2015 OpenFOAM Foundation
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

#include "tetOverlapVolume.H"
#include "tetrahedron.H"
#include "tetPoints.H"
#include "polyMesh.H"
#include "OFstream.H"
#include "treeBoundBox.H"
#include "indexedOctree.H"
#include "treeDataCell.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(tetOverlapVolume, 0);
}

// When to consider a tet to be zero volume. We want to avoid doing clipping
// against negative volume tets. Tet volume can be calculated incorrectly
// due to truncation errors. The value below works for single and double
// precision but could probably be tighter for double precision.
Foam::scalar Foam::tetOverlapVolume::minTetVolume_ = SMALL*SMALL;


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::tetOverlapVolume::tetOverlapVolume()
{}


// * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * * * //

Foam::treeBoundBox Foam::tetOverlapVolume::pyrBb
(
    const pointField& points,
    const face& f,
    const point& fc
)
{
    treeBoundBox bb(fc, fc);
    forAll(f, fp)
    {
        const point& pt = points[f[fp]];
        bb.min() = min(bb.min(), pt);
        bb.max() = max(bb.max(), pt);
    }
    return bb;
}


// * * * * * * * * * * * Public Member Functions  * * * * * * * * * * * * * //

bool Foam::tetOverlapVolume::cellCellOverlapMinDecomp
(
    const primitiveMesh& meshA,
    const label cellAI,
    const primitiveMesh& meshB,
    const label cellBI,
    const treeBoundBox& cellBbB,
    const scalar threshold
) const
{
    hasOverlapOp overlapCheckOp(threshold);
    cellCellOverlapMinDecomp<hasOverlapOp>
    (
        meshA,
        cellAI,
        meshB,
        cellBI,
        cellBbB,
        overlapCheckOp
    );

    return overlapCheckOp.ok_;
}


Foam::scalar Foam::tetOverlapVolume::cellCellOverlapVolumeMinDecomp
(
    const primitiveMesh& meshA,
    const label cellAI,

    const primitiveMesh& meshB,
    const label cellBI,
    const treeBoundBox& cellBbB
) const
{
    sumOverlapOp overlapSumOp;
    cellCellOverlapMinDecomp<sumOverlapOp>
    (
        meshA,
        cellAI,
        meshB,
        cellBI,
        cellBbB,
        overlapSumOp
    );

    return overlapSumOp.iop_.vol_;
}


Foam::Tuple2<Foam::scalar, Foam::point>
Foam::tetOverlapVolume::cellCellOverlapMomentMinDecomp
(
    const primitiveMesh& meshA,
    const label cellAI,

    const primitiveMesh& meshB,
    const label cellBI,
    const treeBoundBox& cellBbB
) const
{
    sumOverlapMomentOp overlapSumOp;
    cellCellOverlapMinDecomp<sumOverlapMomentOp>
    (
        meshA,
        cellAI,
        meshB,
        cellBI,
        cellBbB,
        overlapSumOp
    );

    return overlapSumOp.iop_.vol_;
}


Foam::labelList Foam::tetOverlapVolume::overlappingCells
(
    const polyMesh& fromMesh,
    const polyMesh& toMesh,
    const label iTo
) const
{
    const indexedOctree<treeDataCell>& treeA = fromMesh.cellTree();

    treeBoundBox bbB(toMesh.points(), toMesh.cellPoints()[iTo]);

    return treeA.findBox(bbB);
}


// ************************************************************************* //
