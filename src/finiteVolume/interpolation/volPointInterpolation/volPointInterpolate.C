/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
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

#include "volPointInterpolation.H"
#include "volFields.H"
#include "pointFields.H"
#include "emptyFvPatch.H"
#include "coupledPointPatchField.H"
#include "pointConstraints.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
void volPointInterpolation::pushUntransformedData
(
    List<Type>& pointData
) const
{
    // Transfer onto coupled patch
    const globalMeshData& gmd = mesh().globalData();
    const indirectPrimitivePatch& cpp = gmd.coupledPatch();
    const labelList& meshPoints = cpp.meshPoints();

    const mapDistribute& slavesMap = gmd.globalCoPointSlavesMap();
    const labelListList& slaves = gmd.globalCoPointSlaves();

    List<Type> elems(slavesMap.constructSize());
    forAll(meshPoints, i)
    {
        elems[i] = pointData[meshPoints[i]];
    }

    // Combine master data with slave data
    forAll(slaves, i)
    {
        const labelList& slavePoints = slaves[i];

        // Copy master data to slave slots
        forAll(slavePoints, j)
        {
            elems[slavePoints[j]] = elems[i];
        }
    }

    // Push slave-slot data back to slaves
    slavesMap.reverseDistribute(elems.size(), elems, false);

    // Extract back onto mesh
    forAll(meshPoints, i)
    {
        pointData[meshPoints[i]] = elems[i];
    }
}


template<class Type>
void volPointInterpolation::addSeparated
(
    GeometricField<Type, pointPatchField, pointMesh>& pf
) const
{
    if (debug)
    {
        Pout<< "volPointInterpolation::addSeparated" << endl;
    }

    forAll(pf.boundaryField(), patchI)
    {
        if (pf.boundaryField()[patchI].coupled())
        {
            refCast<coupledPointPatchField<Type> >
                (pf.boundaryField()[patchI]).initSwapAddSeparated
                (
                    Pstream::nonBlocking,
                    pf.internalField()
                );
        }
    }

    // Block for any outstanding requests
    Pstream::waitRequests();

    forAll(pf.boundaryField(), patchI)
    {
        if (pf.boundaryField()[patchI].coupled())
        {
            refCast<coupledPointPatchField<Type> >
                (pf.boundaryField()[patchI]).swapAddSeparated
                (
                    Pstream::nonBlocking,
                    pf.internalField()
                );
        }
    }
}


template<class Type>
void volPointInterpolation::interpolateInternalField
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    GeometricField<Type, pointPatchField, pointMesh>& pf
) const
{
    if (debug)
    {
        Pout<< "volPointInterpolation::interpolateInternalField("
            << "const GeometricField<Type, fvPatchField, volMesh>&, "
            << "GeometricField<Type, pointPatchField, pointMesh>&) : "
            << "interpolating field from cells to points"
            << endl;
    }

    const labelListList& pointCells = vf.mesh().pointCells();

    // Multiply volField by weighting factor matrix to create pointField
    forAll(pointCells, pointi)
    {
        if (!isPatchPoint_[pointi])
        {
            const scalarList& pw = pointWeights_[pointi];
            const labelList& ppc = pointCells[pointi];

            pf[pointi] = pTraits<Type>::zero;

            forAll(ppc, pointCelli)
            {
                pf[pointi] += pw[pointCelli]*vf[ppc[pointCelli]];
            }
        }
    }
}


template<class Type>
void volPointInterpolation::interpolateDimensionedInternalField
(
    const DimensionedField<Type, volMesh>& vf,
    DimensionedField<Type, pointMesh>& pf
) const
{
    if (debug)
    {
        Pout<< "volPointInterpolation::interpolateDimensionedInternalField("
            << "const DimensionedField<Type, volMesh>&, "
            << "DimensionedField<Type, pointMesh>&) : "
            << "interpolating field from cells to points"
            << endl;
    }

    const fvMesh& mesh = vf.mesh();

    const labelListList& pointCells = mesh.pointCells();
    const pointField& points = mesh.points();
    const vectorField& cellCentres = mesh.cellCentres();

    // Re-do weights and interpolation since normal interpolation
    // pointWeights_ are for non-boundary points only. Not efficient but
    // then saves on space.

    // Multiply volField by weighting factor matrix to create pointField
    scalarField sumW(points.size(), 0.0);
    forAll(pointCells, pointi)
    {
        const labelList& ppc = pointCells[pointi];

        pf[pointi] = pTraits<Type>::zero;

        forAll(ppc, pointCelli)
        {
            label celli = ppc[pointCelli];
            scalar pw = 1.0/mag(points[pointi] - cellCentres[celli]);

            pf[pointi] += pw*vf[celli];
            sumW[pointi] += pw;
        }
    }

    // Sum collocated contributions
    pointConstraints::syncUntransformedData(mesh, sumW, plusEqOp<scalar>());
    pointConstraints::syncUntransformedData(mesh, pf, plusEqOp<Type>());

    // Normalise
    forAll(pf, pointi)
    {
        scalar s = sumW[pointi];
        if (s > ROOTVSMALL)
        {
            pf[pointi] /= s;
        }
    }
}


template<class Type>
tmp<Field<Type> > volPointInterpolation::flatBoundaryField
(
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    const fvMesh& mesh = vf.mesh();
    const fvBoundaryMesh& bm = mesh.boundary();

    tmp<Field<Type> > tboundaryVals
    (
        new Field<Type>(mesh.nFaces()-mesh.nInternalFaces())
    );
    Field<Type>& boundaryVals = tboundaryVals();

    forAll(vf.boundaryField(), patchI)
    {
        label bFaceI = bm[patchI].patch().start() - mesh.nInternalFaces();

        if
        (
           !isA<emptyFvPatch>(bm[patchI])
        && !vf.boundaryField()[patchI].coupled()
        )
        {
            SubList<Type>
            (
                boundaryVals,
                vf.boundaryField()[patchI].size(),
                bFaceI
            ).assign(vf.boundaryField()[patchI]);
        }
        else
        {
            const polyPatch& pp = bm[patchI].patch();

            forAll(pp, i)
            {
                boundaryVals[bFaceI++] = pTraits<Type>::zero;
            }
        }
    }

    return tboundaryVals;
}


template<class Type>
void volPointInterpolation::interpolateBoundaryField
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    GeometricField<Type, pointPatchField, pointMesh>& pf
) const
{
    const primitivePatch& boundary = boundaryPtr_();

    Field<Type>& pfi = pf.internalField();

    // Get face data in flat list
    tmp<Field<Type> > tboundaryVals(flatBoundaryField(vf));
    const Field<Type>& boundaryVals = tboundaryVals();


    // Do points on 'normal' patches from the surrounding patch faces
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    forAll(boundary.meshPoints(), i)
    {
        label pointI = boundary.meshPoints()[i];

        if (isPatchPoint_[pointI])
        {
            const labelList& pFaces = boundary.pointFaces()[i];
            const scalarList& pWeights = boundaryPointWeights_[i];

            Type& val = pfi[pointI];

            val = pTraits<Type>::zero;
            forAll(pFaces, j)
            {
                if (boundaryIsPatchFace_[pFaces[j]])
                {
                    val += pWeights[j]*boundaryVals[pFaces[j]];
                }
            }
        }
    }

    // Sum collocated contributions
    pointConstraints::syncUntransformedData(mesh(), pfi, plusEqOp<Type>());

    // And add separated contributions
    addSeparated(pf);

    // Push master data to slaves. It is possible (not sure how often) for
    // a coupled point to have its master on a different patch so
    // to make sure just push master data to slaves.
    pushUntransformedData(pfi);
}


template<class Type>
void volPointInterpolation::interpolateBoundaryField
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    GeometricField<Type, pointPatchField, pointMesh>& pf,
    const bool overrideFixedValue
) const
{
    interpolateBoundaryField(vf, pf);

    // Apply constraints
    const pointConstraints& pcs = pointConstraints::New(pf.mesh());

    pcs.constrain(pf, overrideFixedValue);
}


template<class Type>
void volPointInterpolation::interpolate
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    GeometricField<Type, pointPatchField, pointMesh>& pf
) const
{
    if (debug)
    {
        Pout<< "volPointInterpolation::interpolate("
            << "const GeometricField<Type, fvPatchField, volMesh>&, "
            << "GeometricField<Type, pointPatchField, pointMesh>&) : "
            << "interpolating field from cells to points"
            << endl;
    }

    interpolateInternalField(vf, pf);

    // Interpolate to the patches preserving fixed value BCs
    interpolateBoundaryField(vf, pf, false);
}


template<class Type>
tmp<GeometricField<Type, pointPatchField, pointMesh> >
volPointInterpolation::interpolate
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    const wordList& patchFieldTypes
) const
{
    const pointMesh& pm = pointMesh::New(vf.mesh());

    // Construct tmp<pointField>
    tmp<GeometricField<Type, pointPatchField, pointMesh> > tpf
    (
        new GeometricField<Type, pointPatchField, pointMesh>
        (
            IOobject
            (
                "volPointInterpolate(" + vf.name() + ')',
                vf.instance(),
                pm.thisDb()
            ),
            pm,
            vf.dimensions(),
            patchFieldTypes
        )
    );

    interpolateInternalField(vf, tpf());

    // Interpolate to the patches overriding fixed value BCs
    interpolateBoundaryField(vf, tpf(), true);

    return tpf;
}


template<class Type>
tmp<GeometricField<Type, pointPatchField, pointMesh> >
volPointInterpolation::interpolate
(
    const tmp<GeometricField<Type, fvPatchField, volMesh> >& tvf,
    const wordList& patchFieldTypes
) const
{
    // Construct tmp<pointField>
    tmp<GeometricField<Type, pointPatchField, pointMesh> > tpf =
        interpolate(tvf(), patchFieldTypes);
    tvf.clear();
    return tpf;
}


template<class Type>
tmp<GeometricField<Type, pointPatchField, pointMesh> >
volPointInterpolation::interpolate
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    const word& name,
    const bool cache
) const
{
    typedef GeometricField<Type, pointPatchField, pointMesh> PointFieldType;

    const pointMesh& pm = pointMesh::New(vf.mesh());
    const objectRegistry& db = pm.thisDb();

    if (!cache || vf.mesh().changing())
    {
        // Delete any old occurences to avoid double registration
        if (db.objectRegistry::template foundObject<PointFieldType>(name))
        {
            PointFieldType& pf = const_cast<PointFieldType&>
            (
                db.objectRegistry::template lookupObject<PointFieldType>(name)
            );

            if (pf.ownedByRegistry())
            {
                solution::cachePrintMessage("Deleting", name, vf);
                pf.release();
                delete &pf;
            }
        }


        tmp<GeometricField<Type, pointPatchField, pointMesh> > tpf
        (
            new GeometricField<Type, pointPatchField, pointMesh>
            (
                IOobject
                (
                    name,
                    vf.instance(),
                    pm.thisDb()
                ),
                pm,
                vf.dimensions()
            )
        );

        interpolate(vf, tpf());

        return tpf;
    }
    else
    {
        if (!db.objectRegistry::template foundObject<PointFieldType>(name))
        {
            solution::cachePrintMessage("Calculating and caching", name, vf);
            tmp<PointFieldType> tpf = interpolate(vf, name, false);
            PointFieldType* pfPtr = tpf.ptr();
            regIOobject::store(pfPtr);
            return *pfPtr;
        }
        else
        {
            PointFieldType& pf = const_cast<PointFieldType&>
            (
                db.objectRegistry::template lookupObject<PointFieldType>(name)
            );

            if (pf.upToDate(vf))    //TBD: , vf.mesh().points()))
            {
                solution::cachePrintMessage("Reusing", name, vf);
            }
            else
            {
                solution::cachePrintMessage("Updating", name, vf);
                interpolate(vf, pf);
            }
            return pf;
        }
    }
}


template<class Type>
tmp<GeometricField<Type, pointPatchField, pointMesh> >
volPointInterpolation::interpolate
(
    const GeometricField<Type, fvPatchField, volMesh>& vf
) const
{
    return interpolate(vf, "volPointInterpolate(" + vf.name() + ')', false);
}


template<class Type>
tmp<GeometricField<Type, pointPatchField, pointMesh> >
volPointInterpolation::interpolate
(
    const tmp<GeometricField<Type, fvPatchField, volMesh> >& tvf
) const
{
    // Construct tmp<pointField>
    tmp<GeometricField<Type, pointPatchField, pointMesh> > tpf =
        interpolate(tvf());
    tvf.clear();
    return tpf;
}


template<class Type>
tmp<DimensionedField<Type, pointMesh> >
volPointInterpolation::interpolate
(
    const DimensionedField<Type, volMesh>& vf,
    const word& name,
    const bool cache
) const
{
    typedef DimensionedField<Type, pointMesh> PointFieldType;

    const pointMesh& pm = pointMesh::New(vf.mesh());
    const objectRegistry& db = pm.thisDb();

    if (!cache || vf.mesh().changing())
    {
        // Delete any old occurences to avoid double registration
        if (db.objectRegistry::template foundObject<PointFieldType>(name))
        {
            PointFieldType& pf = const_cast<PointFieldType&>
            (
                db.objectRegistry::template lookupObject<PointFieldType>(name)
            );

            if (pf.ownedByRegistry())
            {
                solution::cachePrintMessage("Deleting", name, vf);
                pf.release();
                delete &pf;
            }
        }


        tmp<DimensionedField<Type, pointMesh> > tpf
        (
            new DimensionedField<Type, pointMesh>
            (
                IOobject
                (
                    name,
                    vf.instance(),
                    pm.thisDb()
                ),
                pm,
                vf.dimensions()
            )
        );

        interpolateDimensionedInternalField(vf, tpf());

        return tpf;
    }
    else
    {
        if (!db.objectRegistry::template foundObject<PointFieldType>(name))
        {
            solution::cachePrintMessage("Calculating and caching", name, vf);
            tmp<PointFieldType> tpf = interpolate(vf, name, false);
            PointFieldType* pfPtr = tpf.ptr();
            regIOobject::store(pfPtr);
            return *pfPtr;
        }
        else
        {
            PointFieldType& pf = const_cast<PointFieldType&>
            (
                db.objectRegistry::template lookupObject<PointFieldType>(name)
            );

            if (pf.upToDate(vf))    //TBD: , vf.mesh().points()))
            {
                solution::cachePrintMessage("Reusing", name, vf);
            }
            else
            {
                solution::cachePrintMessage("Updating", name, vf);
                interpolateDimensionedInternalField(vf, pf);
            }

            return pf;
        }
    }
}


template<class Type>
tmp<DimensionedField<Type, pointMesh> >
volPointInterpolation::interpolate
(
    const DimensionedField<Type, volMesh>& vf
) const
{
    return interpolate(vf, "volPointInterpolate(" + vf.name() + ')', false);
}


template<class Type>
tmp<DimensionedField<Type, pointMesh> >
volPointInterpolation::interpolate
(
    const tmp<DimensionedField<Type, volMesh> >& tvf
) const
{
    // Construct tmp<pointField>
    tmp<DimensionedField<Type, pointMesh> > tpf = interpolate(tvf());
    tvf.clear();
    return tpf;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
