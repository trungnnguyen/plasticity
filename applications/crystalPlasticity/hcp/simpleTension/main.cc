//tension BVP
//general headers
#include <fstream>
#include <sstream>
using namespace std;

#define feOrder   1
#define quadOrder 2
#define meshRefineFactor 0
#define writeOutput true
#define linearSolverType PETScWrappers::SolverCG
#define totalNumIncrements 1000
#define maxLinearSolverIterations 5000
#define relLinearSolverTolerance  1.0e-18
#define maxNonLinearIterations 6
#define absNonLinearTolerance 1.0e-18
#define relNonLinearTolerance 1.0e-13
#define stopOnConvergenceFailure false

//dealIIheaders
#include "../../../../src/materialModels/crystalPlasticity/hcp/crystalPlasticityHCP.cc"

//overload mesh() method to generate the required polycrystal geometry
template <int dim>
void crystalPlasticity<dim>::mesh(){
    //creating mesh
    this->pcout << "generating problem mesh\n";
    double spanX=1.0; //Span along x-axis
    double spanY=1.0; //Span along y-axis
    double spanZ=1.0; //Span along z-axis
    //std::vector<unsigned int> rep (dim, 4); rep[0]=39; rep[1]=56; rep[2]=59;
    //GridGenerator::subdivided_hyper_rectangle (this->triangulation,rep, Point<dim>(), Point<dim>(spanX,spanY,spanZ));
    GridGenerator::hyper_rectangle (this->triangulation, Point<dim>(), Point<dim>(spanX,spanY,spanZ));
    this->triangulation.refine_global (meshRefineFactor);
}

//Mark boundaries for applying Dirichlet BC's
template <int dim>
void crystalPlasticity<dim>::markBoundaries(){
    typename DoFHandler<dim>::active_cell_iterator
    cell = this->dofHandler.begin_active(),
    endc = this->dofHandler.end();
    
    //All boundaries are by marked with flag '0' by default.
    //To pick specific boundaries, one needs to mark them
    //with integer flags and use those flags in apply_dirichlet_conditons()
    for (;cell!=endc; ++cell){
        if (cell->is_locally_owned()){
            for (unsigned int f=0; f<GeometryInfo<dim>::faces_per_cell; ++f){
                if (cell->face(f)->at_boundary()){
                    const Point<dim> face_center = cell->face(f)->center();
                    if (face_center[0] == 0.0){
                        cell->face(f)->set_boundary_indicator (1); //back boundary
                    }
                    else if(face_center[0] == 1.0){
                        cell->face(f)->set_boundary_indicator (2); //front boundary
                    }
                    else if(face_center[1] == 0.0){
                        cell->face(f)->set_boundary_indicator (3); //left boundary
                    }
                    else if(face_center[2] == 0.0){
                        cell->face(f)->set_boundary_indicator (4); //bottom boundary
                    }
                }
            }
        }
    }
}


//Class to set Dirichlet BC values
template <int dim>
class BCFunction : public Function<dim>{
public:
    BCFunction(): Function<dim> (dim){}
    void vector_value (const Point<dim>   &p, Vector<double>   &values) const{
        Assert (values.size() == dim, ExcDimensionMismatch (values.size(), dim));
        values[0]=0.0001; // displacement along X-Direction
    }
};

//Apply Dirchlet BCs for simple tension BVP
template <int dim>
void crystalPlasticity<dim>::applyDirichletBCs(){
    this->constraints.clear ();
    this->constraints.reinit (this->locally_relevant_dofs);
    DoFTools::make_hanging_node_constraints (this->dofHandler, this->constraints);
    std::vector<bool> mechanicsBoundary_Z1 (dim, false); mechanicsBoundary_Z1[0]=true;
    std::vector<bool> mechanicsBoundary_Z2 (dim, false); mechanicsBoundary_Z2[0]=true;
    std::vector<bool> mechanicsBoundary_Z3 (dim, false); mechanicsBoundary_Z3[1]=true;
    std::vector<bool> mechanicsBoundary_Z4 (dim, false); mechanicsBoundary_Z4[2]=true;
    //u1 applied on X1=1
    if (this->currentIteration==0) {
        VectorTools:: interpolate_boundary_values (this->dofHandler,
                                                   2,
                                                   BCFunction<dim>(),
                                                   this->constraints,
                                                   mechanicsBoundary_Z2);
    }
    else {
        VectorTools:: interpolate_boundary_values (this->dofHandler,
                                                   2,
                                                   ZeroFunction<dim>(dim),
                                                   this->constraints,
                                                   mechanicsBoundary_Z2);
    }
    //u1=0 on X1=0
    VectorTools:: interpolate_boundary_values (this->dofHandler,
                                               1,
                                               ZeroFunction<dim>(dim),
                                               this->constraints,
                                               mechanicsBoundary_Z1);
    //u2=0 on X2=0
    VectorTools:: interpolate_boundary_values (this->dofHandler,
                                               3,
                                               ZeroFunction<dim>(dim),
                                               this->constraints,
                                               mechanicsBoundary_Z3);
    //u3=0 on X3=0
    VectorTools:: interpolate_boundary_values (this->dofHandler,
                                               4,
                                               ZeroFunction<dim>(dim),
                                               this->constraints,
                                               mechanicsBoundary_Z4);
    
    this->constraints.close ();
}

//main
int main (int argc, char **argv)
{
    Utilities::System::MPI_InitFinalize mpi_initialization(argc, argv, 1);
    try
    {
        deallog.depth_console(0);
        crystalPlasticity<3> problem;
        
        FullMatrix<double> m_alpha,n_alpha; // Slip directions and Slip Normals
        const unsigned int n_slip_systems=24; //No. of slip systems
        
        n_alpha.reinit(n_slip_systems,3);
        m_alpha.reinit(n_slip_systems,3);
        string line;
        ifstream myfile;
        
        // Reading slip Normals
        myfile.open("slipNormals.txt");
        int i=0; int j=0;
        if (myfile.is_open()){
            for (unsigned int k=0; k<3*n_slip_systems; k++){
                if(j==2){
                    getline (myfile,line,'\n');
                    
                }
                else
                    getline (myfile,line,'\t');
                n_alpha[i][j] = atof(line.c_str());
                std::cout <<n_alpha[i][j]<<"\n";
                j++;
                if(j==3){
                    i=i+1; j=0;
                }
                
            }
            myfile.close();
        }
        else cout << "Unable to open slipNormals file";
        myfile.clear() ;
        
        // Reading slip Normals
        myfile.open("slipDirections.txt");
        i=0;
        j=0;
        if (myfile.is_open()){
            for (unsigned int k=0; k<3*n_slip_systems; k++){
                if(j==2){
                    getline (myfile,line,'\n');
                    
                }
                else
                    getline (myfile,line,'\t');
                m_alpha[i][j] = atof(line.c_str());
                std::cout <<m_alpha[i][j]<<"\n";
                j++;
                if(j==3){
                    i=i+1; j=0;
                }
                
            }
            myfile.close();
        }
        else cout << "Unable to open slipDirections file";
        myfile.clear() ;
        
        problem.properties.n_slip_systems=n_slip_systems;
	
Vector<double> parameters;
	parameters.reinit(20);
	myfile.open("parameters.txt");
	if (myfile.is_open())
	{     
		for (unsigned int k=0; k<20; k++)
		{	     

			getline (myfile,line,'\n');
			parameters(k) = atof(line.c_str()); 
			//cout<<line.c_str()<<'\n';
		}
		myfile.close();
	}

		 //Latent Hardening Ratio
   	 problem.properties.q1=1.4;
        problem.properties.q2=1.0;
        //Slip Hardening Parameters
        problem.properties.a1=parameters(5);
        problem.properties.a2=parameters(6);
        problem.properties.a3=parameters(7);
        problem.properties.a4=parameters(8);
        problem.properties.a5=parameters(9);
        
        problem.properties.h01=parameters(10);
        problem.properties.h02=parameters(11);
        problem.properties.h03=parameters(12);
        problem.properties.h04=parameters(13);
        problem.properties.h05=parameters(14);
        
        problem.properties.s_s1=parameters(15);
        problem.properties.s_s2=parameters(16);
        problem.properties.s_s3=parameters(17);
        problem.properties.s_s4=parameters(18);
        problem.properties.s_s5=parameters(19);
        
        //Initial slip deformation resistance
        problem.properties.s01=parameters(0);
        problem.properties.s02=parameters(1);
        problem.properties.s03=parameters(2);
        problem.properties.s04=parameters(3);
        problem.properties.s05=parameters(4);
        
        //Elastic Parameters
        problem.properties.C11=59.3e3;
        problem.properties.C12=25.7e3;
        problem.properties.C13=21.4e3;
        problem.properties.C33=61.5e3;
        problem.properties.C44=16.4e3;
        problem.properties.m_alpha=m_alpha;
        problem.properties.n_alpha=n_alpha;
        
        //reading materials atlas files
        unsigned int numPts[3]={4, 4, 3}; // No. of voxels in x,y and z directions
        double spanX1=1.0; //Span along x-axis
        double spanY1=1.0; //Span along y-axis
        double spanZ1=1.0; //Span along z-axis
        
        double stencil[3]={spanX1/(numPts[0]-1), spanY1/(numPts[1]-1), spanZ1/(numPts[2]-1)}; // Dimensions of voxel
        problem.orientations.loadOrientations("grainID.txt",
                                              12,
                                              "orientations.txt",
                                              numPts,
                                              stencil);
        problem.orientations.loadOrientationVector("orientations.txt");
        problem.run ();
    }
    catch (std::exception &exc)
    {
        std::cerr << std::endl << std::endl
        << "----------------------------------------------------"
        << std::endl;
        std::cerr << "Exception on processing: " << std::endl
        << exc.what() << std::endl
        << "Aborting!" << std::endl
        << "----------------------------------------------------"
        << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << std::endl << std::endl
        << "----------------------------------------------------"
        << std::endl;
        std::cerr << "Unknown exception!" << std::endl
        << "Aborting!" << std::endl
        << "----------------------------------------------------"
        << std::endl;
        return 1;
    }
    
    return 0;
}