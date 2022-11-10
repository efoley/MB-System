#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <proj.h>
#include "vtkDataSet.h"
#include "vtkErrorCode.h"
#include "vtkSetGet.h"
#include "vtkCallbackCommand.h"
#include "vtkDelaunay2D.h"
#include "vtk_netcdf.h"
#include "TopoGridReader.h"
#include "GmtGridData.h"
#include "SwathGridData.h"


#define NO_Z_VALUE 0

#define VTK_CREATE(type, name) vtkSmartPointer<type> name = vtkSmartPointer<type>::New()

#define UTM_X_NAME "Easting (meters)"
#define UTM_Y_NAME "Northing (meters)"

#define GMT_EXTENSION ".grd"
#define SWATH_EXTENSION_PREFIX ".mb"


using namespace mb_system;

void AprojTest(char *msg) {

  std::cout << "AprojTest(): " << msg << std::endl;

  const int BSIZE = 1024;
  double xMin = 0.;
  
  // Get UTM zone of grid's W edge
  int utmZone = ((xMin + 180)/6 + 0.5);

  std::cerr << "UTM zone: " << utmZone << std::endl;
  
  PJ_INFO projInfo = proj_info();
  std::cerr << "proj release: " << projInfo.release << std::endl;
  
  PJ_CONTEXT *projContext = ::proj_context_create();
  if (projContext) {
    std::cerr << "Created projContext OK" << std::endl;
  }
  else {
    std::cerr << "Error creating projContext OK" << std::endl;
  }

  const char *srcCRS = "EPSG:4326";
  char targCRS[64];
  sprintf(targCRS, "+proj=utm +zone=%d +datum=WGS84", utmZone); 
  PJ *proj = ::proj_create_crs_to_crs (projContext,
                                       srcCRS,
                                       targCRS,
                                       nullptr);
  bool error = false;
  if (!proj) {
    std::cerr << "failed to create proj" << std::endl;
    exit(1);
  }
  else {
    std::cerr << "created proj OK" << std::endl;    
  }

  char buffer[BSIZE];
  int const pid = getpid();
  snprintf(buffer, BSIZE, "/proc/%d/maps", pid);
  FILE * const maps = fopen(buffer, "r");
  while (fgets(buffer, BSIZE, maps) != NULL) {
    unsigned long from, to;
    int const r = sscanf(buffer, "%lx-%lx", &from, &to);
    if (r != 2) {
      puts("!");
      continue;
    }
    void *fptr = (void *)(&proj_create_crs_to_crs);
        
    if ((from <= (uintptr_t)fptr) &&
        ((uintptr_t)fptr < to)) {
      char const * name = strchr(buffer, '/');
      if (name) {
        printf("using %s", name);
      } else {
        puts("?");
      }
    }
  }
  if (error) {
    exit(-1);
  }
}


TopoGridReader::TopoGridReader() :
  fileName_(nullptr),
  gridType_(TopoGridType::Unknown),
  xUnits_(nullptr), yUnits_(nullptr), zUnits_(nullptr)
{
  
  gridPoints_ = vtkSmartPointer<vtkPoints>::New();
  gridPoints_->SetDataTypeToDouble();
  gridPolygons_ = vtkSmartPointer<vtkCellArray>::New();  

  this->SetNumberOfInputPorts(0);

  VTK_CREATE(vtkCallbackCommand, cbc);
  cbc->SetCallback(&TopoGridReader::SelectionModifiedCallback); // what's this?
  cbc->SetClientData(this);  // what's this?

  AprojTest((char *)"from TopoGridReader constructor");
}

TopoGridReader::~TopoGridReader() {
}


int TopoGridReader::RequestData(vtkInformation* request,
			       vtkInformationVector** inputVector,
			       vtkInformationVector* outputVector) {

  std::cerr << "TopoGridReader::RequestData()" << std::endl;
  (void)request;     // Unused parameter
  (void)inputVector; // Unused parameter
  
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // Check for valid output vtkDataSet
  vtkDataSet* output = vtkDataSet::GetData(outInfo);
  if (!output)
  {
    vtkErrorMacro(<< "Bad output type.");
    SetErrorCode(vtkErrorCode::UserError);
    return 0;
  }  

  vtkPolyData* polyOutput = vtkPolyData::SafeDownCast(output);
  if (!polyOutput) {
    SetErrorCode(vtkErrorCode::UserError);
    vtkErrorMacro(<< "Output is not vtkPolyData");
    return 0;
  }
  
  // Read grid file
  std::cerr << "TopoGridReader::RequestData() - readGridfile()" << std::endl;
  grid_ = readGridfile(fileName_);
  
  if (!grid_) {
    std::cerr << "error while reading " << fileName_ << std::endl;
    SetErrorCode(vtkErrorCode::CannotOpenFileError);
    return 0;
  }


  if (xUnits_) {
    free((void *)xUnits_);
  }
  if (yUnits_) {
    free((void *)yUnits_);
  }
  if (zUnits_) {
    free((void *)zUnits_);
  }

  grid_->units(&xUnits_, &yUnits_, &zUnits_);
  
  PJ *toUTM = nullptr;
  PJ_CONTEXT *projContext = nullptr;
  bool convertToUTM = !fileInUTM();
  
  if (gridType_ == TopoGridType::SwathGrid) {
    // Data already in UTM
    convertToUTM = false;
  }
  
  double xMin, xMax, yMin, yMax, zMin, zMax;
  grid_->bounds(&xMin, &xMax, &yMin, &yMax, &zMin, &zMax);
  std::cerr << "xmin=" << xMin << ", xMax=" << xMax
            << ", yMin=" << yMin << ", yMax=" << yMax << std::endl;

  // If x and y are not in UTM, must convert them to UTM
  if (convertToUTM) {
    std::cout << "file projection is not UTM" << std::endl;
    // Set up PROJ software first...


    // Get UTM zone of grid's W edge
    int utmZone = ((xMin + 180)/6 + 0.5);

    PJ_CONTEXT *projContext = proj_context_create();
    const char *srcCRS = "EPSG:4326";
    char targCRS[64];
    sprintf(targCRS, "+proj=utm +zone=%d +datum=WGS84", utmZone); 
    toUTM = proj_create_crs_to_crs (projContext,
                                    srcCRS,
                                    targCRS,
                                    nullptr);
    if (!toUTM) {
      vtkErrorMacro(<< "failed to create toUTM");
      SetErrorCode(vtkErrorCode::UserError);
      return 0;      
    }

    // Ensure proper coordinate order
    PJ* normT = proj_normalize_for_visualization(projContext,
                                                 toUTM);
    proj_destroy(toUTM);
    
    if (!normT) {
      vtkErrorMacro(<< "failed to create norm proj");
      SetErrorCode(vtkErrorCode::UserError);
      return 0;      
    }

    toUTM = normT;
  }
  else {
    std::cout << "Already in UTM" << std::endl;
  }

  
  unsigned nRows = grid_->nRows();
  unsigned nColumns = grid_->nColumns();
  std::cerr << "nRows=" << nRows << ", nColumns=" << nColumns << std::endl;
  
  // Pre-allocate points memory
  if (!gridPoints_->Allocate(nRows * nColumns)) {
    std::cerr << "failed to allocate "
	      <<  nRows * nColumns << " points"
	      << std::endl;
  }

  // Reset/clear points
  gridPoints_->Reset();

  unsigned row;
  unsigned col;

  if (convertToUTM) {
    std::cerr << "WARNING: conversion to UTM not yet implemented!" << std::endl;
  }
  
  // Load points read from grid file
  double x, y, z;
  double lat, lon;
  int nValidPoints = 0;
  bool gridMissingZValues = false;
  std::cerr << "TopoGridReader::RequestData() - load points" << std::endl;    
  for (row = 0; row < nRows; row++) {
    for (col = 0; col < nColumns; col++) {

      if (!convertToUTM) {
        // Already in UTM
        grid_->data(row, col, &y, &x, &z);
        // Don't insert NaN-valued data
        if (std::isnan(z)) {
          z = NO_Z_VALUE;
          gridMissingZValues = true;
        }
        vtkIdType id = gridPoints_->InsertNextPoint(x, y, z);
        nValidPoints++;
      }
      else {
        grid_->data(row, col, &lat, &lon, &z);
        if (std::isnan(z)) {
          z = NO_Z_VALUE;
          gridMissingZValues = true;          
        }
        
        // Convert lat/lon to UTM
        PJ_COORD lonLat = proj_coord(lon, lat,
                                     0, 0);

        PJ_COORD utm = proj_trans(toUTM, PJ_FWD, lonLat);
        //        std::cerr << "utm: " << utm.enu.e << ", " << utm.enu.n << std::endl;
        vtkIdType id = gridPoints_->InsertNextPoint(utm.enu.e,
                                                    utm.enu.n,
                                                    z);
      }
    }
  }

  // Clean up proj UTM conversion stuff
  if (convertToUTM) {
    proj_destroy(toUTM);
    proj_context_destroy(projContext);
  }

  double bounds[6];
  gridPoints_->GetBounds(bounds);
  std::cerr << "gridPoints_: xMin=" << bounds[0] << ", xMax=" << bounds[1] <<
    ", yMin=" << bounds[2] << ", yMax=" << bounds[3] <<
    ", zMin=" << bounds[4] << ", zMax=" << bounds[5] << std::endl;
  
  // Set Delaunay triangle vertices
  /* ***
       // Add grid points to a polydata object
      vtkNew<vtkPolyData> polyData;
      polyData->SetPoints(gridPoints_);
      vtkNew<vtkDelaunay2D> delaunay;
      delaunay_->SetInputData(polyData);

      *** */
     if (!gridPolygons_->Allocate(nRows * nColumns * 2)) {
    std::cerr << "failed to allocat "
	      <<  nRows * nColumns *2 << " polygons"
	      << std::endl;
  }

  vtkIdType triangleVertexId[3];
  // Triangles must stay within row and column bounds
  for (unsigned row = 0; row < nRows-1; row++) {
    for (unsigned col = 0; col < nColumns-1; col++) {

      // First triangle
      triangleVertexId[0] = gridOffset(nRows, nColumns, row, col);
      triangleVertexId[1] = gridOffset(nRows, nColumns, row, col+1);
      triangleVertexId[2] = gridOffset(nRows, nColumns, row+1, col+1);
      // If any vertices refer to point with no z-data, then do not insert into gridPolygons
      if (!gridMissingZValues || !triangleMissingZValues(triangleVertexId)) {
        gridPolygons_->InsertNextCell(3, triangleVertexId);
      }

      
      // Second triangle
      triangleVertexId[0] = gridOffset(nRows, nColumns, row, col);
      triangleVertexId[1] = gridOffset(nRows, nColumns, row+1, col+1);
      triangleVertexId[2] = gridOffset(nRows, nColumns, row+1, col);
      // If any vertices refer to point with no z-data, then do not insert into gridPolygons
      if (!gridMissingZValues || !triangleMissingZValues(triangleVertexId)) {
        gridPolygons_->InsertNextCell(3, triangleVertexId);
      }      
    }
  }


  // Save to object's points and polygons
  polyOutput->SetPoints(gridPoints_);
  polyOutput->SetPolys(gridPolygons_);
  // polyOutput->SetPolys(delaunay->GetOutput());
  
  std::cerr << "TopoGridReader::RequestData() - done" << std::endl;

  return 1;
}


void TopoGridReader::SetFileName(const char *fileName) {
  fprintf(stderr, "In TopoGridReader::SetFileName()\n");
  if (fileName_) {
    fprintf(stderr, "TopoGridReader::SetFileName: free fileName_\n");
    free((void *)fileName_);
  }
  fprintf(stderr, "TopoGridReader::SetFileName: call strdup()\n");
  fileName_ = strdup((char *)fileName);
  
}


void TopoGridReader::SelectionModifiedCallback(vtkObject*, unsigned long,
                                              void* clientdata, void*)
{
  static_cast<TopoGridReader*>(clientdata)->Modified();
}


void TopoGridReader::gridBounds(double *xMin, double *xMax,
                                 double *yMin, double *yMax,
                                 double *zMin, double *zMax) {

  // grid_->bounds(xMin, xMax, yMin, yMax, zMin, zMax);
  double bounds[6];
  gridPoints_->GetBounds(bounds);
  *xMin = bounds[0];
  *xMax = bounds[1];
  *yMin = bounds[2];
  *yMax = bounds[3];
  *zMin = bounds[4];
  *zMax = bounds[5];  
}


vtkIdType TopoGridReader::gridOffset(unsigned nRows, unsigned nCols,
                                    unsigned row, unsigned col) {
  if (row >= nRows || col >= nCols) { 
    // Out of bounds
    fprintf(stderr,
            "getGridOffset(): out-of-bounds: row=%d, nRows=%d, col=%d, nCols=%d\n",
	    row, nRows, col, nCols);
  }

  return  (col + row * nCols);
}



float TopoGridReader::zScaleLatLon(float latRange, float lonRange,
                                  float zRange) {

  float avgLatLonRange = (latRange + lonRange) / 2.;

  return avgLatLonRange / zRange;
}



bool TopoGridReader::fileInUTM() {
  if (!strcmp(xUnits_, UTM_X_NAME) &&
      !strcmp(yUnits_, UTM_Y_NAME)) {
    return true;
  }
  return false;
}


TopoGridData *TopoGridReader::readGridfile(char *filename) {

  TopoGridData *grid = nullptr;
  
  switch (gridType_) {
  case TopoGridType::GMTGrid:
    grid = new GmtGridData();
    break;

  case TopoGridType::SwathGrid:
    grid = new SwathGridData();
    break;
    
  default:
    // Unknown grid type
    std::cerr << filename << ": uhandled grid type: " << gridType_
              << std::endl;    
    return nullptr;
  }

  if (!grid->readDatafile(filename)) {
    std::cerr << "Error reading file " << filename << std::endl;
    return nullptr;
  }

  // Set grid parameters based on data just read from file
  grid->setParameters();
  
  return grid;
}


TopoGridType TopoGridReader::getGridType(const char *filename) {

  // Get file extension
  char *extension = strrchr((char *)filename, '.');
  if (!extension) {
    return TopoGridType::Unknown;
  }
  
  if (!strcmp(extension, GMT_EXTENSION)) {
    return TopoGridType::GMTGrid;
  }
  else if (!strncmp(extension, SWATH_EXTENSION_PREFIX,
                    strlen(SWATH_EXTENSION_PREFIX))) {
    return TopoGridType::SwathGrid;
  }

  return TopoGridType::Unknown;

}


bool TopoGridReader::triangleMissingZValues(vtkIdType *vertices) {

  double *point;
  for (int v = 0; v < 3; v++) {
    point = gridPoints_->GetPoint(vertices[v]);
    if (point[2] == NO_Z_VALUE) {
      return true;
    }
  }

  // Not missing z-values
  return false;
}
