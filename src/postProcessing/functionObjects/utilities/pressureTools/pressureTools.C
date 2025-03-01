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

#include "pressureTools.H"
#include "volFields.H"
#include "dictionary.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(pressureTools, 0);
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

Foam::dimensionedScalar Foam::pressureTools::rhoScale
(
    const volScalarField& p
) const
{
    if (p.dimensions() == dimPressure)
    {
        return dimensionedScalar("1", dimless,  1.0);
    }
    else
    {
        return dimensionedScalar("rhoRef", dimDensity, rhoInf_);
    }
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::rho
(
    const volScalarField& p
) const
{
    if (p.dimensions() == dimPressure)
    {
        return p.mesh().lookupObject<volScalarField>(rhoName_);
    }
    else
    {
        if (!rhoInfInitialised_)
        {
            FatalErrorIn
            (
                "Foam::tmp<Foam::volScalarField> Foam::pressureTools::rho"
                "("
                "    const volScalarField&"
                ") const"
            )
                << type() << " " << name_ << ": "
                << "pressure identified as incompressible, but reference "
                << "density is not set.  Please set rhoName to rhoInf, and "
                << "set an appropriate value for rhoInf"
                << exit(FatalError);
        }

       return
            tmp<volScalarField>
            (
                new volScalarField
                (
                    IOobject
                    (
                        "rho",
                        p.mesh().time().timeName(),
                        p.mesh(),
                        IOobject::NO_READ,
                        IOobject::NO_WRITE
                    ),
                    p.mesh(),
                    dimensionedScalar("zero", dimDensity, rhoInf_)
                )
            );
    }
}


Foam::dimensionedScalar Foam::pressureTools::pRef() const
{
    dimensionedScalar value("pRef", dimPressure, 0.0);

    if (calcTotal_)
    {
        value.value() += pRef_;
    }

    return value;
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::pDyn
(
    const volScalarField& p
) const
{
    const fvMesh& mesh = refCast<const fvMesh>(obr_);

    tmp<volScalarField> tpDyn
    (
        new volScalarField
        (
            IOobject
            (
                "pDyn",
                mesh.time().timeName(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensionedScalar("zero", dimPressure, 0.0)
        )
    );

    if (calcTotal_)
    {
        const volVectorField& U = obr_.lookupObject<volVectorField>(UName_);

        tpDyn() == rho(p)*0.5*magSqr(U);
    }

    return tpDyn;
}


Foam::tmp<Foam::volScalarField> Foam::pressureTools::convertToCoeff
(
    const volScalarField& p
) const
{
    tmp<volScalarField> tCoeff(p);

    if (calcCoeff_)
    {
        tCoeff() -= dimensionedScalar("pInf", dimPressure, pInf_);

        const dimensionedScalar p0("p0", dimPressure, SMALL);
        const dimensionedVector U("U", dimVelocity, UInf_);
        const dimensionedScalar rho("rho", dimDensity, rhoInf_);

        tCoeff() /= 0.5*rho*magSqr(U) + p0;
    }

    return tCoeff;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::pressureTools::pressureTools
(
    const word& name,
    const objectRegistry& obr,
    const dictionary& dict,
    const bool loadFromFiles
)
:
    name_(name),
    obr_(obr),
    active_(true),
    pName_("p"),
    UName_("U"),
    rhoName_("rho"),
    resultName_(word::null),
    log_(true),
    calcTotal_(false),
    pRef_(0.0),
    calcCoeff_(false),
    pInf_(0.0),
    UInf_(vector::zero),
    rhoInf_(0.0),
    rhoInfInitialised_(false)
{
    // Check if the available mesh is an fvMesh, otherwise deactivate
    if (!isA<fvMesh>(obr_))
    {
        active_ = false;
        WarningIn
        (
            "pressureTools::pressureTools"
            "("
                "const word&, "
                "const objectRegistry&, "
                "const dictionary&, "
                "const bool"
            ")"
        )   << "No fvMesh available, deactivating " << name_ << nl
            << endl;
    }

    read(dict);

    if (active_)
    {
        dimensionSet pDims(dimPressure);

        if (calcCoeff_)
        {
            pDims /= dimPressure;
        }

        const fvMesh& mesh = refCast<const fvMesh>(obr_);

        volScalarField* pPtr
        (
            new volScalarField
            (
                IOobject
                (
                    resultName_,
                    mesh.time().timeName(),
                    mesh,
                    IOobject::NO_READ,
                    IOobject::NO_WRITE
                ),
                mesh,
                dimensionedScalar("0", pDims, 0.0)
            )
        );

        mesh.objectRegistry::store(pPtr);
    }
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::pressureTools::~pressureTools()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::pressureTools::read(const dictionary& dict)
{
    if (active_)
    {
        log_.readIfPresent("log", dict);

        dict.readIfPresent("pName", pName_);
        dict.readIfPresent("UName", UName_);
        dict.readIfPresent("rhoName", rhoName_);

        rhoInfInitialised_ = false;

        if (rhoName_ == "rhoInf")
        {
            dict.lookup("rhoInf") >> rhoInf_;
            rhoInfInitialised_ = true;
        }

        dict.lookup("calcTotal") >> calcTotal_;
        if (calcTotal_)
        {
            dict.lookup("pRef") >> pRef_;
        }

        dict.lookup("calcCoeff") >> calcCoeff_;
        if (calcCoeff_)
        {
            dict.lookup("pInf") >> pInf_;
            dict.lookup("UInf") >> UInf_;
            dict.lookup("rhoInf") >> rhoInf_;

            scalar zeroCheck = 0.5*rhoInf_*magSqr(UInf_) + pInf_;

            if (mag(zeroCheck) < ROOTVSMALL)
            {
                WarningIn("void Foam::pressureTools::read(const dictionary&)")
                    << type() << " " << name_ << ": "
                    << "Coefficient calculation requested, but reference "
                    << "pressure level is zero.  Please check the supplied "
                    << "values of pInf, UInf and rhoInf" << endl;
            }

            rhoInfInitialised_ = true;
        }

        if (!dict.readIfPresent("resultName", resultName_))
        {
            resultName_ = pName_;

            if (calcTotal_)
            {
                resultName_ = "total(" + resultName_ + ")";
            }
            else
            {
                resultName_ = "static(" + resultName_ + ")";
            }

            if (calcCoeff_)
            {
                resultName_ = resultName_ + "_coeff";
            }
        }
    }
}


void Foam::pressureTools::execute()
{
    if (active_)
    {
        const volScalarField& p = obr_.lookupObject<volScalarField>(pName_);

        volScalarField& pResult =
            const_cast<volScalarField&>
            (
                obr_.lookupObject<volScalarField>(resultName_)
            );

        pResult == convertToCoeff(rhoScale(p)*p + pDyn(p) + pRef());
    }
}


void Foam::pressureTools::end()
{
    if (active_)
    {
        execute();
    }
}


void Foam::pressureTools::timeSet()
{
    // Do nothing
}


void Foam::pressureTools::write()
{
    if (active_)
    {
        const volScalarField& pResult =
            obr_.lookupObject<volScalarField>(resultName_);

        Info(log_)
            << type() << " " << name_ << " output:" << nl
            << "    writing field " << pResult.name() << nl
            << endl;

        pResult.write();
    }
}


// ************************************************************************* //
