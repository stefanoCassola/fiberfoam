//Declare variables needed for permeability calculation
string mainDir;
scalar mainFlowDir, secondaryFlowDir, tertiaryFlowDir;
scalar mainDirMinDim, secondaryDirMinDim, tertiaryDirMinDim, mainDirMaxDim, secondaryDirMaxDim, tertiaryDirMaxDim;
scalar flowLength, flowLengthROI, flowCrossArea;
scalar density, nu;
scalar pIn, pOut;
scalar meshVol, fvc;

cellSet selectedCellsSet(mesh, "selectedCells", mesh.nCells()/10);

if (runTime.timeName()=="1") {

	Info<<"-----------------------------------------------------------"<<endl;
	Info<<"Retrieving information for permeability calculation..."<<endl;				//Check for information need for permeability calculation during fist time step
	Info<<"-----------------------------------------------------------"<<nl<<endl;

	//Check highest velocity --> must be main flow direction
	if (U.average()[0] > U.average()[1] && U.average()[0] > U.average()[2]) {
		Info << "Mesh bounds: " << mesh.bounds() << nl;
		mainDir = "x";
		mainFlowDir = 0;
		secondaryFlowDir = 1;
		tertiaryFlowDir = 2;
		//calculate flow length and bounding box for structure without inlet/outlet
		mainDirMinDim = (readScalar(blockMeshDict.lookup("Nxmin")) + readScalar(blockMeshDict.lookup("inlet_length")) * readScalar(blockMeshDict.lookup("scale")));
		Info << "mainDirMinDim: " << mainDirMinDim << nl;
		secondaryDirMinDim = readScalar(blockMeshDict.lookup("Nymin")) * readScalar(blockMeshDict.lookup("scale"));
		Info << "secondaryDirMinDim: " << secondaryDirMinDim << nl;
		tertiaryDirMinDim = readScalar(blockMeshDict.lookup("Nzmin")) * readScalar(blockMeshDict.lookup("scale"));
		Info << "tertiaryDirMinDim: " << tertiaryDirMinDim << nl;
		mainDirMaxDim = (readScalar(blockMeshDict.lookup("Nxmax")) - readScalar(blockMeshDict.lookup("outlet_length"))) * readScalar(blockMeshDict.lookup("scale"));
		Info << "mainDirMaxDim: " << mainDirMaxDim << nl;
		secondaryDirMaxDim = readScalar(blockMeshDict.lookup("Nymax")) * readScalar(blockMeshDict.lookup("scale"));
		Info << "secondaryDirMaxDim: " << secondaryDirMaxDim << nl;
		tertiaryDirMaxDim = readScalar(blockMeshDict.lookup("Nzmax")) * readScalar(blockMeshDict.lookup("scale"));
		Info << "tertiaryDirMaxDim: " << tertiaryDirMaxDim << nl;
		// Create cell set that only contains cells belonging to the structure and to inlet and outlet
		forAll(mesh.C(), cellI)
		{
			const point& cellCenter = mesh.C()[cellI];
			if (
				cellCenter.x() >= mainDirMinDim && cellCenter.x() <= mainDirMaxDim &&
				cellCenter.y() >= secondaryDirMinDim && cellCenter.y() <= secondaryDirMaxDim &&
				cellCenter.z() >= tertiaryDirMinDim && cellCenter.z() <= tertiaryDirMaxDim
				)
		{
			selectedCellsSet.insert(cellI);
		}
		}


	};

	if (U.average()[1] > U.average()[0] && U.average()[1] > U.average()[2]) {
		mainDir = "y";
		mainFlowDir = 1;
		secondaryFlowDir = 2;
		tertiaryFlowDir = 0;
		//calculate bounding box for structure without inlet/outlet
		mainDirMinDim = (readScalar(blockMeshDict.lookup("Nymin")) + readScalar(blockMeshDict.lookup("inlet_length")) * readScalar(blockMeshDict.lookup("scale")));
		secondaryDirMinDim = readScalar(blockMeshDict.lookup("Nzmin")) * readScalar(blockMeshDict.lookup("scale"));
		tertiaryDirMinDim = readScalar(blockMeshDict.lookup("Nxmin")) * readScalar(blockMeshDict.lookup("scale"));
		mainDirMaxDim = (readScalar(blockMeshDict.lookup("Nymax")) - readScalar(blockMeshDict.lookup("outlet_length"))) * readScalar(blockMeshDict.lookup("scale"));
		secondaryDirMaxDim = readScalar(blockMeshDict.lookup("Nzmax")) * readScalar(blockMeshDict.lookup("scale"));
		tertiaryDirMaxDim = readScalar(blockMeshDict.lookup("Nxmax")) * readScalar(blockMeshDict.lookup("scale"));

		// Create cell set that only contains cells belonging to the structure and to inlet and outlet
		forAll(mesh.C(), cellI)
		{
			const point& cellCenter = mesh.C()[cellI];
			if (
				cellCenter.x() >= tertiaryDirMinDim && cellCenter.x() <= tertiaryDirMaxDim &&
				cellCenter.y() >= mainDirMinDim && cellCenter.y() <= mainDirMaxDim &&
				cellCenter.z() >= secondaryDirMinDim && cellCenter.z() <= secondaryDirMaxDim
				)
		{
			selectedCellsSet.insert(cellI);
		}
		}

	};

	if (U.average()[2] > U.average()[0] && U.average()[2] > U.average()[1]) {
		mainDir = "z";
		mainFlowDir = 2;
		secondaryFlowDir = 0;
		tertiaryFlowDir = 1;
		//calculate flow length and bounding box for structure without inlet/outlet
		mainDirMinDim = (readScalar(blockMeshDict.lookup("Nzmin")) + readScalar(blockMeshDict.lookup("inlet_length")) * readScalar(blockMeshDict.lookup("scale")));
		secondaryDirMinDim = readScalar(blockMeshDict.lookup("Nxmin")) * readScalar(blockMeshDict.lookup("scale"));
		tertiaryDirMinDim = readScalar(blockMeshDict.lookup("Nymin")) * readScalar(blockMeshDict.lookup("scale"));
		mainDirMaxDim = (readScalar(blockMeshDict.lookup("Nzmax")) - readScalar(blockMeshDict.lookup("outlet_length"))) * readScalar(blockMeshDict.lookup("scale"));
		secondaryDirMaxDim = readScalar(blockMeshDict.lookup("Nxmax")) * readScalar(blockMeshDict.lookup("scale"));
		tertiaryDirMaxDim = readScalar(blockMeshDict.lookup("Nymax")) * readScalar(blockMeshDict.lookup("scale"));

		// Create cell set that only contains cells belonging to the structure and to inlet and outlet
		forAll(mesh.C(), cellI)
		{
			const point& cellCenter = mesh.C()[cellI];
			if (
				cellCenter.x() >= secondaryDirMinDim && cellCenter.x() <= secondaryDirMaxDim&&
				cellCenter.y() >= tertiaryDirMinDim && cellCenter.y() <= tertiaryDirMaxDim &&
				cellCenter.z() >= mainDirMinDim && cellCenter.z() <= mainDirMaxDim
				)
		{
			selectedCellsSet.insert(cellI);
		}
		}
	};

	//Save information in variables

	flowLength = mesh.bounds().max()[mainFlowDir] - mesh.bounds().min()[mainFlowDir];
	flowLengthROI = mainDirMaxDim - mainDirMinDim;
	flowCrossArea = (mesh.bounds().max()[secondaryFlowDir] - mesh.bounds().min()[secondaryFlowDir]) * (mesh.bounds().max()[tertiaryFlowDir] - mesh.bounds().min()[tertiaryFlowDir]);
	density = (readScalar(transportProperties.lookup("density")));
	nu = (readScalar(transportProperties.lookup("nu")));		//read in from dictionaty transportProperties
	pIn = (readScalar(transportProperties.lookup("pIn")));		//read in from dictionaty transportProperties
	pOut = (readScalar(transportProperties.lookup("pOut")));	//read in from dictionaty transportProperties
	meshVol = sum(mesh.V()).value();
	fvc = (1- (meshVol / (flowLength*flowCrossArea))) * 100;


	//Just some output
	Info<<"The main flow direction is "<<mainDir<<"!"<<nl<<endl;
	Info<<"The maximum flow length (incl. inlet/outlet) is: "<<flowLength<<" m"<<endl;
	Info<<"The calculated flow length is: "<<flowLengthROI<<" m"<<endl;
	Info<<"The flow crosssectional area is: "<<flowCrossArea<<" m2"<<endl;
	Info<<"The fluid density is: "<<density<<" kg/m3"<<endl;
	Info<<"The kinematic viscosity of the fluid is: "<<nu<<" m2/s"<<endl;
	Info<<"The inlet pressure is: "<<pIn<<" Pa"<<endl;
	Info<<"The outlet pressure: "<<pOut<<" Pa"<<endl;
	Info<<"The fiber volume content is : "<<fvc<<" %"<<nl<<endl;

	Info<<"-----------------------------------------------------------"<<nl<<endl;
};

Info<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl
    <<"++++++++++++++++++++++++++++++++++++++++Permeability outputs++++++++++++++++++++++++++++++++++++++++++"<<nl<<endl;

// Compute the average velocity in region of interest without inlet/outlet

vector sumU;

forAllIter(cellSet, selectedCellsSet, iter)
{
	sumU += U[iter.key()];
}
vector avgUBox = sumU / selectedCellsSet.size();

Info << "Averaged velocities in x y z direction in the region of interest (witout inlet/outlet) "<<nl<<nl
	<<"     - "<<avgUBox<<" m/s"<<nl<<endl;

//Calculate and output for average velocity in complete domain including inlet/outlet
Info<<"Averaged velocities in x y z direction in the complete domain "<<nl<<nl
    <<"     - "<<U.average().value()<<" m/s"<<nl<<endl;


//Permeability with average velocity
scalar permVolAvgUmain =  -(avgUBox[mainFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
scalar permVolAvgUsec = -(avgUBox[secondaryFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
scalar permVolAvgUtert = -(avgUBox[tertiaryFlowDir]*nu*density*flowLengthROI)/(pOut-pIn);
Info<<"The permeabilities in the region of interest (without inlet/outlet) calculated with the volume averaged velocities: "<<nl<<nl
    <<"     - Main flow direction: "<<permVolAvgUmain<<" m2"<<nl
    <<"     - Secondary flow direction: "<<permVolAvgUsec<<" m2"<<nl
    <<"     - Tertiary flow direction is: "<<permVolAvgUtert<<" m2"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;

//Flow rate and permeability calculation with flow rate through outlet

// Define the x-coordinate range for the cross-section
scalar xMinOut = mainDirMaxDim - 0.1 * readScalar(blockMeshDict.lookup("scale"));  // Specify the minimum x-coordinate
scalar xMaxOut = mainDirMaxDim + 0.1 * readScalar(blockMeshDict.lookup("scale"));  // Specify the maximum x-coordinate


int indexOutlet = mesh.boundaryMesh().findPatchID("outlet");
Info<<"The volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<sum(phi.boundaryField()[indexOutlet])<<" m3/s"<<nl<<endl;;
double permFlowRate = -((sum(phi.boundaryField()[indexOutlet])/flowCrossArea)*nu*density*flowLengthROI)/(pOut-pIn);
Info<<"The permeability calculated with the volumetric flow rate through the outlet is: "<<nl<<nl
    <<"     - "<<permFlowRate<<" m2"<<nl<<endl;
Info<<"------------------------------------------------------------------------------------------------------"<<nl<<endl;


