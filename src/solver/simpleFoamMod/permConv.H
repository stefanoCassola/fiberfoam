//Add current permeability value to vector permsVec which is initialized in simpleFOAM_mod.C

/*simple.read();

Info<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
//Info<<"++++++++"<<permeabilityDict.getOrDefault("convWindow",10)<<"+++++++++++++++++++++++"<<endl;
Info<<"++++++++"<<convPermeability_<<"+++++++++++++++++++++++"<<endl;
Info<<"+++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
*/


permsVelVec.push_back(permVolAvgUmain);
//permsFlowVec.push_back(permFlowRate);     //not needed for now
iterations.push_back(stod(runTime.timeName()));

//*************************************Display options for debugging**********************
/*
// display the elements of the vector permsVelVec which contains the permeabilities calculated with the volume averaged velocity
Info<<nl<< "Vector permsVelVec Elements: ";
for(double permVel : permsVelVec) {
    Info<< permVel <<", ";
}

// display the elements of the vector permsFlowVec which contains the permeabilities calculated with the flow rate through the outlet
Info<<nl<<nl<< "Vector permsFlowVec Elements: ";
for(double permFlow : permsFlowVec) {
    Info<< permFlow <<", ";
}

// display the elements of the vector iterations
Info<<nl<<nl<< "Vector iterations Elements: ";
for(double it : iterations) {
    Info<< it <<", ";
}
*/
//************************************************************************************


//Polynomial (linear) fit permeability values
if (readBool(fvSolution.subDict("SIMPLE").subDict("permeabilityControl").lookup("convPermeability")) == 1) {
    const int window = readScalar(fvSolution.subDict("SIMPLE").subDict("permeabilityControl").lookup("convWindow"));        //Number of permeability values for regression
    const int order = 1;                                                    //Order of regression: order = 1 --> linear regression; order > 1 --> polynomial regression
    if (stoi(runTime.timeName())>window) {                            //Start regression only when enough permeability values are available

        //create subvectors according to window size
        std::vector<double>   subIterations(&iterations[iterations.size()-window],&iterations[iterations.size()]);
        std::vector<double>   subPermsVelVec(&permsVelVec[permsVelVec.size()-window],&permsVelVec[permsVelVec.size()]);

        //Normalize vectors for regression so that the slope is independed from the magnitude of permeability (value between 0 and 1)
        double maxIt = *max_element(subIterations.begin(), subIterations.end());
        double maxPerm = *max_element(permsVelVec.begin(), permsVelVec.end());

        DivideVectorByScalar(subIterations, maxIt);
        DivideVectorByScalar(subPermsVelVec, maxPerm);

        //Fitting to data stored in subvectors
        std::vector<double> coeffs;
        polyfit(subIterations,subPermsVelVec,coeffs,order);

        //Calculated predicted permeability using window
        scalar predPerm = ((*max_element(subIterations.begin(), subIterations.end())+1) * coeffs[1] + coeffs[0])*maxPerm;

        //display coeffs of linear regression
        Info<< "Normalized slope of linear regression: "<<coeffs[1]<<nl<<endl;
        //Info<< "Predicted permeability: "<<coeffs[0]*maxPerm<<" m2"<<nl<<endl;
        Info<< "Predicted permeability: "<<predPerm<<" m2"<<nl<<endl;

        //Calculate Error predicted Permeability vs. current calculated permeability
        scalar errPerm = 1 - predPerm/permVolAvgUmain;

        Info<<"Error predicted vs. calculated permeability: "<<mag(errPerm)<<nl<<endl;

        //Check if normalized slope is lower as the value defined in controlDict
        scalar convSlope = (readScalar(fvSolution.subDict("SIMPLE").subDict("permeabilityControl").lookup("convSlope")));		//read in from dictionaty controlDict
        scalar errorBound = readScalar(fvSolution.subDict("SIMPLE").subDict("permeabilityControl").lookup("errorBound"));
        if (coeffs[1]<convSlope && mag(errPerm)<errorBound){
            Info<<"+++++++++Convergence is reached!+++++++++"<<nl<<endl;
            #include "writeFile.H"
            runTime.writeAndEnd();
            runTime.printExecutionTime(Info);
            Info<< "End\n" << endl;
            return 0;
        }


        //*****************Unnecesseary code block only for higher order regression (which is not needed)**********************
        /*
        //for(double coeff : coeffs) {
        //Info<< coeff <<", ";
        //}

        //printing fitted values
        std::vector<double> fitted_perms{0};
        Info<<nl<< "Printing fitted values" <<nl<<endl;
        for(int p = 0; p < subIterations.size(); ++ p)
        {
            double pfitted = coeffs[0] + coeffs[1]*iterations.at(p) + coeffs[2]*(pow(iterations.at(p), 2)) +coeffs[3]*(pow(iterations.at(p), 3)) ;
            fitted_perms.push_back(pfitted);
            Info<< pfitted<<", ";
            myFile << pfitted<<";";
        }

        double forecastWindow = 10;
        double pPred = coeffs[0] + coeffs[1]*(iterations.size()+forecastWindow) + coeffs[2]*(pow((iterations.size()+forecastWindow), 2)) +coeffs[3]*(pow((iterations.size()+forecastWindow), 3)) ;

        //double slope = (permsVelVec[permsVelVec.size()-2]-permsVelVec[permsVelVec.size()-1])/(iterations[iterations.size()-2]-iterations[iterations.size()-1])

        Info<<nl<<"Predicted permeability: "<<pPred<<nl<<endl;
        */
    //***********************************************************************************************************
    }
}
Info<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
Info<<"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"<<endl;
Info<<nl<<"-----------------Next time step-----------------"<<nl<<endl;


