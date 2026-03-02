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
scalar roiMin;
scalar roiMax;


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
    density = (readScalar(transportProperties.lookup("density")));
    nu = (readScalar(transportProperties.lookup("nu")));
    pIn = (readScalar(transportProperties.lookup("pIn")));
    pOut = (readScalar(transportProperties.lookup("pOut")));

    // --- Buffer zone exclusion (read from blockMeshDict) ---
    scalar inletBuf = readScalar(blockMeshDict.lookup("inlet_length"));
    scalar outletBuf = readScalar(blockMeshDict.lookup("outlet_length"));
    scalar voxelScale = readScalar(blockMeshDict.lookup("scale"));
    scalar bufferLength = (inletBuf + outletBuf) * voxelScale;

    // Subtract buffer zone length from flow length
    flowLength -= bufferLength;

    // Define ROI (region of interest) bounds excluding buffer zones
    roiMin = mesh.bounds().min()[mainFlowDir] + inletBuf * voxelScale;
    roiMax = mesh.bounds().max()[mainFlowDir] - outletBuf * voxelScale;

    // Domain volume of the fibrous ROI
    domainVolume = flowLength * flowCrossArea;

    // Compute mesh volume within ROI only
    meshVol = 0;
    {
        const volVectorField& cellCenters = mesh.C();
        forAll(cellCenters, cellI)
        {
            scalar pos = cellCenters[cellI][mainFlowDir];
            if (pos >= roiMin && pos < roiMax)
            {
                meshVol += mesh.V()[cellI];
            }
        }
    }

    fvc = (1 - (meshVol / domainVolume)) * 100;

    // --- Output info ---
    Info<<"The main flow direction is "<<mainDir<<"!"<<nl<<endl;
    Info<<"The flow length is: "<<flowLength<<" m"<<endl;
    if (bufferLength > 0)
    {
        Info<<"Buffer zones excluded: inlet="<<inletBuf<<" outlet="<<outletBuf
            <<" voxels (total "<<bufferLength<<" m)"<<endl;
        Info<<"ROI bounds along "<<mainDir<<": ["<<roiMin<<", "<<roiMax<<"]"<<endl;
    }
    Info<<"The flow crosssectional area is: "<<flowCrossArea<<" m²"<<endl;
    Info<<"The domain volume (ROI): "<<domainVolume<<" m³"<<endl;
    Info<<"The total cell volume (ROI): "<<meshVol<<" m³"<<endl;
    Info<<"The fluid density is: "<<density<<" kg/m³"<<endl;
    Info<<"The kinematic viscosity of the fluid is: "<<nu<<" m²/s"<<endl;
    Info<<"The inlet pressure is: "<<pIn<<" Pa"<<endl;
    Info<<"The outlet pressure: "<<pOut<<" Pa"<<endl;
    Info<<"The fiber volume content is : "<<fvc<<" %"<<nl<<endl;

    Info<<"-----------------------------------------------------------"<<nl<<endl;
};

Info<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl
    <<"++++++++++++++++++++++++++++++++++++++++Permeability outputs++++++++++++++++++++++++++++++++++++++++++"<<nl<<endl;

// Compute volume-averaged velocity over fibrous ROI only
scalar roiVol = 0;
vector roiWeightedU = vector::zero;
{
    const volVectorField& cellCenters = mesh.C();
    forAll(cellCenters, cellI)
    {
        scalar pos = cellCenters[cellI][mainFlowDir];
        if (pos >= roiMin && pos < roiMax)
        {
            scalar vol = mesh.V()[cellI];
            roiWeightedU += U[cellI] * vol;
            roiVol += vol;
        }
    }
}
vector roiAvgU = roiWeightedU / roiVol;

//Output for volume averaged velocity
Info<<"Average velocities in x y z direction (ROI) "<<nl<<nl
    <<"     - "<<roiAvgU<<" m/s"<<nl<<endl;

//Permeability with average velocity
scalar permVolAvgUmain =  -((roiAvgU[mainFlowDir]*(roiVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);
scalar permVolAvgUsec  =  -((roiAvgU[secondaryFlowDir]*(roiVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);
scalar permVolAvgUtert =  -((roiAvgU[tertiaryFlowDir]*(roiVol/domainVolume))*nu*density*flowLength) / (pOut-pIn);

Info<<"The permeabilities calculated with the volume averaged velocities: "<<nl<<nl
    <<"     - Main flow direction: "<<permVolAvgUmain<<" m²"<<nl
    <<"     - Secondary flow direction: "<<permVolAvgUsec<<" m²"<<nl
    <<"     - Tertiary flow direction: "<<permVolAvgUtert<<" m²"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;

// Update meshVol and fvc with current ROI values
meshVol = roiVol;
fvc = (1 - (meshVol / domainVolume)) * 100;

//Flow rate and permeability calculation with flow rate through outlet
int indexOutlet = mesh.boundaryMesh().findPatchID("outlet");
Info<<"The volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<sum(phi.boundaryField()[indexOutlet])<<" m³/s"<<nl<<endl;

double permFlowRate = -((sum(phi.boundaryField()[indexOutlet])/flowCrossArea)*nu*density*flowLength)/(pOut-pIn);
Info<<"The permeability calculated with the volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<permFlowRate<<" m²"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;
