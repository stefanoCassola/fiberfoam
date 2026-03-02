//Declare variables needed for permeability calculation
string mainDir;
scalar mainFlowDir;
scalar secondaryFlowDir;
scalar tertiaryFlowDir;
scalar flowLength;
scalar flowCrossArea;
scalar domainVolume;
scalar density;
scalar nu;
scalar pIn;
scalar pOut;
scalar meshVol;
scalar fvc;


if (runTime.timeIndex()== (runTime.startTime().value()+1)) {

    Info<<"-----------------------------------------------------------"<<endl;
    Info<<"Retrieving information for permeability calculation..."<<endl;
    Info<<"-----------------------------------------------------------"<<nl<<endl;

    // --- Robust flow direction detection based on outlet patch orientation ---
    label outletPatchID = mesh.boundaryMesh().findPatchID("outlet");

    if (outletPatchID == -1) {
        FatalErrorInFunction
            << "No patch named 'outlet' found in boundary file!" << nl
            << "Cannot determine flow direction automatically." << exit(FatalError);
    }

    const polyPatch& outletPatch = mesh.boundaryMesh()[outletPatchID];
    const vectorField& faceAreas = outletPatch.faceAreas();

    // Compute average outlet normal vector
    vector avgNormal = Foam::sum(faceAreas) / mag(Foam::sum(faceAreas));
    avgNormal /= mag(avgNormal);  // normalize to unit vector

    scalar absX = mag(avgNormal.x());
    scalar absY = mag(avgNormal.y());
    scalar absZ = mag(avgNormal.z());

    if (absX >= absY && absX >= absZ) {
        mainDir = "x";
        mainFlowDir = 0;
        secondaryFlowDir = 1;
        tertiaryFlowDir = 2;
    }
    else if (absY >= absX && absY >= absZ) {
        mainDir = "y";
        mainFlowDir = 1;
        secondaryFlowDir = 2;
        tertiaryFlowDir = 0;
    }
    else {
        mainDir = "z";
        mainFlowDir = 2;
        secondaryFlowDir = 0;
        tertiaryFlowDir = 1;
    }

    Info<<"Detected main flow direction from outlet patch orientation: "<<mainDir<<nl<<endl;

    // --- Geometry and fluid properties ---
    flowLength = mesh.bounds().max()[mainFlowDir] - mesh.bounds().min()[mainFlowDir];
    flowCrossArea = (mesh.bounds().max()[secondaryFlowDir] - mesh.bounds().min()[secondaryFlowDir]) 
                  * (mesh.bounds().max()[tertiaryFlowDir] - mesh.bounds().min()[tertiaryFlowDir]);
    domainVolume = flowLength * flowCrossArea;
    density = (readScalar(transportProperties.lookup("density")));
    nu = (readScalar(transportProperties.lookup("nu")));
    pIn = (readScalar(transportProperties.lookup("pIn")));
    pOut = (readScalar(transportProperties.lookup("pOut")));
    meshVol = sum(mesh.V()).value();
    fvc = (1 - (meshVol / (flowLength * flowCrossArea))) * 100;

    // --- Output info ---
    Info<<"The main flow direction is "<<mainDir<<"!"<<nl<<endl;
    Info<<"The flow length is: "<<flowLength<<" m"<<endl;
    Info<<"The flow crosssectional area is: "<<flowCrossArea<<" m²"<<endl;
    Info<<"The domain volume is: "<<domainVolume<<" m³"<<endl;
    Info<<"The total cell volume is: "<<meshVol<<" m³"<<endl;
    Info<<"The fluid density is: "<<density<<" kg/m³"<<endl;
    Info<<"The kinematic viscosity of the fluid is: "<<nu<<" m²/s"<<endl;
    Info<<"The inlet pressure is: "<<pIn<<" Pa"<<endl;
    Info<<"The outlet pressure: "<<pOut<<" Pa"<<endl;
    Info<<"The fiber volume content is : "<<fvc<<" %"<<nl<<endl;

    Info<<"-----------------------------------------------------------"<<nl<<endl;
};

Info<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl
    <<"++++++++++++++++++++++++++++++++++++++++Permeability outputs++++++++++++++++++++++++++++++++++++++++++"<<nl<<endl;

//Output for volume averaged velocity
Info<<"Average velocities in x y z direction "<<nl<<nl
    <<"     - "<<U.average().value()<<" m/s"<<nl<<endl;

//Permeability with average velocity
scalar permVolAvgUmain =  -((U.average()[mainFlowDir].value()*(meshVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);
scalar permVolAvgUsec  =  -((U.average()[secondaryFlowDir].value()*(meshVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);
scalar permVolAvgUtert =  -((U.average()[tertiaryFlowDir].value()*(meshVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);

Info<<"The permeabilities calculated with the volume averaged velocities: "<<nl<<nl
    <<"     - Main flow direction: "<<permVolAvgUmain<<" m²"<<nl
    <<"     - Secondary flow direction: "<<permVolAvgUsec<<" m²"<<nl
    <<"     - Tertiary flow direction: "<<permVolAvgUtert<<" m²"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;

//Flow rate and permeability calculation with flow rate through outlet
int indexOutlet = mesh.boundaryMesh().findPatchID("outlet");
Info<<"The volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<sum(phi.boundaryField()[indexOutlet])<<" m³/s"<<nl<<endl;

double permFlowRate = -((sum(phi.boundaryField()[indexOutlet])/flowCrossArea)*nu*density*flowLength)/(pOut-pIn);
Info<<"The permeability calculated with the volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<permFlowRate<<" m²"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;
