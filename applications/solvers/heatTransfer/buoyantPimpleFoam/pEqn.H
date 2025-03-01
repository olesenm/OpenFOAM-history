{
    bool closedVolume = p_rgh.needReference();

    rho = thermo.rho();

    // Thermodynamic density needs to be updated by psi*d(p) after the
    // pressure solution - done in 2 parts. Part 1:
    thermo.rho() -= psi*p_rgh;

    volScalarField rAU(1.0/UEqn.A());
    surfaceScalarField rAUf("rAUf", fvc::interpolate(rho*rAU));

    volVectorField HbyA("HbyA", U);
    HbyA = rAU*UEqn.H();

    surfaceScalarField phig(-rAUf*ghf*fvc::snGrad(rho)*mesh.magSf());

    surfaceScalarField phiHbyA
    (
        "phiHbyA",
        (
            (fvc::interpolate(rho*HbyA) & mesh.Sf())
          + rAUf*fvc::ddtCorr(rho, U, phi)
        )
      + phig
    );

    fvOptions.makeRelative(fvc::interpolate(rho), phiHbyA);

    // Update the fixedFluxPressure BCs to ensure flux consistency
    setSnGrad<fixedFluxPressureFvPatchScalarField>
    (
        p_rgh.boundaryField(),
        (
            phiHbyA.boundaryField()
          - fvOptions.relative(mesh.Sf().boundaryField() & U.boundaryField())
           *rho.boundaryField()
        )/(mesh.magSf().boundaryField()*rAUf.boundaryField())
    );

    tmp<fvScalarMatrix> p_rghDDtEqn
    (
        new fvScalarMatrix(p_rgh, dimMass/dimTime)
    );

    if (compressible)
    {
        p_rghDDtEqn =
        (
            fvc::ddt(rho) + psi*correction(fvm::ddt(p_rgh))
        ==
            fvOptions(psi, p_rgh, rho.name())
        );
    }

    while (pimple.correctNonOrthogonal())
    {
        fvScalarMatrix p_rghEqn
        (
            p_rghDDtEqn()
          + fvc::div(phiHbyA)
          - fvm::laplacian(rAUf, p_rgh)
        );

        p_rghEqn.solve(mesh.solver(p_rgh.select(pimple.finalInnerIter())));

        if (pimple.finalNonOrthogonalIter())
        {
            // Calculate the conservative fluxes
            phi = phiHbyA + p_rghEqn.flux();

            // Explicitly relax pressure for momentum corrector
            p_rgh.relax();

            // Correct the momentum source with the pressure gradient flux
            // calculated from the relaxed pressure
            U = HbyA + rAU*fvc::reconstruct((phig + p_rghEqn.flux())/rAUf);
            U.correctBoundaryConditions();
            fvOptions.correct(U);
            K = 0.5*magSqr(U);
        }
    }

    p = p_rgh + rho*gh;

    // Second part of thermodynamic density update
    thermo.rho() += psi*p_rgh;

    if (thermo.dpdt())
    {
        dpdt = fvc::ddt(p);
    }

    if (compressible)
    {
        #include "rhoEqn.H"
    }
    #include "compressibleContinuityErrs.H"

    if (closedVolume)
    {
        if (!compressible)
        {
            p += dimensionedScalar
            (
                "p",
                p.dimensions(),
                pRefValue - getRefCellValue(p, pRefCell)
            );
            p_rgh = p - rho*gh;
        }
        else
        {
            p += (initialMass - fvc::domainIntegrate(thermo.rho()))
                /compressibility;
            rho = thermo.rho();
            p_rgh = p - rho*gh;
        }
    }

    Info<< "rho max/min : " << max(rho).value() << " " << min(rho).value()
        << endl;
}
