std::ofstream outputFile("permeabilityInfo.csv");

outputFile<<"flowLength;"<<flowLength<<";m"<<nl
	<<"flowCrossArea;"<<flowCrossArea<<";m2"<<nl
	<<"density;"<<density<<";kg/m3"<<nl
	<<"nu;"<<nu<<";m2/s"<<nl
	<<"pIn;"<<pIn<<";Pa"<<nl
	<<"pOut;"<<pOut<<";Pa"<<nl
    <<"permVolAvgUmain;"<<permVolAvgUmain<<";m2"<<nl
    <<"permVolAvgUsec;"<<permVolAvgUsec<<";m2"<<nl
    <<"permVolAvgUtert;"<<permVolAvgUtert<<";m2"<<nl
    <<"permFlowRate;"<<permFlowRate<<";m2"<<nl
	<<"fvc;"<<fvc<<";%"<<nl<<endl;
outputFile.close();

//Rename file according to main flow direction
if  (mainFlowDir == 0){
rename("permeabilityInfo.csv","xPermeabilityInfo.csv");
}
if  (mainFlowDir == 1){
rename("permeabilityInfo.csv","yPermeabilityInfo.csv");
}
if  (mainFlowDir == 2){
rename("permeabilityInfo.csv","zPermeabilityInfo.csv");
}
