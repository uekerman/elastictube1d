#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include "precice/SolverInterface.hpp"
#include "fluid_nl.h"
#include "../mapping/NearestNeighborMapping.hpp"
#include "../mapping/LinearInterpolationMapping.hpp"

using std::cout;
using std::endl;

using namespace precice;
using namespace precice::constants;

void printData(
    const std::vector<double>& data)
{
  std::cout << "Received data = " << data[0];
  for (size_t i = 1; i < data.size(); i++) {
    std::cout << ", " << data[i];
  }
  std::cout << std::endl;
}


int main(
    int argc, char **argv)
{
  cout << "Starting Fluid Solver..." << endl;

  int isMultilevelApproach = atoi(argv[argc-1]);
  int N = 0, N_SM = 0;
  double tau = 0.0, kappa = 0.0;

  if(isMultilevelApproach)
  {
    if (argc != 7)
        {
      cout << endl;
      cout << "Usage: " << argv[0] << " configurationFileName N tau kappa" << endl;
      cout << endl;
      cout << "N:     Number of mesh elements, needs to be equal for fluid and structure solver." << endl;
      cout << "N_SM:  Number of surrogate model mesh elements, needs to be equal for fluid and structure solver."<< endl;
      cout << "tau:   Dimensionless time step size." << endl;
      cout << "kappa: Dimensionless structural stiffness." << endl;
      cout << "isMultilevelApproach: 0/1" << endl;
      return -1;
    }
    N = atoi(argv[2]);
    N_SM = atoi(argv[3]);
    tau = atof(argv[4]);
    kappa = atof(argv[5]);
    std::cout << "N: " << N <<" N (surrogate model): " << N_SM << " tau: " << tau << " kappa: " << kappa<< " multi-level: "<< isMultilevelApproach << std::endl;
  }else{
    if (argc != 6)
        {
      cout << endl;
      cout << "Usage: " << argv[0] << " configurationFileName N tau kappa" << endl;
      cout << endl;
      cout << "N:     Number of mesh elements, needs to be equal for fluid and structure solver." << endl;
      cout << "tau:   Dimensionless time step size." << endl;
      cout << "kappa: Dimensionless structural stiffness." << endl;
      cout << "isMultilevelApproach: 0/1" << endl;
      return -1;
    }
    N = atoi(argv[2]);
    tau = atof(argv[3]);
    kappa = atof(argv[4]);
    std::cout << "N: " << N << " tau: " << tau << " kappa: " << kappa << " multi-level: "<< isMultilevelApproach << std::endl;
  }

  if(isMultilevelApproach)
    std::cout<<"The solver adapter is running in multi-level coupling mode, i.e., the solver provides one or more surrogate model evaluations to enable a multi-level based coupling method."<<std::endl;

  std::string configFileName(argv[1]);


  std::string solverName = "FLUID_1D";

  cout << "Configure preCICE..." << endl;

  // Initialize the solver interface with our name, our process index (like rank) and the total number of processes.
  SolverInterface interface(solverName, 0, 1);

  // Provide the configuration file to precice. After configuration a usuable state of that SolverInterface is reached.
  // Reads the XML file and contacts the server, if used.
  interface.configure(configFileName);
  cout << "preCICE configured..." << endl;

  // init data (fine)
  int i;
  double *u, *u_n, *p, *p_n, *a, *a_n;
  int dimensions = interface.getDimensions();

  u = new double[N + 1]; // Speed
  u_n = new double[N + 1];
  p = new double[N + 1]; // Pressure
  p_n = new double[N + 1];
  a = new double[N + 1];
  a_n = new double[N + 1];

  // init data (coarse)
  double *u_coarse, *u_n_coarse, *p_coarse, *p_n_coarse, *a_coarse, *a_n_coarse;
  double *a_copy_coarse, *p_copy_coarse; // fine mesh
  // mappings
  LinearInterpolationMapping upMapping, downMapping;

  if(isMultilevelApproach)
  {
    a_copy_coarse = new double[N + 1]; // coarse displ. data on fine mesh
    p_copy_coarse = new double[N + 1]; // coarse pressure data on fine mesh

    u_coarse = new double[N_SM + 1]; // Speed
    u_n_coarse = new double[N_SM + 1];
    p_coarse = new double[N_SM + 1]; // Pressure
    p_n_coarse = new double[N_SM + 1];
    a_coarse = new double[N_SM + 1];
    a_n_coarse = new double[N_SM + 1];
  }

  //precice stuff
  int meshID = interface.getMeshID("Fluid_Nodes");
  int aID = interface.getDataID("Displacements", meshID);
  int pID = interface.getDataID("Stresses", meshID);
  int *vertexIDs;

  int aID_coarse;
  int pID_coarse;

  if(isMultilevelApproach)
  {
    aID_coarse = interface.getDataID("Coarse_Displacements", meshID);
    pID_coarse = interface.getDataID("Coarse_Stresses", meshID);
  }


  vertexIDs = new int[(N + 1)];
  double *grid;
  grid = new double[dimensions * (N + 1)];

  // fine model
  for (i = 0; i <= N; i++)
  {
    u[i] = 1.0 / (kappa * 1.0);
    u_n[i] = 1.0 / (kappa * 1.0);
    a[i] = 1.0;
    a_n[i] = 1.0;
    p[i] = 0.0;
    p_n[i] = 0.0;
    for (int dim = 0; dim < dimensions; dim++)
      grid[i * dimensions + dim] = i * (1 - dim);

    if(isMultilevelApproach)
    {
      a_copy_coarse[i] = 1.0;
      p_copy_coarse[i] = 0.0;
    }
  }

  if(isMultilevelApproach)
  {
    // surrogate model
    for (i = 0; i <= N_SM; i++)
        {
      u_coarse[i] = 1.0 / (kappa * 1.0);
      u_n_coarse[i] = 1.0 / (kappa * 1.0);
      a_coarse[i] = 1.0;
      a_n_coarse[i] = 1.0;
      p_coarse[i] = 0.0;
      p_n_coarse[i] = 0.0;
    }
  }

  int t = 0; //number of timesteps

  // initialize coupling vertices on the given mesh
  interface.setMeshVertices(meshID, N + 1, grid, vertexIDs);

  cout << "Fluid: init precice..." << endl;
  interface.initialize();

  if (interface.isActionRequired(actionWriteInitialData())) {

    interface.writeBlockScalarData(pID, N + 1, vertexIDs, p);

    if(isMultilevelApproach)
      interface.writeBlockScalarData(pID_coarse, N + 1, vertexIDs, p_copy_coarse);

    //interface.initializeData();
    interface.fulfilledAction(actionWriteInitialData());
  }

  interface.initializeData();

  if (interface.isReadDataAvailable()) {

    interface.readBlockScalarData(aID, N + 1, vertexIDs, a);

    if(isMultilevelApproach)
      interface.readBlockScalarData(aID_coarse, N + 1, vertexIDs, a_copy_coarse);
  }


  /**
   * simulation: time-stepping loop
   */
  while (interface.isCouplingOngoing()) {

    // When an implicit coupling scheme is used, checkpointing is required
    if (interface.isActionRequired(actionWriteIterationCheckpoint())) {
      interface.fulfilledAction(actionWriteIterationCheckpoint());
    }

    // surrogate model evaluation for surrogate model optimization or MM cycle
    if(interface.hasToEvaluateSurrogateModel())
    {
      std::cout<<"\n    ### evaluate coarse model of fluid solver, t="<<t<<" ###\n"<<std::endl;

/*
      std::cout<<"FluidSolver: before down mapping p_copy_coarse = [\n";
      for (int i = 0; i <= N; i++)
        std::cout<<p_copy_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
      std::cout<<"FluidSolver: before down mapping a_copy_coarse = [\n";
      for (int i = 0; i <= N; i++)
        std::cout<<a_copy_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
*/

      // map down:  fine --> coarse [displ, pressure]
      downMapping.map(N+1, N_SM+1, a_copy_coarse, a_coarse);
      downMapping.map(N+1, N_SM+1, p_copy_coarse, p_coarse);
/*
      std::cout<<"FluidSolver: after down mapping p_coarse = [\n";
      for (int i = 0; i <= N_SM; i++)
        std::cout<<p_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
      std::cout<<"FluidSolver: after down mapping a_coarse = [\n";
      for (int i = 0; i <= N_SM; i++)
        std::cout<<a_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
*/

      // ### surrogate model evaluation ###    p_old is not used for gamma = 0.0
      fluid_nl(a_coarse, a_n_coarse, u_coarse, u_n_coarse, p_coarse, p_n_coarse, p_coarse, t + 1, N_SM, kappa, tau, 0.0);

      // map up:  coarse --> fine [displ, pressure]
      upMapping.map(N_SM+1, N+1, a_coarse, a_copy_coarse);
      upMapping.map(N_SM+1, N+1, p_coarse, p_copy_coarse);

/*
      std::cout<<"N= "<<N<<std::endl;
      std::cout<<"FluidSolver: after up mapping p_copy_coarse = [\n";
      for (int i = 0; i <= N; i++)
        std::cout<<p_copy_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
      std::cout<<"FluidSolver: after up mapping a_copy_coarse = [\n";
      for (int i = 0; i <= N; i++)
        std::cout<<a_copy_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
*/
      /*
      std::cout<<"FluidSolver: write pressure data coarse, p = [\n";
      for (int i = 0; i < N_SM; i++)
        std::cout<<p_copy_coarse[i]<<", ";
      std::cout<<"\n"<<endl;
       */

      // write coarse model response to precice
      interface.writeBlockScalarData(pID_coarse, N + 1, vertexIDs, p_copy_coarse);

      // write also coarse model response on fine mesh to precice
      // this is necessary, as the pressure values for the current iterate need
      // to be propagated to the second solver.
      interface.writeBlockScalarData(pID, N + 1, vertexIDs, p_copy_coarse);
    }


    // fine model evaluation (in MM iteration cycles)
    if(interface.hasToEvaluateFineModel())
    {
      std::cout<<"\n    ### evaluate fine model of fluid solver, t="<<t<<" ###\n"<<std::endl;

      // map up:  coarse --> fine  [old velocity, old pressure, old displ]
      // synchronize the u, p, a data from the previous time step for the fine model with those from the coarse model
      if(isMultilevelApproach){
        //upMapping.map(N_SM, N, u_n_coarse, u_n);
        //upMapping.map(N_SM, N, p_n_coarse, p_n);
        //upMapping.map(N_SM, N, a_n_coarse, a_n);
        ///*
       // for (i = 0; i <= N; i++)
       // {
       //   u_n[i] = u_n_coarse[i];
       //   p_n[i] = p_n_coarse[i];
       //   a_n[i] = a_n_coarse[i];
       // }
        //*/
      }

      // ### fine model evaluation ###    p_old is not used for gamma = 0.0
      fluid_nl(a, a_n, u, u_n, p, p_n, p, t + 1, N, kappa, tau, 0.0);

      /*
      if(isMultilevelApproach){
        std::cout<<"FluidSolver: write pressure data fine, p = [\n";
        for (int i = 0; i < N; i++)
          std::cout<<p_copy_coarse[i]<<", ";
        std::cout<<"\n"<<endl;
      }else{
        std::cout<<"FluidSolver: write pressure data fine, p = [\n";
        for (int i = 0; i < N; i++)
          std::cout<<p[i]<<", ";
        std::cout<<"\n"<<endl;
      }
      */

      // write fine model response
      interface.writeBlockScalarData(pID, N + 1, vertexIDs, p);
    }

    // perform coupling using preCICE
    interface.advance(0.01);


    // read coupling data from preCICE.

    // surrogate model evaluation for surrogate model optimization or MM cycle
    if (interface.hasToEvaluateSurrogateModel()){
      if(isMultilevelApproach){
        interface.readBlockScalarData(aID_coarse, N + 1, vertexIDs, a_copy_coarse);

        /*
        std::cout<<"FluidSolver: read displ data coarse, a = [\n";
        for (int i = 0; i < N_SM; i++)
          std::cout<<a_copy_coarse[i]<<", ";
        std::cout<<"\n"<<endl;
        */
      }
    }

    // fine model evaluation (in MM iteration cycles)
    if(interface.hasToEvaluateFineModel()){
      interface.readBlockScalarData(aID, N + 1, vertexIDs, a);

      /*
      std::cout<<"FluidSolver: read displ data fine, a = [\n";
      for (int i = 0; i < N; i++)
        std::cout<<a[i]<<", ";
      std::cout<<"\n"<<endl;
      */
    }

    if (interface.isActionRequired(actionReadIterationCheckpoint())) { // i.e. not yet converged
      // cout << "Iterate" << endl;
      interface.fulfilledAction(actionReadIterationCheckpoint());
    }
    else {

      // store u, p, a values from old time step
      for (i = 0; i <= N; i++)
      {
        u_n[i] = u[i];
        p_n[i] = p[i];
        a_n[i] = a[i];
      }

      if(isMultilevelApproach)
      {
        // map down:  fine --> coarse [displ, pressure]
        // necessary as a_copy_coarse is updated, i.e., red from the coupling (preCICE)
        downMapping.map(N+1, N_SM+1, a_copy_coarse, a_coarse);

        // save values from last time step for coarse model
        for (i = 0; i <= N_SM; i++)
        {
          u_n_coarse[i] = u_coarse[i];
          p_n_coarse[i] = p_coarse[i];
          a_n_coarse[i] = a_coarse[i];
        }
        // save values from last time step for fine model

        // map up:  coarse --> fine [velocity]
        // necessary as a_copy_coarse is updated, i.e., red from the coupling (preCICE)
        upMapping.map(N_SM+1, N+1, u_coarse, u_n);
        for (i = 0; i <= N; i++)
        {
          p_n[i] = p_copy_coarse[i];
          a_n[i] = a_copy_coarse[i];
        }
      }

      cout << "\n\n ------------------------------------------------------\n "
              "Advancing in time, Fluid Solver finished timestep: "
           << t <<"\n ------------------------------------------------------"<< endl;

      // advance in time
      t++;

      /*
      for (i = 0; i <= N; i++)
      {
        if(isMultilevelApproach){
        u_n[i] = a_copy_coarse[i];
        p_n[i] = p_copy_coarse[i];
        a_n[i] = a_coarse[i];
        }else
        {
          u_n[i] = u[i];
          p_n[i] = p[i];
          a_n[i] = a[i];
        }
      }

      if(isMultilevelApproach)
      {
        for (i = 0; i <= N_SM; i++)
        {
          u_n_coarse[i] = u_coarse[i]; // mapping issues, but take care that correct data is shifted
          p_n_coarse[i] = p_copy_coarse[i];
          a_n_coarse[i] = a_copy_coarse[i];
        }
      }
      */
    }
  }

  interface.finalize();
  cout << "Exiting FluidSolver" << endl;

  return 0;
}
